#include "pub_config.h"

void appMain(void * arg)
{
	Person* pubParam = malloc(sizeof(Person));
	if (pubParam == NULL)
        printf("ERROR!");

	init_pub(pubParam);

	while(1){
		rclc_executor_spin_some(&pubParam->executor, RCL_MS_TO_NS(100));
		usleep(100000);
	}

	// free resources
	RCCHECK(rcl_publisher_fini(&pubParam->publisher, &pubParam->node)) //Destroy publisher
	RCCHECK(rcl_node_fini(&pubParam->node)) //Destroy node
	free(pubParam);

  	vTaskDelete(NULL);
}
