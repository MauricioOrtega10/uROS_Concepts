#include <Arduino.h>
#include <micro_ros_platformio.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/int32.h>
#include <std_srvs/srv/set_bool.h>

rcl_publisher_t number_publisher;
rcl_publisher_t adder_publisher;
rcl_subscription_t subscriber;
std_msgs__msg__Int32 number_msg;
std_msgs__msg__Int32 adder_msg;
std_msgs__msg__Int32 sub_msg;
std_srvs__srv__SetBool_Request srv_server_req;
std_srvs__srv__SetBool_Response srv_server_res;
std_srvs__srv__SetBool_Response srv_client_res;
std_srvs__srv__SetBool_Request srv_client_req;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;
rcl_service_t server_service;
rcl_client_t server_client;
int64_t sequence_number;


bool adder_flag = false;
bool current_state_client = false;
bool previous_state_client = false;

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

#define BTN 13

// Error handle loop
void error_loop() 
{
  while(1) 
  {
    delay(100);
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) 
{
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) 
  {
    RCSOFTCHECK(rcl_publish(&number_publisher, &number_msg, NULL));
  }
}

void service_server_callback(const void * req, void * res)
{
  // Cast received message to used type
  std_srvs__srv__SetBool_Request * req_in = (std_srvs__srv__SetBool_Request *) req;
  std_srvs__srv__SetBool_Response * res_in = (std_srvs__srv__SetBool_Response *) res;

  if (req_in->data == true)
  {
    const char * response = "Adder has been activated";
    adder_flag = true;
    res_in->success = true;
    res_in->message.data = const_cast<char*>(response);
    res_in->message.size = strlen(response);
  }
  else
  {
    const char * response = "Adder has been disabled";
    adder_flag = false;
    res_in->success = false;
    res_in->message.data = const_cast<char*>(response);
    res_in->message.size = strlen(response);
  }
}

void service_client_callback(const void * res)
{
  // Cast received message to used type
  std_srvs__srv__SetBool_Response * res_in = (std_srvs__srv__SetBool_Response *) res;
  printf("Actual state: %s", res_in->success ? "true" : "false");
  printf("%s \n", res_in->message.data);
}

void subscription_callback(const void * msgin)
{
  // Cast received message to used type
  const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;
  RCSOFTCHECK(rcl_publish(&adder_publisher, &adder_msg, NULL));

  if (adder_flag)
    {
      adder_msg.data += msg->data;
    }

}
  

void setup() 
{
  
  // Configure serial transport
  Serial.begin(115200);
  set_microros_serial_transports(Serial);
  delay(2000);

  pinMode(BTN, INPUT_PULLUP);
  allocator = rcl_get_default_allocator();

  // Create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // Create node
  RCCHECK(rclc_node_init_default(&node, "u_ros_platformio_node", "", &support));

  // Create number publisher
  RCCHECK(rclc_publisher_init_default(
    &number_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "number_publisher"));

  // Create adder publisher
  RCCHECK(rclc_publisher_init_default(
    &adder_publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "adder_publisher"));

  // Create subscriber
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "number_publisher"));

  // create timer,
  const unsigned int timer_timeout = 1000;
  RCCHECK(rclc_timer_init_default(
    &timer,
    &support,
    RCL_MS_TO_NS(timer_timeout),
    timer_callback));


  //Create service server
  RCCHECK(rclc_service_init_default(
    &server_service,
    &node, 
    ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool),
    "/addnumbers"));
  
  //Create service client
  RCCHECK(rclc_client_init_default(
    &server_client,
    &node, 
    ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool),
    "/addnumbers"));

  // create executor
  RCCHECK(rclc_executor_init(&executor, &support.context, 6, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &sub_msg, subscription_callback, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_service(&executor, &server_service, &srv_server_res, &srv_server_req, service_server_callback));
  RCCHECK(rclc_executor_add_client(&executor, &server_client, &srv_client_res, service_client_callback));

  // Value initialization
  number_msg.data = 1;
  adder_msg.data = 0;
}

void loop() 
{
  delay(100);
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));

  //send a request
  if (digitalRead(BTN) == 0)
  {
    current_state_client = !current_state_client;
  }
  if (current_state_client != previous_state_client)
  {
    if (current_state_client == false)
    {
      srv_client_req.data = false;
      RCCHECK(rcl_send_request(&server_client, &srv_client_req, &sequence_number));
    }
    else
    {
      srv_client_req.data = true;
      RCCHECK(rcl_send_request(&server_client, &srv_client_req, &sequence_number));
    }
    previous_state_client = current_state_client;
  } 
}