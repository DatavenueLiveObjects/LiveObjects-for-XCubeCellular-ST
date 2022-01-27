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
* @file			mqtt_client.h
* @brief		Helpful methods to interface to Orange LiveObjects Platform
* @details		This class contains all the necessary methods to be able to interact easily
* 				with MQTT platform
*/

#ifndef MQTT_CLIENT_H_
#define MQTT_CLIENT_H_

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "dc_common.h"
#include "cmsis_os.h"
#include "orange_mqtt.h"
#include "mqtt_socket.h"


extern osMessageQId cellular_queue;

#define EVENT_CELLULAR_INFO				1
#define EVENT_CELLULAR_IP_INFO			2

typedef enum mqtt_error {
	MQTT_CLIENT_SUCCESS = 0,
	MQTT_CLIENT_DISTANTNAME_UNKNOWN,
	MQTT_CLIENT_CREATE_SOCKET_FAILED,
	CLIENT_MQTT_CONNECT_SOCKET_FAILED,
	CLIENT_MQTT_CONNECT_FAILED,
	MQTT_CLIENT_PUBLISH_FAILED,
	MQTT_CLIENT_ERROR_MALFORMED_RESPONSE,
	MQTT_CLIENT_UNKNOWN_ERROR,
	MQTT_CLIENT_COUNT_ERROR,
}enum_mqtt_client_error;

extern HANDLE_CELLULAR* create_cellular();
extern HANDLE_CELLULAR* mqtt_client_init();
extern void mqtt_client_enable(HANDLE_MQTT* handle);
extern enum_mqtt_client_error mqtt_client_connect(HANDLE_MQTT* handle_mqtt, const uint8_t* url, uint16_t port, const uint8_t* user_name, const uint8_t* password, const uint8_t* client_id, enum_tls_type tls_type);
extern enum_mqtt_client_error mqtt_client_publish_mqtt(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t *data, uint16_t len_data, uint8_t qos);
extern bool mqtt_client_subscribe_mqtt(HANDLE_MQTT* handle_mqtt, const uint8_t *topic, uint8_t qos);
extern uint16_t publish_packet(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t *data, uint16_t len_data, uint8_t qos);

#endif /* MQTT_CLIENT_H_ */
