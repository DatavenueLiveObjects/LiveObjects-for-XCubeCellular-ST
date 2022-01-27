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

#include "mqtt_client.h"
#include "orange_mqtt.h"
#include "mqtt_socket.h"
#include "plf_config.h"
#include "dc_common.h"

#include "error_handler.h"

#include "cellular_app.h"
#include "cellular_app_socket.h"
#include "cellular_app_trace.h"
#include "cellular_control_common.h"
#include "cellular_control_api.h"

#if (USE_MBEDTLS == 1)
#include "mbedtls_timedate.h"
#include "mbedtls_credentials.h"
#endif /* (USE_MBEDTLS == 1) */

#include "liveobjects_conf.h"

HANDLE_CELLULAR handle_cellular;

static void cellular_info_cb(ca_event_type_t event, const cellular_info_t *const p_cellular_info, void *const p_callback_ctx);
static void mqtt_cellular_ip_info_cb(ca_event_type_t event_type, const cellular_ip_info_t *const p_ip_info, void *const p_callback_ctx);

static bool mqtt_client_check_distantname(HANDLE_MQTT* handle_mqtt, const uint8_t* url);
static enum_mqtt_client_error mqtt_client_connect_mqtt(HANDLE_MQTT* handle, const uint8_t* client_id, const uint8_t* user_name, const uint8_t* password);

bool cellular_app_data_is_ready;

HANDLE_CELLULAR* create_cellular()
{
	handle_cellular.network_initialized = false;
	handle_cellular.network_is_on = false;

	/* Cellular components statical init */
	cellular_init();

	return &handle_cellular;
}

void mqtt_client_enable(HANDLE_MQTT* handle)
{
	if (cellular_info_cb_registration(cellular_info_cb, handle))
	{
		CELLULAR_APP_ERROR(CELLULAR_APP_ERROR_CELLULARAPP, ERROR_FATAL)
	}

	if (cellular_ip_info_cb_registration(mqtt_cellular_ip_info_cb, handle) != CELLULAR_SUCCESS)
	{
		CELLULAR_APP_ERROR(CELLULAR_APP_ERROR_CELLULARAPP, ERROR_FATAL)
	}

	cellular_start();
}

void cellular_info_cb(ca_event_type_t event, const cellular_info_t *const p_cellular_info, void *const p_callback_ctx)
{
	HANDLE_MQTT* handle = (HANDLE_MQTT*) p_callback_ctx;
	PRINT_INFO("############################ cellular_info_cb ############################ \n\r")
	//(void) rtosalMessageQueuePut(handle->cellular_queue, (uint32_t) EVENT_CELLULAR_INFO, (uint32_t)0U);
}

static void mqtt_cellular_ip_info_cb(ca_event_type_t event_type, const cellular_ip_info_t *const p_ip_info, void *const p_callback_ctx)
{
	HANDLE_MQTT* handle = (HANDLE_MQTT*) p_callback_ctx;

  /* Event to know Modem status ? */
  if ((event_type == CA_IP_INFO_EVENT)  && (p_ip_info != NULL))
  {
    /* If IP address is not null then it means Data is ready */
    if (p_ip_info->ip_addr.addr != 0U)
    {
      /* Data is ready */
      if (cellular_app_is_data_ready() == false)
      {
        /* Modem is ready */
    	PRINT_INFO("%s: Modem ready to transmit data", "mqttClient")
    	handle_cellular.network_is_on = true;

        handle_cellular.network_initialized = true;
        (void) rtosalMessageQueuePut(handle->cellular_queue, (uint32_t) EVENT_CELLULAR_IP_INFO, (uint32_t)0U);

    	//cellular_app_propagate_info(CA_IP_INFO_EVENT);
      }
      /* else: nothing to do because data ready status already known */
    }
    else
    {
      if (cellular_app_is_data_ready() == true)
      {
        /* Modem is not ready */
    	PRINT_INFO("%s: Modem NOT ready to transmit data!", "mqttClient")
      	handle_cellular.network_is_on = false;
      }
      /* else: nothing to do because data ready status already known */
    }

//    handle_cellular.network_initialized = true;
//    (void) rtosalMessageQueuePut(handle->cellular_queue, (uint32_t) EVENT_CELLULAR_IP_INFO, (uint32_t)0U);
  }
}

#if (USE_MBEDTLS == 1)
/**
  * @brief  initialize tls credentials
  * @param  none
  * @retval -
  */
static void  mqttclient_tlsinit(void)
{
  /* credential init */
  mbedtls_credentials_init();

  /* get time and date from network and set in to RTC */
  (void)mbedtls_timedate_set_from_network();
}
#endif /* (USE_MBEDTLS == 1) */

enum_mqtt_client_error mqtt_client_connect(HANDLE_MQTT* mqtt_handle, const uint8_t* url, uint16_t port, const uint8_t* user_name, const uint8_t* password, const uint8_t* client_id, enum_tls_type tls_type)
{
	#if (USE_MBEDTLS == 1)
		if(tls_type != TLS_NONE) mqttclient_tlsinit();
	#endif /* (USE_MBEDTLS == 1) */

	bool success = mqtt_client_check_distantname(mqtt_handle, url);
	if(!success) return MQTT_CLIENT_DISTANTNAME_UNKNOWN;

	int32_t socket = create_socket();
	if (socket < 0)
	{
		PRINT_INFO("socket create KO");
		return MQTT_CLIENT_CREATE_SOCKET_FAILED;
	}
	mqtt_handle->socket = socket;

	mqtt_handle->port = port;

	int32_t ret = connect_socket(mqtt_handle->socket, mqtt_handle->ip, mqtt_handle->port, tls_type);
	if(ret != COM_SOCKETS_ERR_OK) return CLIENT_MQTT_CONNECT_SOCKET_FAILED;

	enum_mqtt_client_error res = mqtt_client_connect_mqtt(mqtt_handle, client_id, user_name, password);
	if(res != MQTT_CLIENT_SUCCESS) return res;

	mqtt_handle->is_connected = true;

	return MQTT_CLIENT_SUCCESS;
}

enum_mqtt_client_error mqtt_client_connect_mqtt(HANDLE_MQTT* handle, const uint8_t* client_id, const uint8_t* user_name, const uint8_t* password)
{
	enum_mqtt_client_error ret = MQTT_CLIENT_SUCCESS;

	uint16_t len = create_mqtt_connect_frame(handle);
	int32_t sent = send_buffer(handle->socket, handle->buffer, len, 0);

	if(sent <= 0)
	{
		handle->is_connected = false;
		close_socket(handle->socket);
		return CLIENT_MQTT_CONNECT_FAILED;
	}

	len = read_mqtt_frame(handle, MAX_BUFFER_SIZE, CONNECT_TIMEOUT_MS);
	if (len != 4)
	{
		handle->is_connected = false;
		close_socket(handle->socket);
		return CLIENT_MQTT_CONNECT_FAILED;
	}

	if ((handle->buffer[0] != (MQTT_CTRL_CONNECTACK << 4)) || (handle->buffer[1] != 2))
	{
		handle->is_connected = false;
		close_socket(handle->socket);
		return CLIENT_MQTT_CONNECT_FAILED;
	}

	if (handle->buffer[3] != 0)
	{
		handle->is_connected = false;
		close_socket(handle->socket);
		return CLIENT_MQTT_CONNECT_FAILED;
	}

	return ret;
}

enum_mqtt_client_error mqtt_client_publish_mqtt(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t *data, uint16_t len_data, uint8_t qos)
{
	uint16_t len = create_mqtt_publish_frame(handle,
		topic,
		data,
		strlen((const char*)data),
		qos);

	int32_t ret = send_buffer(handle->socket, handle->buffer, len, MQTT_QOS_0);
	if(ret == SOCKET_ERROR) handle->is_connected = false;

	return (ret > 0) ? MQTT_CLIENT_SUCCESS :  MQTT_CLIENT_PUBLISH_FAILED;
}

bool mqtt_client_subscribe_mqtt(HANDLE_MQTT* handle_mqtt, const uint8_t *topic, uint8_t qos)
{
	uint8_t len = create_mqtt_subscribe_frame(handle_mqtt, topic, qos);
	size_t sent = send_buffer(handle_mqtt->socket, handle_mqtt->buffer, len, 0);

	if(sent == SOCKET_ERROR) handle_mqtt->is_connected = false;

	if(sent != len) return false;

	if(handle_mqtt->protocol_level < MQTT_PROTOCOL_LEVEL_3)
		return true;

	if (!process_mqtt_frame_until(handle_mqtt, MQTT_CTRL_SUBACK, SUBACK_TIMEOUT_MS))
		return false;

	return true;
}

/**
  * @brief  check and convert mqtt server url to IP address
  * @param  -
  * @retval - return true if OK, false if failed
  */
bool mqtt_client_check_distantname(HANDLE_MQTT* handle_mqtt, const uint8_t* url)
{
	bool ret = true;

	if(( url == NULL) || (crs_strlen(url) == 0U)) return 1U; /* Distant server name not defined*/

	/* DNS network resolution request */
	PRINT_INFO("Distant Name provided %s. DNS resolution started", url);

	com_sockaddr_t distantaddr;

	distantaddr.sa_len = (uint8_t)sizeof(com_sockaddr_t);

    if(com_gethostbyname(url, &distantaddr) != COM_SOCKETS_ERR_OK) return false; /* DNS resolution NOK. Will retry later */

    handle_mqtt->ip = ((com_sockaddr_in_t *)&distantaddr)->sin_addr.s_addr;

    return ret;
}
