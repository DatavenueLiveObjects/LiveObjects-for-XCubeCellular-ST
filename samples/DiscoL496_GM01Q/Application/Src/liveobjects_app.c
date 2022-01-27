/**
* Copyright (c) Orange. All Rights Reserved.
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

/* Orange LiveObjects MQTT API
*
* Version:     1.0-SNAPSHOT
* Created:     2020-08-15 by Halim BENDIABDALLAH
*/

#include "plf_config.h"
#include "plf_cellular_app_config.h"

#if (USE_CELLULAR_APP == 1)
#include <string.h>
#include <stdbool.h>

#include "cellular_app.h"
#include "cellular_app_trace.h"
#include "cellular_service_datacache.h"

#include "rtosal.h"

#include "cellular_control_api.h"
#include "cellular_runtime_custom.h"

#include "mqtt_liveobjects.h"

/* Status of Modem */
static bool cellular_app_data_is_ready; /* false/true: data is not ready/data is ready */

bool cellular_app_is_data_ready(void)
{
  return (cellular_app_data_is_ready);
}

void lo_cellular_msg(HANDLE_MQTT* handle, uint8_t msg)
{
	PRINT_FORCE("Performance test in progress! Wait its end before to retry!")
}

void lo_command_callback(HANDLE_MQTT* handle, int64_t cid, const uint8_t* req, const json_value* arg)
{
	if(strcmp((char*)req, "cmd"))
	{

	}
	lo_publish_ack_command(handle, cid, "{\"done\":\"sendcmd\"}");
}

void lo_cellular_ip_msg(HANDLE_MQTT* handle, uint8_t msg)
{
	PRINT_FORCE("lo_cellular_ip_msg !")
}

void application_thread_main(void *p_argument)
{
	HANDLE_MQTT* handle = lo_create();

	lo_set_default_config(handle);

	lo_start_cellular(handle, 0);

	if(lo_connect_mqtt(handle) == MQTT_CLIENT_SUCCESS)
	{
		PRINT_INFO("Connected to LiveObjects");
	}
	else
	{
		PRINT_INFO("Connection to LiveObjects failed");
	}

	lo_set_command_callback(handle, lo_command_callback);
	lo_command_subscribe(handle);

	unsigned long offset_ms = xTaskGetTickCount();
	uint16_t publish_delay = 60000;
    bool first = true;

	while(true)
	{
		if(lo_is_connected(handle))
		{
			if(((xTaskGetTickCount() - offset_ms) > publish_delay) || first)
			{
				first = false;
				offset_ms = HAL_GetTick();
				char buffer[256];
				static dc_cellular_info_t  cellular_info;
				static dc_sim_info_t cellular_sim_info;

				/* OP NAME to publish */
				(void)dc_com_read(&dc_com_db, DC_CELLULAR_INFO, (void *)&cellular_info, sizeof(cellular_info));
				/* read SIM infos in data cache */
				(void)dc_com_read(&dc_com_db, DC_CELLULAR_SIM_INFO, (void *)&cellular_sim_info, sizeof(cellular_sim_info));

				sprintf(buffer, (const char*) "{\"opname\":\"sequans002\",\"imei\":\"%s\",\"imsi\":\"%s\"}", cellular_info.imei, cellular_sim_info.imsi);
				lo_publish_data(handle, (const char*) buffer);
			}
			osDelay(500);
		}
	}
}

void application_start(void)
{
	uint8_t thread_name[CELLULAR_APP_THREAD_NAME_MAX];
	uint32_t len;

	/* Thread Name Generation */
	/* Let a space to add the EchoClt number and '+1' to copy '\0' */
	len = crs_strlen((const uint8_t *)"DemoApp ");
	(void)memcpy(thread_name, "DemoApp ", CELLULAR_APP_MIN((len + 1U), CELLULAR_APP_THREAD_NAME_MAX));
	/* Thread Creation */

	/* Thread Name Insatnce Update */
	thread_name[(len - 1U)] = (uint8_t)0x30 + 1U; /* start at EchoClt1 */

	/* Thread Creation */
	osThreadId thread_id = rtosalThreadNew((const rtosal_char_t *)thread_name, (os_pthread)application_thread_main, ECHOCLIENT_THREAD_PRIO, ECHOCLIENT_THREAD_STACK_SIZE, NULL);
	/* Check Creation is ok */
	if (thread_id == NULL)
	{
	  CELLULAR_APP_ERROR((CELLULAR_APP_ERROR_ECHOCLIENT + (int32_t)ECHOCLIENT_THREAD_NUMBER + 1), ERROR_FATAL)
	}
}

#endif
