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

/**
* @file			mqtt_liveobjects.h
* @brief		Helpful methods to interface to Orange LiveObjects Platform
* @details		This class contains all the necessary methods to be able to interact easily
* 				with the Orange LiveObjects IoT platform
*/

#ifndef MQTT_LIVE_OBJECTS_H_
#define MQTT_LIVE_OBJECTS_H_

#include "json.h"
#include "mqtt_client.h"
#include "liveobjects_conf.h"
#include "orange_mqtt.h"
/**
* @brief     Live Objects has several application behaviors depending on the connection mode (user name) described in the following section.
* 			 Currently Live objects supports 3 connection modes.
*
* @details   Each of these values ​​is used to define the connection mode with LiveObjects
* 			 "Device" mode used to communicate with a native MQTT device.
*			 "External connector" mode is an API that allows communication with devices using other protocols not supported by Live Objects.
*			 To communicate with Live Objects, these devices are connected with a backend itself connected in "external connector" mode that can translate requests and responses (from / to) MQTT.
*			 "Application" mode with this mode, Live Objects provides an application mode to allow business applications to use the MQTT API.
*			 "Bridge" and "Payload Bridge" are deprecated
*/
typedef enum mode
{
	DEVICE_MODE = 0,
	CONNECTOR_MODE,
	APPLICATION_MODE,
	BRIDGE_MODE, 			// deprecated
	PAYLOAD_BRIDGE_MODE, 	// deprecated
	COUNT_MODE
}enum_mode;


/**
* @brief     List of publish topic managed by LiveObjects
*
* @details   There are 4 types of publish topic:
* 			 - Data message publication
* 			 	. TOPIC_DATA: to publish a JSON data message
*				. TOPIC_DATA_BINARY: to publish a binary data message and use a decoder associated to the device’s interface
*			 - Command management
*			 	. CMD_RESULT : to return command responses
*			 - Configuration management
*			 	. CONFIG_TOPIC: to announce the current configuration or respond to a config update request
*			 - Resource management
*			 	. RSC_TOPIC: to announce the current resource versions
*			 	. RSC_UDP_RES_TOPIC: to receive resource update requests and announce remote resource update compatibility
*			 	. RSC_UDP_ERR_TOPIC: to respond to resource update requests
*/
typedef enum publish_topic {
	TOPIC_DATA = 0,
	TOPIC_DATA_BINARY,
	CMD_RESULT,
	CONFIG_TOPIC,
	RSC_TOPIC,
	RSC_UDP_RES_TOPIC,
	RSC_UDP_ERR_TOPIC,
	COUNT_PUBLISH_TOPIC
}enum_publish_topic;

/**
* @brief     List of subscribe topic managed by LiveObjects
*
* @details   There are 3 types of subscribe topic:
*			 - Command management
*			 	. CMD_TOPIC : to receive commands and announce remote command compatibility
*			 - Configuration management
*			 	. CONFIG_UDP_TOPIC: to receive configuration update requests and announce remote configuration update compatibility
*			 - Resource management
*			 	. RSC_UDP_TOPIC: to receive resource update requests and announce remote resource update compatibility
*/
typedef enum subscribe_topic {
	CMD_TOPIC = 0,
	CONFIG_UDP_TOPIC,
	RSC_UDP_TOPIC,
	COUNT_SUBSCRIBE_TOPIC
}enum_subscribe_topic;

/**
* @brief     List of status of update response (resource management)
*
* @details   There are 7 status:
* 			- OK : the update is accepted and will start,
*			- UNKNOWN_RESOURCE : the update is refused, because the resource (identifier) is unsupported by the device
*			- WRONG_SOURCE_VERSION : the device is no longer in the "current" (old) resource version specified in the resource update request
*			- WRONG_TARGET_VERSION : the device doesn’t support the "target" (new) resource version specified in the resource update request
*			- INVALID_RESOURCE : the requested new resource version has incorrect version format or metadata
*			- NOT_AUTHORIZED : the device refuses to update the targeted resource (ex: bad timing, "read-only" resource, etc.)
*			- INTERNAL_ERROR : an error occurred on the device, preventing for the requested resource update
*/
typedef enum update_response {
	OK,
	UNKNOWN_RESOURCE = 0,
	WRONG_SOURCE_VERSION,
	WRONG_TARGET_VERSION,
	INVALID_RESOURCE,
	NOT_AUTHORIZED,
	INTERNAL_ERROR,
	COUNT_UPDATE_RESPONSES
}enum_update_response;

extern uint8_t lo_start_cellular(HANDLE_MQTT* handle, uint32_t timeout);

/**
* @brief		Create and initializing instance of LiveObjects connection
* @details		Used to instantiate a new LiveObjects handle
* @return		Pointer of the instance handle
*/
extern HANDLE_MQTT* lo_create();

/**
* @brief		disconnect instance of LiveObjects connection
* @details		Used to disconnect an existing LiveObjects connexion
* @return		void
*/

extern void lo_disconnect(HANDLE_MQTT* handle);


/**
* @brief		Setting the mandatory parameters to connect to LiveObjects
* @details		Set the default parameters defined in liveobjects_conf.h file
* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
* @return		Boolean value, true if everything is ok, false if there was a problem during the execution.
*/

extern bool lo_set_default_config(HANDLE_MQTT* handle);
/**
	* @brief		Setting the mandatory parameters to connect to LiveObjects
	* @details		This function allows the user to define all the parameters essential to the connection of LiveObjects
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		server_name		LiveObjects URL \b 16 uint8_t*
	* @param		port			MQTT port \b 16 uint16_t
	* @param		user_name		Connection mode (see enum_mode) \b 16 uint8_t*
	* @param		password		A valid API key value (Tenant ID) \b 16 uint8_t*
	* @param		client_id		The clientId value is used as MQTT client Id must be a valid Live Objects URN and conform to the following format: urn:lo:nsid:{namespace}:{id} \b 16 uint8_t*
	* @param		stream_id  		Data stream of the device \b uint8_t*
	* @return		Boolean value, true if everything is ok, false if there was a problem during the execution.
	*/
extern bool lo_set_config(HANDLE_MQTT* handle, uint8_t* server_name, uint16_t port, uint8_t* user_name, uint8_t* password, uint8_t* client_id, uint8_t* stream_id);

/**
	* @brief		Connect to LiveObjects (no secure mode)
	* @details		This function connect to LiveObjects
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @return		enum value, MQTT_CLIENT_SUCCESS if everything is ok, other if there was a problem during the execution.
*/
extern enum_mqtt_client_error lo_connect_mqtt(HANDLE_MQTT* handle);

#if (USE_MBEDTLS == 1)
	/**
		* @brief		Connect to LiveObjects (Secure mode)
		* @details		This function connect to LiveObjects
		* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
		* @param		tls_type TLS type (see enum_tls_type) \b 16 enum_tls_type
		* @return		Boolean value, true if everything is ok, false if there was a problem during the execution.
	*/
	extern enum_mqtt_client_error lo_connect_mqtts(HANDLE_MQTT* handle, enum_tls_type tls_type);
#endif

/**
	* @brief		Check if MQTT connection is available
	* @details		This function return the state of connection
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @return		void
*/
extern bool lo_is_connected(HANDLE_MQTT* handle);
/**
	* @brief		Set the procedure command callback (no secure mode)
	* @details		This function define the name of the callback which is called when an request is received
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		Pointer of the callback procedure
	* @return		void
*/
extern void lo_set_command_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT* handle_mqtt, int64_t cid, const uint8_t* req, const json_value* arg));

/**
	* @brief		Set the procedure config udp callback (no secure mode)
	* @details		This function define the name of the callback which is called when an request is received
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		Pointer of the callback procedure
	* @return		void
*/
extern void lo_set_config_udp_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT* handle_mqtt, int64_t cid, const uint8_t* req, const json_value* arg));

/**
	* @brief		Set the procedure rsc udp callback (no secure mode)
	* @details		This function define the name of the callback which is called when an request is received
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		Pointer of the callback procedure
	* @return		void
*/
extern void lo_set_rsc_udp_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(HANDLE_MQTT* handle_mqtt, int64_t cid, const uint8_t* req, const json_value* arg));

/**
	* @brief		Set the procedure generic subscribe callback (no secure mode)
	* @details		This function define the name of the callback which is called when an request is received
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		Pointer of the callback procedure
	* @return		void
*/
extern void lo_set_subscribe_callback(HANDLE_MQTT* handle, void (*lo_publish_response_callback)(mqtt_response_publish *p_published));

/**
	* @brief		Subscribe topic
	* @details		This function subscribe a topic.
	* 				This feature is useful if a new topic is offered by LiveObjects and the software has not been updated. Otherwise, preferably use the lo_xxx_subscribe functions
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		const uint8_t* 	Topic name. \b 16 char*
*/
extern void lo_subscribe(HANDLE_MQTT* handle, const uint8_t* topic);

/**
	* @brief		Subscribe command topic
	* @details		This function subscribe "dev/cmd" topic to receive commands and announce remote command compatibility
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_command_subscribe(HANDLE_MQTT* handle);

/**
	* @brief		Subscribe configuration management topic
	* @details		This function subscribe "dev/cfg/upd" topic to receive configuration update requests and announce remote configuration update compatibility
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_config_update_subscribe(HANDLE_MQTT* handle);

/**
	* @brief		Subscribe resource management topic
	* @details		This function subscribe "dev/rsc/upd" topic to receive resource update requests and announce remote resource update compatibility
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_resource_update_subscribe(HANDLE_MQTT* handle);

/**
	* @brief		Unsubscribe topic
	* @details		This function unsubscribe a topic.
	* 				This feature is useful if a new topic is offered by LiveObjects and the software has not been updated. Otherwise, preferably use the lo_xxx_unsubscribe functions
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		char* 	Topic name. \b 16 char*
*/
extern void lo_unsubscribe(HANDLE_MQTT* handle, char* topic);

/**
	* @brief		Unsubscribe command topic
	* @details		This function unsubscribe "dev/cmd" topic
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_command_unsubscribe(HANDLE_MQTT* handle);

/**
	* @brief		Unsubscribe configuration management topic
	* @details		This function subscribe "dev/cfg/upd" topic
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_config_update_subscribe(HANDLE_MQTT* handle);

/**
	* @brief		Unsubscribe resource management topic
	* @details		This function subscribe "dev/rsc/upd" topic
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
*/
extern void lo_resource_update_subscribe(HANDLE_MQTT* handle);

/**
	* @brief		Publish data values
	* @details		This function publish to LiveObjects the json values with Data enrichment logic. (see for more details https://liveobjects.orange-business.com/doc/html/lo_manual_v2.html#MQTT_DEV_DATA)
	* @param		handle 			Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		const char* 	Json Serialized values (example : {"tag1":"val1","tag2":"val2","tag3":"val3"}" \b 16 char*
*/
extern enum_mqtt_client_error lo_publish_data(HANDLE_MQTT* handle, const char* data);
/**
	* @brief		Publish command responses
	* @details		This function publish to LiveObjects the json command responses. (see for more details https://liveobjects.orange-business.com/doc/html/lo_manual_v2.html#MQTT_DEV_CMD)
	* @param		handle 		Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		cid			CID send by receive command
	* @param		response	Json Serialized arg values (example : {\"done\": true} \b 16 char*
*/
extern enum_mqtt_client_error lo_publish_ack_command(HANDLE_MQTT* handle, int64_t cid, char* response);

/**
	* @brief		Publish command responses
	* @details		This function publish to LiveObjects the json resource update response. (see for more details https://liveobjects.orange-business.com/doc/html/lo_manual_v2.html#MQTT_DEV_CMD)
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		cid		CID send by receive command \b 16 int64_t
	* @param		enum_update_response response status \b 16 enum_update_response
*/
extern enum_mqtt_client_error lo_publish_ack_config_update(HANDLE_MQTT* handle, int64_t cid, enum_update_response response);

/**
	* @brief		Synchronize MQTT connection
	* @details		This function makes it possible to validate the MQTT connection and to send a keep alive if necessary and to report data reception events from the server
	* @param		handle 	Instance of the LiveObjects connection \b 16 HANDLE_MQTT*
	* @param		cid		CID send by receive command \b 16 int64_t
*/

void lo_set_cellular_callback(HANDLE_MQTT* handle, void (*lo_cellular_callback)(HANDLE_MQTT* handle_mqtt, uint8_t msg));

void lo_set_cellular_ip_callback(HANDLE_MQTT* handle, void (*lo_cellular_ip_callback)(HANDLE_MQTT* handle_mqtt, uint8_t msg));

#endif /* MQTT_LIVE_OBJECTS_H_ */
