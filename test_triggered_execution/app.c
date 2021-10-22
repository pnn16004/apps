#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/string.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

//Check for error
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

rcl_publisher_t my_string_pub;
rcl_publisher_t my_int_pub;
std_msgs__msg__String string_sub_msg;
std_msgs__msg__Int32 pub_int_msg;
int pub_int_value;
std_msgs__msg__Int32 int_sub_msg;
int pub_string_value;

//Custom-defined trigger conditions. rclc_executor_trigger_any would also be possible
typedef struct
{
  rcl_timer_t * timer1;
  rcl_timer_t * timer2;
} pub_trigger_object_t;

typedef struct
{
  rcl_subscription_t * sub1;
  rcl_subscription_t * sub2;
} sub_trigger_object_t;

bool pub_trigger(rclc_executor_handle_t * handles, unsigned int size, void * obj)
{
  if (handles == NULL) {
    printf("Error in pub_trigger: 'handles' is a NULL pointer\n");
    return false;
  }
  if (obj == NULL) {
    printf("Error in pub_trigger: 'obj' is a NULL pointer\n");
    return false;
  }
  pub_trigger_object_t * comm_obj = (pub_trigger_object_t *) obj;
  bool timer1 = false;
  bool timer2 = false;
  //printf("pub_trigger ready set: ");
  for (unsigned int i = 0; i < size; i++) {
    if (handles[i].data_available) {
      void * handle_ptr = rclc_executor_handle_get_ptr(&handles[i]);
      if (handle_ptr == comm_obj->timer1) {
        timer1 = true;
      }
      if (handle_ptr == comm_obj->timer2) {
        timer2 = true;
      }
    }
  }
  return (timer1 || timer2);
}

bool sub_trigger(rclc_executor_handle_t * handles, unsigned int size, void * obj)
{
  if (handles == NULL) {
    printf("Error in sub_trigger: 'handles' is a NULL pointer\n");
    return false;
  }
  if (obj == NULL) {
    printf("Error in sub_trigger: 'obj' is a NULL pointer\n");
    return false;
  }
  sub_trigger_object_t * comm_obj = (sub_trigger_object_t *) obj;
  bool sub1 = false;
  bool sub2 = false;
  //printf("sub_trigger ready set: ");
  for (unsigned int i = 0; i < size; i++) {
    if (handles[i].data_available == true) {
      void * handle_ptr = rclc_executor_handle_get_ptr(&handles[i]);

      if (handle_ptr == comm_obj->sub1) {
        sub1 = true;
      }
      if (handle_ptr == comm_obj->sub2) {
        sub2 = true;
      }
    }
  }
  return (sub1 && sub2);
}

//The subscription callback casts the message parameter msgin
//to an equivalent type of std_msgs::msg::String in C
//and prints out the received message.
void my_string_subscriber_callback(const void * msgin)
{
  const std_msgs__msg__String * msg = (const std_msgs__msg__String *)msgin;
  if (msg == NULL) {
    printf("my_string_subscriber_callback: msgin is NULL\n");
  } else {
    printf("Callback 1: %s\n", msg->data.data);
  }
}

void my_int_subscriber_callback(const void * msgin)
{
  const std_msgs__msg__Int32 * msg = (const std_msgs__msg__Int32 *)msgin;
  if (msg == NULL) {
    printf("my_int_subscriber_callback: msgin is NULL\n");
  } else {
    printf("Callback 2: %d\n", msg->data);
  }
}

#define UNUSED(x) (void)x;

void my_timer_string_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  rcl_ret_t rc;
  UNUSED(last_call_time);
  if (timer != NULL) {
    //printf("Timer: time since last call %d\n", (int) last_call_time);
    std_msgs__msg__String pub_msg;
    std_msgs__msg__String__init(&pub_msg);
    const unsigned int PUB_MSG_CAPACITY = 20;
    pub_msg.data.data = malloc(PUB_MSG_CAPACITY);
    pub_msg.data.capacity = PUB_MSG_CAPACITY;
    snprintf(pub_msg.data.data, pub_msg.data.capacity, "Hello World!%d", pub_string_value++);
    pub_msg.data.size = strlen(pub_msg.data.data);

    RCSOFTCHECK(rcl_publish(&my_string_pub, &pub_msg, NULL));
  } else {
    printf("Error in my_timer_string_callback: timer parameter is NULL\n");
  }
}

void my_timer_int_callback(rcl_timer_t * timer, int64_t last_call_time)
{
  rcl_ret_t rc;
  UNUSED(last_call_time);
  if (timer != NULL) {
    //printf("Timer: time since last call %d\n", (int) last_call_time);
    pub_int_msg.data = pub_int_value++;
    RCSOFTCHECK(rcl_publish(&my_int_pub, &pub_int_msg, NULL));
  } else {
    printf("Error in my_timer_int_callback: timer parameter is NULL\n");
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

	// create a publisher 1
	// - topic name: 'topic_0'
	// - message type: std_msg::msg::String
	const char * topic_name = "topic_0";
	const rosidl_message_type_support_t * my_type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String);
	RCCHECK(rclc_publisher_init_default(
		&my_string_pub,
		&my_node,
		&my_type_support,
		&topic_name));

	// create timer 1
	// - publishes 'my_string_pub' every 'timer_timeout' ms
	rcl_timer_t my_string_timer;
	const unsigned int timer_period = RCL_MS_TO_NS(100); // Timer period on nanoseconds
	RCCHECK(rclc_timer_init_default(
		&my_string_timer,
		&support,
		timer_period,
		my_timer_string_callback));

	// create publisher 2
	// - topic name: 'topic_1'
	// - message type: std_msg::msg::Int
	const char * topic_name_1 = "topic_1";
	const rosidl_message_type_support_t * my_int_type_support =	ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32);
	RCCHECK(rclc_publisher_init_default(
		&my_int_pub, 
		&my_node, 
		my_int_type_support, 
		topic_name_1));

	// create timer 2
	// - publishes 'my_int_pub' every 'timer_int_timeout' ms
	rcl_timer_t my_int_timer;
	const unsigned int timer_int_period = RCL_MS_TO_NS(1000); 
	RCCHECK(rclc_timer_init_default(
		&my_int_timer, 
		&support, 
		timer_int_period, 
		my_timer_int_callback));
	
	//Initialize pub msg vars in timer callbacks
	std_msgs__msg__Int32__init(&pub_int_msg);
	pub_int_value = 0;
	pub_string_value = 0;

	// create subscription 1
	rcl_subscription_t my_string_sub = rcl_get_zero_initialized_subscription();
	rcl_subscription_options_t my_subscription_options = rcl_subscription_get_default_options();
	my_subscription_options.qos.depth = 0; // qos: last is best = register semantics
	RCCHECK(rcl_subscription_init(&my_string_sub, &my_node, my_type_support, topic_name, &my_subscription_options));
	// initialize subscription message
	std_msgs__msg__String__init(&string_sub_msg);

	// create subscription 2
	rcl_subscription_t my_int_sub = rcl_get_zero_initialized_subscription();
	RCCHECK(rclc_subscription_init_default(&my_int_sub, &my_node, my_int_type_support, topic_name_1));
	// initialize subscription message
	std_msgs__msg__Int32__init(&int_sub_msg);

	rclc_executor_t executor_pub;
	rclc_executor_t executor_sub;

	// Executor for publishing messages
	unsigned int num_handles_pub = 2;
	printf("Executor_pub: number of DDS handles: %u\n", num_handles_pub);
	executor_pub = rclc_executor_get_zero_initialized_executor();
	rclc_executor_init(&executor_pub, &support.context, num_handles_pub, &allocator);

	RCCHECK(rclc_executor_add_timer(&executor_pub, &my_string_timer));
	RCCHECK(rclc_executor_add_timer(&executor_pub, &my_int_timer));

	RCCHECK(rclc_executor_add_timer(&executor_pub, &my_string_timer));
	RCCHECK(rclc_executor_add_timer(&executor_pub, &my_int_timer));
	
	unsigned int num_handles_sub = 2;
	printf("Executor_sub: number of DDS handles: %u\n", num_handles_sub);
	executor_sub = rclc_executor_get_zero_initialized_executor();
	rclc_executor_init(&executor_sub, &support.context, num_handles_sub, &allocator);

	// add subscription to executor
	RCCHECK(rclc_executor_add_subscription(
	&executor_sub, &my_string_sub, &string_sub_msg,
	&my_string_subscriber_callback,
	ON_NEW_DATA));

	// add int subscription to executor
	RCCHECK(rclc_executor_add_subscription(
	&executor_sub, &my_int_sub, &int_sub_msg,
	&my_int_subscriber_callback,
	ON_NEW_DATA));

	rclc_executor_set_trigger(&executor_pub, rclc_executor_trigger_any, NULL);
	rclc_executor_set_trigger(&executor_sub, rclc_executor_trigger_all, NULL);

	while(1){
		for (unsigned int i = 0; i < 100; i++) {
		// timeout specified in ns                 (here: 1s)
		rclc_executor_spin_some(&executor_pub, 1000 * (1000 * 1000));
		usleep(1000); // 1ms
		rclc_executor_spin_some(&executor_sub, 1000 * (1000 * 1000));
		}
	}

	rcl_ret_t rc;
	// clean up
	rc = rclc_executor_fini(&executor_pub);
	rc += rclc_executor_fini(&executor_sub);
	rc += rcl_publisher_fini(&my_string_pub, &my_node);
	rc += rcl_publisher_fini(&my_int_pub, &my_node);
	rc += rcl_timer_fini(&my_string_timer);
	rc += rcl_timer_fini(&my_int_timer);
	rc += rcl_subscription_fini(&my_string_sub, &my_node);
	rc += rcl_subscription_fini(&my_int_sub, &my_node);
	rc += rcl_node_fini(&my_node);
	rc += rclc_support_fini(&support);

	std_msgs__msg__Int32__fini(&pub_int_msg);
	std_msgs__msg__String__fini(&string_sub_msg);
	std_msgs__msg__Int32__fini(&int_sub_msg);

	if (rc != RCL_RET_OK) {
	printf("Error while cleaning up!\n");
	return -1;
	}

  	vTaskDelete(NULL);
}
