#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

//Logging, console
#include <esp_log.h>
#include <esp_console.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_fat.h>
#include <linenoise/linenoise.h>
//Drivers
#include <driver/uart.h>
#include <driver/gpio.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

//Check for error
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

rcl_publisher_t my_pub;
std_msgs__msg__String pub_msg;
std_msgs__msg__String sub_msg;

//The subscription callback casts the message parameter msgin
//to an equivalent type of std_msgs::msg::String in C
//and prints out the received message.
void my_subscriber_callback(const void * msgin)
{
  const std_msgs__msg__String * msg = (const std_msgs__msg__String *)msgin;
  if (msg == NULL) {
    printf("Callback: msg NULL\n");
  } else {
    printf("Callback: I heard: %s\n", msg->data.data);
  }
}

// By placing rcl_publish in a timer callback, we can get periodic publications
void my_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	//The callback gives a pointer to the associated timer
	//and the time elapsed since the previous call
	//or since the timer was created if it is the first call to the callback.
	RCLC_UNUSED(last_call_time);
	if (timer != NULL) {
		// Publish message
		RCSOFTCHECK(rcl_publish(&my_pub, &pub_msg, NULL));
		pub_msg.data.data++;
	}
}

void appMain(void * arg)
{
	// Initialize micro-ROS allocator
	rcl_allocator_t allocator = rcl_get_default_allocator();

	// Initialize support object
	rclc_support_t support;
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator)); // create init_options

	// Create node object
	rcl_node_t my_node;
	const char * node_name = "node_0";
	const char * namespace = "executor_examples";
	// Init default node
	RCCHECK(rclc_node_init_default(&my_node, node_name, namespace, &support));

	// create publisher
	const char * topic_name = "topic_0";
	const rosidl_message_type_support_t * my_type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String);
	RCCHECK(rclc_publisher_init_default(
		&my_pub,
		&my_node,
		&my_type_support,
		&topic_name));

	// Create and initialize timer object
	rcl_timer_t my_timer;
	const unsigned int timer_period = RCL_MS_TO_NS(1000); // Timer period on nanoseconds
	RCCHECK(rclc_timer_init_default(
		&my_timer,
		&support,
		timer_period,
		my_timer_callback));

	// assign message to publisher
	std_msgs__msg__String__init(&pub_msg); //Init first publisher message
	const unsigned int PUB_MSG_CAPACITY = 20;
	pub_msg.data.data = malloc(PUB_MSG_CAPACITY); //Allocate memory
	pub_msg.data.capacity = PUB_MSG_CAPACITY; //Maximum capacity
	snprintf(pub_msg.data.data, pub_msg.data.capacity, "Hello World!"); //Assign content of message
	pub_msg.data.size = strlen(pub_msg.data.data); //Length of message

	//Define subscription
	rcl_subscription_t my_sub = rcl_get_zero_initialized_subscription();
	RCCHECK(rclc_subscription_init_default(&my_sub, &my_node, my_type_support, topic_name););
	std_msgs__msg__String__init(&sub_msg); //Init global message

	// create executor
	rclc_executor_t executor;
	executor = rclc_executor_get_zero_initialized_executor();
	unsigned int num_handles = 1 + 1;
	RCCHECK(rclc_executor_init(&executor, &support.context, num_handles, &allocator));
	RCCHECK(rclc_executor_add_subscription( //Add subscription
		&executor,
		&my_sub, 
		&sub_msg, 
		&my_subscriber_callback, 
		ON_NEW_DATA)); //Selects the execution semantics of the spin-method
	RCCHECK(rclc_executor_add_timer(&executor, &my_timer));

	while(1){
		for (unsigned int i = 0; i < 10; i++) {
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1000));
		}
		usleep(100000);
	}

	// clean up
	RCCHECK(rclc_executor_fini(&executor));
	RCCHECK(rcl_publisher_fini(&my_pub, &my_node));
	RCCHECK(rcl_timer_fini(&my_timer));
	RCCHECK(rcl_subscription_fini(&my_sub, &my_node));
	RCCHECK(rcl_node_fini(&my_node));
	RCCHECK(rclc_support_fini(&support));
	std_msgs__msg__String__fini(&pub_msg);
	std_msgs__msg__String__fini(&sub_msg);

  	vTaskDelete(NULL);
}
