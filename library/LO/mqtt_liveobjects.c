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

#include "mqtt_liveobjects.h"
#include "mqtt_socket.h"
#include <string.h>
#include <inttypes.h>
#include "mqtt_client.h"

#include "cellular_runtime_custom.h"
#include "cellular_service.h"
#include "orange_mqtt.h"
#include "cellular_app.h"
#include "cellular_app_socket.h"
#include "cellular_app_trace.h"
#include "cellular_control_common.h"
#include "cellular_control_api.h"


#define USE_TRACE_MQTT_LIVEOBJECTS	1

#define FREE_STRING(X)	if(X != NULL) { free(X); X = NULL;}

#define LO_TOPIC_DATA 			"dev/data"			/* to publish a JSON data message */
#define LO_TOPIC_DATA_BINARY	"dev/data/binary"	/* to publish a binary data message and use a decoder associated to the device’s interface */
#define LO_CMD_RESULT			"dev/cmd/res"		/* to return command responses */
#define LO_CONFIG_TOPIC 		"dev/cfg"			/* to announce the current configuration or respond to a config update request */
#define LO_RSC_TOPIC			"dev/rsc"  			/* to announce the current resource versions */
#define LO_RSC_UDP_RES_TOPIC	"dev/rsc/upd/res"	/* to respond to resource update requests */
#define LO_RSC_UDP_ERR_TOPIC	"dev/rsc/upd/err"	/* to announce resource update error */

#define LO_CMD_TOPIC 			"dev/cmd"			/* to receive commands and announce remote command compatibility */
#define LO_CONFIG_UDP_TOPIC		"dev/cfg/udp"		/* to receive configuration update requests and announce remote configuration update compatibility */
#define LO_RSC_UDP_TOPIC		"dev/rsc/upd"		/* to receive resource update requests and announce remote resource update compatibility */

#define LO_DEVICE_MODE			"json+device"
#define	LO_CONNECTOR_MODE		"connector"
#define	LO_APPLICATION_MODE		"application"
#define LO_BRIDGE_MODE	 		"json+bridge"		// deprecated
#define LO_PAYLOAD_BRIDGE_MODE 	"payload+bridge"	// deprecated

#define LO_OK_UPDATE_STATUS						"OK"
#define LO_UNKNOWN_RESOURCE_UPDATE_STATUS		"UNKNOWN_RESOURCE"
#define LO_WRONG_SOURCE_VERSION_UPDATE_STATUS	"WRONG_SOURCE_VERSION"
#define LO_WRONG_TARGET_VERSION_UPDATE_STATUS	"WRONG_TARGET_VERSION"
#define LO_INVALID_RESOURCE_UPDATE_STATUS		"INVALID_RESOURCE"
#define LO_NOT_AUTHORIZED_UPDATE_STATUS			"NOT_AUTHORIZED"
#define LO_INTERNAL_ERROR_UPDATE_STATUS			"INTERNAL_ERROR"

HANDLE_MQTT* handle;

const uint8_t* PUBLISH_TOPIC[] = {(uint8_t*)LO_TOPIC_DATA, (uint8_t*)LO_TOPIC_DATA_BINARY, (uint8_t*)LO_CMD_RESULT, (uint8_t*)LO_CONFIG_TOPIC,
		(uint8_t*)LO_RSC_TOPIC, (uint8_t*)LO_RSC_UDP_RES_TOPIC, (uint8_t*)LO_RSC_UDP_ERR_TOPIC };

const uint8_t* SUBSCRIBE_TOPIC[] = {(uint8_t*)LO_CMD_TOPIC, (uint8_t*)LO_CONFIG_UDP_TOPIC, (uint8_t*)LO_RSC_UDP_TOPIC};

const uint8_t* LO_MODE[] = {(uint8_t*)LO_DEVICE_MODE, (uint8_t*)LO_CONNECTOR_MODE, (uint8_t*)LO_APPLICATION_MODE,
		(uint8_t*)LO_BRIDGE_MODE, (uint8_t*)LO_PAYLOAD_BRIDGE_MODE};

const uint8_t* LO_STATUS_UPDATE[] = {(uint8_t*)LO_OK_UPDATE_STATUS, (uint8_t*)LO_UNKNOWN_RESOURCE_UPDATE_STATUS, (uint8_t*)LO_WRONG_SOURCE_VERSION_UPDATE_STATUS,
		(uint8_t*)LO_WRONG_TARGET_VERSION_UPDATE_STATUS, (uint8_t*)LO_INVALID_RESOURCE_UPDATE_STATUS, (uint8_t*)LO_NOT_AUTHORIZED_UPDATE_STATUS, (uint8_t*)LO_INTERNAL_ERROR_UPDATE_STATUS};

static void lo_publish_callback(HANDLE_MQTT* handle, mqtt_response_publish *p_published);

json_value* get_json_node(json_value* parent_node, char* name)
{
    if (parent_node == NULL) {
            return NULL;
    }

    if (parent_node->type != json_object) return NULL;

    const unsigned int count = parent_node->u.object.length;
	int i = 0;

    while(i < count)
	{
		char* current_name = parent_node->u.object.values[i].name;
		if(strcmp(current_name, name) == 0)
		{
			return parent_node->u.object.values[i].value;
		}
		else i++;
	}
    return NULL;
}

uint8_t lo_start_cellular(HANDLE_MQTT* handle, uint32_t timeout)
{
	if(!handle->cellular_activated) mqtt_client_enable(handle);

	handle->cellular_queue = rtosalMessageQueueNew(NULL, CELLULAR_APP_QUEUE_SIZE);

	(void)rtosalMessageQueueGet(handle->cellular_queue, &handle->msg_queue, RTOSAL_WAIT_FOREVER);
}

HANDLE_MQTT* lo_create()
{
	handle = create_mqtt();
	handle->handle_cellular = create_cellular();

	return handle;
}

void lo_free(HANDLE_MQTT* handle)
{
	if(handle != NULL)
	{
		FREE_STRING(handle->server);
		FREE_STRING(handle->clientid);
		FREE_STRING(handle->user);
		FREE_STRING(handle->pass);
		FREE_STRING(handle->stream_id);
		FREE_STRING(handle->will_topic);
		FREE_STRING(handle->will_payload);

		free(handle);
		handle = NULL;
	}
}

void lo_disconnect(HANDLE_MQTT* handle)
{
	mqtt_disconnect(handle);
}

bool lo_set_config(HANDLE_MQTT* handle, uint8_t* server_name, uint16_t port, uint8_t* user_name, uint8_t* password, uint8_t* client_id, uint8_t* stream_id)
{
	if(handle == NULL) return false;

	FREE_STRING(handle->server);
	FREE_STRING(handle->user);
	FREE_STRING(handle->pass);
	FREE_STRING(handle->clientid);
	FREE_STRING(handle->stream_id);

	size_t len = strlen((const char *)server_name);
	handle->server = (uint8_t*)malloc((strlen((const char *)server_name) + 1) * sizeof(uint8_t));
	strncpy((char *)handle->server, (const char *)server_name, strlen((const char *)server_name));
	handle->server[len] = '\0';

	len = strlen((const char *)user_name);
	handle->user = (uint8_t*)malloc((strlen((const char *)user_name) + 1) * sizeof(uint8_t));
	strncpy((char *)handle->user, (const char *)user_name, strlen((const char *)user_name));
	handle->user[len] = '\0';

	len = strlen((const char *)password);
	handle->pass = (uint8_t*)malloc((strlen((const char *)password) + 1) * sizeof(uint8_t));
	strncpy((char *)handle->pass, (const char *)password, strlen((const char *)password));
	handle->pass[len] = '\0';

	len = strlen((const char *)client_id);
	handle->clientid = (uint8_t*)malloc((strlen((const char *)client_id) + 1) * sizeof(uint8_t));
	strncpy((char *)handle->clientid, (const char *)client_id, strlen((const char *)client_id));
	handle->clientid[len] = '\0';

	len = strlen((const char *)stream_id);
	handle->stream_id = (uint8_t*)malloc((strlen((const char *)stream_id) + 1) * sizeof(uint8_t));
	strncpy((char *)handle->stream_id, (const char *)stream_id, strlen((const char *)stream_id));
	handle->stream_id[len] = '\0';

	handle->port = port;

	return true;
}

bool lo_set_default_config(HANDLE_MQTT* handle)
{
	return lo_set_config(handle, MQTTCLIENT_DEFAULT_SERVER_NAME, MQTTCLIENT_DEFAULT_SERVER_PORT,
		MQTTCLIENT_DEFAULT_USERNAME,
		MQTTCLIENT_DEFAULT_PASSWORD,
		MQTTCLIENT_DEFAULT_CLIENTID,
		MQTT_DEFAULT_STREAM_ID);
}

enum_mqtt_client_error lo_connect(HANDLE_MQTT* handle, enum_tls_type tls_type)
{
	handle->cellular_activated = true;
	handle->publish_response_callback = lo_publish_callback;

	enum_mqtt_client_error res = mqtt_client_connect(handle, handle->server,
			handle->port,
			handle->user,
			handle->pass,
			handle->clientid,
			tls_type);

	if(res == MQTT_CLIENT_SUCCESS)
		handle->clock_keepalive_ms = xTaskGetTickCount();

	return res;
}

void lo_synchronize(void *p_argument)
{
	HANDLE_MQTT* handle = (HANDLE_MQTT*)p_argument;

	while(true)
	{
		HAL_Delay(1000);
		uint32_t lastDelay = xTaskGetTickCount() - handle->clock_keepalive_ms;

		if(lastDelay > handle->keepalive_duration * 1000)
		{
			mqtt_ping(handle);
			handle->clock_keepalive_ms = xTaskGetTickCount();
		}
		uint16_t ret = read_mqtt_subscription(handle, SUBACK_TIMEOUT_MS);

		if(ret > 0)
		{
			if(handle->publish_response_callback != NULL)
			{
				handle->publish_response_callback(handle, &handle->response_publish);
			}
		}
	}
}

enum_mqtt_client_error lo_connect_mqtt(HANDLE_MQTT* handle)
{
	enum_mqtt_client_error res = lo_connect(handle, TLS_NONE);

	if(res == MQTT_CLIENT_SUCCESS)
	{
		uint8_t thread_name[CELLULAR_APP_THREAD_NAME_MAX];
		uint32_t len;

		/* Thread Name Generation */
		/* Let a space to add the EchoClt number and '+1' to copy '\0' */
		len = crs_strlen((const uint8_t *)"Synchronize ");
		(void)memcpy(thread_name, "Synchronize ", CELLULAR_APP_MIN((len + 1U), CELLULAR_APP_THREAD_NAME_MAX));
		/* Thread Creation */

		/* Thread Name Insatnce Update */
		thread_name[(len - 1U)] = (uint8_t)0x30 + 1U; /* start at EchoClt1 */

		/* Thread Creation */
		osThreadId thread_id = rtosalThreadNew((const rtosal_char_t *)thread_name, (os_pthread)lo_synchronize, ECHOCLIENT_THREAD_PRIO, ECHOCLIENT_THREAD_STACK_SIZE, handle);
		/* Check Creation is ok */
		if (thread_id == NULL)
		{
		  CELLULAR_APP_ERROR((CELLULAR_APP_ERROR_ECHOCLIENT + (int32_t)ECHOCLIENT_THREAD_NUMBER + 1), ERROR_FATAL)
		}
	}
	return res;
}


#if (USE_MBEDTLS == 1)
	enum_mqtt_client_error lo_connect_mqtts(HANDLE_MQTT* handle, enum_tls_type tls_type)
	{
		return lo_connect(handle, tls_type);
	}
#endif

bool lo_is_connected(HANDLE_MQTT* handle)
{
	return handle->is_connected;
}

void lo_set_cellular_callback(HANDLE_MQTT* handle, void (*lo_cellular_callback)(HANDLE_MQTT* handle_mqtt, uint8_t msg))
{
	handle->lo_cellular_callback = lo_cellular_callback;
}

void lo_set_cellular_ip_callback(HANDLE_MQTT* handle, void (*lo_cellular_ip_callback)(HANDLE_MQTT* handle_mqtt, uint8_t msg))
{
	handle->lo_cellular_ip_callback = lo_cellular_ip_callback;
}

void lo_set_command_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT* handle_mqtt, int64_t cid, const uint8_t* req, const json_value* arg))
{
	handle->lo_command_callback = lo_publish_response_callback;
}

void lo_set_config_udp_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT*, int64_t cid, const uint8_t* req, const json_value* arg))
{
	handle->lo_config_udp_callback = lo_publish_response_callback;
}

void lo_set_rsc_udp_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT*, int64_t cid, const uint8_t* req, const json_value* arg))
{
	handle->lo_rsc_udp_callback = lo_publish_response_callback;
}

void lo_set_subscribe_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(mqtt_response_publish *p_published))
{
	handle->lo_subscribe_callback = lo_publish_response_callback;
}

void lo_publish_callback(HANDLE_MQTT* handle, mqtt_response_publish *p_published)
{
  if (memcmp((const char*)p_published->topic, (const char*)SUBSCRIBE_TOPIC[CMD_TOPIC], p_published->topic_size) == 0)
  {
		json_char* json = (json_char*)p_published->message;

		json_value* node_parent = json_parse(json, p_published->message_size);
		if(node_parent == NULL) return;

		json_value* req_node = get_json_node(node_parent, "req");
		json_value* arg_node = get_json_node(node_parent, "arg");
		json_value* cid_node = get_json_node(node_parent, "cid");

		int64_t cid = cid_node->u.integer;
		char* req = req_node->u.string.ptr;

		HANDLE_MQTT* lo_handle = (HANDLE_MQTT*)handle;

		if(lo_handle->lo_command_callback != NULL)
			lo_handle->lo_command_callback(lo_handle, cid, (const uint8_t*)req, arg_node);
  }
  else if (memcmp((const char*)p_published->topic, (const char*)SUBSCRIBE_TOPIC[CONFIG_UDP_TOPIC], p_published->topic_size) == 0)
  {
		json_char* json = (json_char*)p_published->message;

		json_value* node_parent = json_parse(json, p_published->message_size);
		if(node_parent == NULL) return;

		json_value* req_node = get_json_node(node_parent, "req");
		json_value* arg_node = get_json_node(node_parent, "arg");
		json_value* cid_node = get_json_node(node_parent, "cid");

		int64_t cid = cid_node->u.integer;
		char* req = req_node->u.string.ptr;

		HANDLE_MQTT* lo_handle = (HANDLE_MQTT*)handle;

		if(lo_handle->lo_config_udp_callback != NULL)
			lo_handle->lo_config_udp_callback(lo_handle, cid, (const uint8_t*)req, arg_node);
  }
  else if (memcmp((const char*)p_published->topic, (const char*)SUBSCRIBE_TOPIC[RSC_UDP_TOPIC], p_published->topic_size) == 0)
  {
		json_char* json = (json_char*)p_published->message;

		json_value* node_parent = json_parse(json, p_published->message_size);
		if(node_parent == NULL) return;

		json_value* req_node = get_json_node(node_parent, "req");
		json_value* arg_node = get_json_node(node_parent, "arg");
		json_value* cid_node = get_json_node(node_parent, "cid");

		int64_t cid = cid_node->u.integer;
		char* req = req_node->u.string.ptr;

		HANDLE_MQTT* lo_handle = (HANDLE_MQTT*)handle;

		if(lo_handle->lo_rsc_udp_callback != NULL)
			lo_handle->lo_rsc_udp_callback(lo_handle, cid, (const uint8_t*)req, arg_node);
  }
  else
  {
	  HANDLE_MQTT* lo_handle = (HANDLE_MQTT*)handle;

	  if(lo_handle->lo_subscribe_callback != NULL)
		  lo_handle->lo_subscribe_callback(p_published);
  }
}

void lo_subscribe(HANDLE_MQTT* handle, const uint8_t* topic)
{
	mqtt_client_subscribe_mqtt(handle, topic, MQTT_QOS_0);
}

void lo_command_subscribe(HANDLE_MQTT* handle)
{
	lo_subscribe(handle, SUBSCRIBE_TOPIC[CMD_TOPIC]);
}

void lo_config_update_subscribe(HANDLE_MQTT* handle)
{
	lo_subscribe(handle, SUBSCRIBE_TOPIC[CONFIG_UDP_TOPIC]);
}

void lo_resource_update_subscribe(HANDLE_MQTT* handle)
{
	lo_subscribe(handle, SUBSCRIBE_TOPIC[RSC_UDP_TOPIC]);
}

void lo_unsubscribe(HANDLE_MQTT* handle, char* topic)
{
	//unsubscribe_packet(handle, topic);
}

void lo_command_unsubscribe(HANDLE_MQTT* handle)
{
	lo_unsubscribe(handle, (char*)SUBSCRIBE_TOPIC[CMD_TOPIC]);
}

void lo_config_update_unsubscribe(HANDLE_MQTT* handle)
{
	lo_unsubscribe(handle, (char*)SUBSCRIBE_TOPIC[CONFIG_UDP_TOPIC]);
}

void lo_resource_update_unsubscribe(HANDLE_MQTT* handle)
{
	lo_unsubscribe(handle, (char*)SUBSCRIBE_TOPIC[RSC_UDP_TOPIC]);
}

enum_mqtt_client_error lo_publish_data(HANDLE_MQTT* handle, const char* data)
{
	if(handle == NULL) return MQTT_CLIENT_UNKNOWN_ERROR;

	uint8_t buffer[MAX_BUFFER_SIZE];

	sprintf((char*)buffer, "{\"s\":\"%s\",\"v\":%s}", (const char*)handle->stream_id, data);

	PRINT_INFO("%s", (char*)buffer);

	return mqtt_client_publish_mqtt(handle, PUBLISH_TOPIC[TOPIC_DATA],
		buffer,
		strlen((const char*)buffer),
		MQTT_QOS_0);
}

enum_mqtt_client_error lo_publish_ack_config_update(HANDLE_MQTT* handle, int64_t cid, enum_update_response response)
{
	if(response > COUNT_UPDATE_RESPONSES) return 0;

	char jsonCmdResponse[128];
	memset(jsonCmdResponse, 0, 128);

	sprintf(jsonCmdResponse, "{\"cfg\":%s,\"cid\":%" PRId64 "}", (const CRC_CHAR_t *)LO_STATUS_UPDATE[response], cid);

	return mqtt_client_publish_mqtt(handle, PUBLISH_TOPIC[CONFIG_TOPIC],
		(uint8_t*)jsonCmdResponse,
		strlen(jsonCmdResponse),
		MQTT_QOS_0);
}

enum_mqtt_client_error lo_publish_ack_command(HANDLE_MQTT* handle, int64_t cid, char* response)
{
	char jsonCmdResponse[128];
	memset(jsonCmdResponse, 0, 128);

	sprintf(jsonCmdResponse, "{\"res\":%s,\"cid\":%" PRId64 "}", response, cid);

	return mqtt_client_publish_mqtt(handle, PUBLISH_TOPIC[CMD_RESULT],
		(uint8_t*)jsonCmdResponse,
		strlen(jsonCmdResponse),
		MQTT_QOS_0);
}
