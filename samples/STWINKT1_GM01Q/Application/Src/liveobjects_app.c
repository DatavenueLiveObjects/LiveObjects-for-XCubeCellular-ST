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

	while(true)
	{
		PRINT_INFO("############################ lo_synchronize ############################ \n\r")

		if(lo_is_connected(handle))
		{
			lo_synchronize(handle);

			if((xTaskGetTickCount() - offset_ms) > publish_delay)
			{
				offset_ms = HAL_GetTick();
				char buffer[256];
				//static dc_cellular_info_t  cellular_info;
				//static dc_sim_info_t cellular_sim_info;

				/* OP NAME to publish */
				//(void)dc_com_read(&dc_com_db, DC_CELLULAR_INFO, (void *)&cellular_info, sizeof(cellular_info));
				/* read SIM infos in data cache */
				//(void)dc_com_read(&dc_com_db, DC_CELLULAR_SIM_INFO, (void *)&cellular_sim_info, sizeof(cellular_sim_info));

				//sprintf(buffer, (const char*) "{\"opname\":\"sequans002\",\"imei\":\"%s\",\"imsi\":\"%s\"}", cellular_info.imei, cellular_sim_info.imsi);
				sprintf(buffer, (const char*) "{\"opname\":\"sequans002\"}");
				lo_publish_data(handle, (const char*) buffer);
			}
			osDelay(500);

		}
	}
}

/**
  * @brief  Start all threads needed to activate CellularApp features and call Cellular start
  * @param  -
  * @retval -
  */
void application_start(void)
{
	HANDLE_MQTT* handle = lo_create();

	lo_set_default_config(handle);

	lo_set_cellular_callback(handle, lo_cellular_msg);
	lo_set_cellular_ip_callback(handle, lo_cellular_ip_msg);

	lo_start(handle);
}

#endif /* USE_CELLULAR_APP == 1 */
