
/*
product:	GWI-200
version:	1.2
date:		11/08/2021
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "string.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"

#include "config.h"
#include "GO.c"
#include "MOVON_MDAS9.c"
#include "MOVON_MDSM7.c"

void config() {
   PIN_FUNC_SELECT( IO_MUX_GPIO13_REG, PIN_FUNC_GPIO );
   PIN_FUNC_SELECT( IO_MUX_GPIO15_REG, PIN_FUNC_GPIO );

   gpio_set_direction( DEV_LED, GPIO_MODE_OUTPUT );
   gpio_set_level( DEV_LED, 1 );

   gpio_set_direction( FMS_LED, GPIO_MODE_OUTPUT );
   gpio_set_level( FMS_LED, 1 );
}

void init() {
   gpio_set_level( DEV_LED, 0 );
   gpio_set_level( FMS_LED, 0 );
}

void app_main() {
   config();
	GO_init();
   MDAS9_init();
	MDSM7_init();
	xTaskCreate( GO_rxTask, "GO_rxTask", 1024*2, NULL, configMAX_PRIORITIES, NULL );

	vTaskDelay( 2000 / portTICK_PERIOD_MS );
   init();
	GO_handShake();

   xTaskCreate( MDAS9_rxTask, "MDAS9_rxTask", 1024*2, NULL, configMAX_PRIORITIES, NULL );
   xTaskCreate( MDSM7_rxTask, "MDSM7_rxTask", 1024*2, NULL, configMAX_PRIORITIES, NULL );

}
