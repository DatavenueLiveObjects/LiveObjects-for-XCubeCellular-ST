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

#ifndef ORANGE_MQTT_H_
#define ORANGE_MQTT_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "json.h"
#include "rtosal.h"

#define MQTT_CTRL_CONNECT     		0x1
#define MQTT_CTRL_CONNECTACK  		0x2
#define MQTT_CTRL_PUBLISH     		0x3
#define MQTT_CTRL_PUBACK      		0x4
#define MQTT_CTRL_PUBREC      		0x5
#define MQTT_CTRL_PUBREL      		0x6
#define MQTT_CTRL_PUBCOMP     		0x7
#define MQTT_CTRL_SUBSCRIBE   		0x8
#define MQTT_CTRL_SUBACK      		0x9
#define MQTT_CTRL_UNSUBSCRIBE 		0xA
#define MQTT_CTRL_UNSUBACK    		0xB
#define MQTT_CTRL_PINGREQ     		0xC
#define MQTT_CTRL_PINGRESP    		0xD
#define MQTT_CTRL_DISCONNECT  		0xE

#define MQTT_PROTOCOL_LEVEL_3		3
#define MQTT_PROTOCOL_LEVEL_4		4

#define MQTT_QOS_1 					0x01
#define MQTT_QOS_0 					0x00

#define MQTT_CONN_USERNAMEFLAG    	0x80
#define MQTT_CONN_PASSWORDFLAG    	0x40
#define MQTT_CONN_WILLRETAIN      	0x20
#define MQTT_CONN_WILLQOS_1       	0x08
#define MQTT_CONN_WILLQOS_2       	0x18
#define MQTT_CONN_WILLFLAG        	0x04
#define MQTT_CONN_CLEANSESSION    	0x02

// Default keepalive to 30 seconds.
#define MQTT_CONN_KEEPALIVE 		20

#define CONNECT_TIMEOUT_MS 			6000
#define PUBLISH_TIMEOUT_MS 			500
#define PING_TIMEOUT_MS    			250
#define SUBACK_TIMEOUT_MS 			100

#define MAX_BUFFER_SIZE				256

typedef struct handle_cellular
{
	bool network_initialized;  			/* false: network not initialized, true:  network initialized  */
	bool network_is_on;        			/* false: network is down, true:  network is up  */
}HANDLE_CELLULAR;

typedef struct _mqtt_response_publish {
    /**
     * @brief The DUP flag. DUP flag is 0 if its the first attempt to send this publish packet. A DUP flag
     * of 1 means that this might be a re-delivery of the packet.
     */
    uint8_t dup_flag;

    /**
     * @brief The quality of service level.
     *
     * @see <a href="http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Table_3.11_-">
     * MQTT v3.1.1: QoS Definitions
     * </a>
     */
    uint8_t qos_level;

    /** @brief The retain flag of this publish message. */
    uint8_t retain_flag;

    /** @brief Size of the topic name (number of characters). */
    uint16_t topic_size;

    /**
     * @brief The topic name.
     * @note topic_name is not null terminated. Therefore topic_name_size must be used to get the
     *       string length.
     */
    const uint8_t* topic;

    /** @brief The publish message's packet ID. */
    uint16_t packet_id;

    /** @brief The publish message's application message.*/
    const uint8_t* message;

    /** @brief The size of the application message in bytes. */
    size_t message_size;
}mqtt_response_publish;


typedef struct _handle_mqtt
{
	bool cellular_activated;
	bool is_connected;

	int32_t ip;							/* IP address */
	int32_t socket;						/* Socket */

	uint8_t *server;
	uint16_t port;
	uint8_t *clientid;
	uint8_t *user;
	uint8_t *pass;

	uint8_t *stream_id;

	uint8_t protocol_level;
	uint8_t *will_topic;
	uint8_t *will_payload;
	uint8_t will_qos;
	bool will_retain;

	uint16_t keepalive_duration;
	uint32_t clock_keepalive_ms;

	uint16_t packet_id_counter;

	uint8_t buffer[MAX_BUFFER_SIZE];

	HANDLE_CELLULAR* handle_cellular;

	mqtt_response_publish response_publish;

	uint32_t msg_queue;
	osMessageQId cellular_queue;

	void (*lo_cellular_callback)(struct _handle_mqtt* handle, uint8_t msg);
	void (*lo_cellular_ip_callback)(struct _handle_mqtt*, uint8_t msg);

	void (*publish_response_callback)(struct _handle_mqtt*, mqtt_response_publish *p_publish);
	void (*lo_command_callback)(struct _handle_mqtt*, int64_t cid, const uint8_t* req, const json_value* arg);
	void (*lo_config_udp_callback)(struct _handle_mqtt*, int64_t cid, const uint8_t* req, const json_value* arg);
	void (*lo_rsc_udp_callback)(struct _handle_mqtt*, int64_t cid, const uint8_t* req, const json_value* arg);
	void (*lo_subscribe_callback)(mqtt_response_publish *p_published);
}HANDLE_MQTT;

extern HANDLE_MQTT* create_mqtt();
extern uint16_t create_mqtt_connect_frame(HANDLE_MQTT* handle);
extern uint16_t read_mqtt_frame(HANDLE_MQTT* handle, uint16_t maxsize, uint16_t timeout);
extern uint16_t process_mqtt_frame_until(HANDLE_MQTT* handle, uint8_t waitforpackettype, uint16_t timeout);
extern uint16_t read_mqtt_subscription(HANDLE_MQTT* handle, int16_t timeout);
extern uint16_t create_mqtt_publish_frame(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t *data, uint16_t len_data, uint8_t qos);
extern uint8_t create_mqtt_subscribe_frame(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t qos);
extern uint8_t create_mqtt_unsubscribe_frame(HANDLE_MQTT* handle, const uint8_t* topic);
extern bool mqtt_ping(HANDLE_MQTT* handle);
extern void mqtt_disconnect(HANDLE_MQTT* handle);

#endif /* ORANGE_MQTT_H_ */
