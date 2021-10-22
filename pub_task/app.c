#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <std_msgs/msg/string.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

//Drivers
#include "driver/uart.h"
#include "driver/gpio.h"
#endif

#define ARRAY_LEN 50

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

//Definitions
#define BLINK_GPIO 2 //CONFIG_BLINK_GPIO
#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_BUTTON_PIN 0

rcl_publisher_t publisher;
std_msgs__msg__String msg;

SemaphoreHandle_t xSemaphore;
TaskHandle_t ISR = NULL;
rclc_executor_t executor;

int counter = 0;

// interrupt service routine, called when the button is pressed
void IRAM_ATTR button_isr_handler(void* arg)
{  
    xTaskResumeFromISR(ISR); //portYIELD_FROM_ISR(  );
}

// task that will react to button clicks
void button_task(void *arg)
{
    bool led_status = false;
    while(1)
    {
		xSemaphoreTakeFromISR(xSemaphore, NULL);

        vTaskSuspend(NULL);      
        led_status = !led_status;
        gpio_set_level(BLINK_GPIO, led_status);

        sprintf(msg.data.data, "Hello from ESP32 #%d", counter++);
		msg.data.size = strlen(msg.data.data);
		RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
		printf("I have publish: \"%s\"\n", msg.data.data);
        //vTaskDelete(ISR);
		
		xSemaphoreGiveFromISR(xSemaphore, NULL);
    }
}

void initialize_GPIO(void)
{
    gpio_reset_pin(BLINK_GPIO);
    //gpio_pad_select_gpio(BLINK_GPIO);
    gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    // Enable interrupt on falling (1->0) edge for button pin
    gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
    // Install the driver’s GPIO ISR handler service, which allows per-pin GPIO interrupt handlers
    // Install ISR service with default configuration
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	// Attach the interrupt service routine
    gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);
}

void appMain(void * arg)
{
	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
	//while (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK);

	// create node
	rcl_node_t node;
	RCCHECK(rclc_node_init_default(
		&node, 
		"esp32_pub", 
		"", 
		&support));

	// create publisher
	RCCHECK(rclc_publisher_init_default(
		&publisher,	
		&node, 
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), 
		"topic"));

	initialize_GPIO();

	//Create mutex to be used by ISR
	xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemaphore);
	//Create and start stats task
    xTaskCreate(button_task, "button_task", 4096, NULL, 10, &ISR);

	// create executor
	//rclc_executor_t executor;
	executor = rclc_executor_get_zero_initialized_executor();
	RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
	RCCHECK(rclc_executor_set_trigger(&executor, rclc_executor_trigger_any, &ISR));

	// Fill the array with a known sequence
	msg.data.data = (char * ) malloc(ARRAY_LEN * sizeof(char));
	msg.data.size = 0;
	msg.data.capacity = ARRAY_LEN;

	//rclc_executor_spin(&executor);
	while(1){
		rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
		usleep(100000);
	}

	// free resources
	RCCHECK(rcl_publisher_fini(&publisher, &node))
	RCCHECK(rcl_node_fini(&node))

  	vTaskDelete(button_task);
}

