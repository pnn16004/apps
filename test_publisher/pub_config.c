#include "pub_config.h"

rcl_publisher_t publisher;
std_msgs__msg__Int32 msg;

// By placing rcl_publish in a timer callback, we can get periodic publications
void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	//The callback gives a pointer to the associated timer
	//and the time elapsed since the previous call
	//or since the timer was created if it is the first call to the callback.
	RCLC_UNUSED(last_call_time);
	if (timer != NULL) {
		// Publish message
		RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
		msg.data++;
	}
}

void init_pub(Person* pubParam)
{
    // Initialize micro-ROS allocator
	rcl_allocator_t allocator = rcl_get_default_allocator();

	// Initialize support object
	rclc_support_t support;
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator)); // create init_options

	// Create node object
	rcl_node_t node;
	const char * node_name = "freertos_int32_publisher";
	const char * namespace = "";
	// Init default node
	RCCHECK(rclc_node_init_default(&node, node_name, namespace, &support));


    // create publisher
	const rosidl_message_type_support_t * type_support = ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32);
	RCCHECK(rclc_publisher_init_default(
		&publisher,
		&node,
		&type_support,
		&node_name));

    
    // Create and initialize timer object
	rcl_timer_t timer;
	const unsigned int timer_period = RCL_MS_TO_NS(1000); // Timer period on nanoseconds
	RCCHECK(rclc_timer_init_default(
		&timer,
		&support,
		timer_period,
		timer_callback));

	// create executor
	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	msg.data = 0; //Set message value


    pubParam->node = node;
    pubParam->publisher = publisher;
    pubParam->timer = timer;
    pubParam->executor = executor;
}