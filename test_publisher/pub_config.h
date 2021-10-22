#ifndef PUBCONFIG_H_
#define PUBCONFIG_H_

//Includes and macros
#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/int32.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

//Check for error
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

//Structs and variables
typedef struct Person{
    rcl_node_t node;
    rcl_publisher_t publisher;
    rcl_timer_t timer;
    rclc_executor_t executor;
} Person;

//Functions
extern void init_pub(Person* pubParam);
//

#endif /* PUBCONFIG_H_ */