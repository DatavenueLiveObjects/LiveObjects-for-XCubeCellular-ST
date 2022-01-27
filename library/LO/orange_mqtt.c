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

#include "orange_mqtt.h"
#include "mqtt_socket.h"

#include "com_sockets.h" /* includes all other includes */
#include "cellular_app_trace.h"

HANDLE_MQTT handle_mqtt;

#define USE_TRACE_OERANGE_MQTT	1

HANDLE_MQTT* create_mqtt()
{
	handle_mqtt.cellular_activated = false;
	handle_mqtt.is_connected = false;
	handle_mqtt.ip = 0;
	handle_mqtt.socket = COM_SOCKET_INVALID_ID;

	handle_mqtt.server = NULL;
	handle_mqtt.port = 0;
	handle_mqtt.clientid = NULL;
	handle_mqtt.user = NULL;
	handle_mqtt.pass = NULL;
	handle_mqtt.stream_id = NULL;

	handle_mqtt.protocol_level = MQTT_PROTOCOL_LEVEL_4;

	handle_mqtt.will_topic = NULL;
	handle_mqtt.will_payload = NULL;
	handle_mqtt.will_qos = MQTT_CONN_WILLQOS_1;
	handle_mqtt.will_retain = true;

	handle_mqtt.keepalive_duration = MQTT_CONN_KEEPALIVE;
	handle_mqtt.clock_keepalive_ms = 0;

	handle_mqtt.packet_id_counter = 0;
	memset(handle_mqtt.buffer, 0, MAX_BUFFER_SIZE);

	handle_mqtt.handle_cellular = NULL;

	handle_mqtt.lo_cellular_callback = NULL;
	handle_mqtt.lo_cellular_ip_callback = NULL;

	handle_mqtt.publish_response_callback = NULL;
	handle_mqtt.lo_command_callback = NULL;
	handle_mqtt.lo_config_udp_callback = NULL;
	handle_mqtt.lo_rsc_udp_callback = NULL;
	handle_mqtt.lo_subscribe_callback = NULL;

	return &handle_mqtt;
}

static uint16_t set_string_to_buffer(uint8_t *dest, const uint8_t *src)
{
  uint16_t len = (uint16_t) strlen((const char*)src);

  dest[0] = len >> 8;
  dest[1] = len & 0xFF;

  strncpy((char *)&dest[2], (const char*)src, len);
  return len + 2;
}

uint16_t create_mqtt_connect_frame(HANDLE_MQTT* handle)
{
	uint16_t pos = 0;

	handle->buffer[pos] = (MQTT_CTRL_CONNECT << 4) & 0xF0;
	pos += 2;

	if(handle->protocol_level == MQTT_PROTOCOL_LEVEL_3)
		pos += set_string_to_buffer(&handle->buffer[pos], (const uint8_t*)"MQIsdp");
	else pos += set_string_to_buffer(&handle->buffer[pos], (const uint8_t*)"MQTT");

	handle->buffer[pos] = handle->protocol_level;
	pos++;

	handle->buffer[pos] = MQTT_CONN_CLEANSESSION;

	if (handle->will_topic != NULL )
	{
		handle->buffer[pos] |= MQTT_CONN_WILLFLAG;
		handle->buffer[pos] |= handle->will_qos;

		if(handle->will_retain)
			handle->buffer[pos] |= MQTT_CONN_WILLRETAIN;
	}

	if(handle->user != NULL)
		handle->buffer[pos] |= MQTT_CONN_USERNAMEFLAG;

	if(handle->pass != 0)
		handle->buffer[pos] |= MQTT_CONN_PASSWORDFLAG;

	pos++;

	handle->buffer[pos] = handle->keepalive_duration >> 8;
	pos++;

	handle->buffer[pos] = handle->keepalive_duration & 0xFF;
	pos++;

	if( handle->protocol_level == MQTT_PROTOCOL_LEVEL_3)
	{
		pos += set_string_to_buffer(&handle->buffer[pos], handle->clientid);
	}
	else {
		if (handle->clientid != NULL)
		{
			pos += set_string_to_buffer(&handle->buffer[pos], handle->clientid);
		}
		else
		{
			/* Otherwise server generate random clientid */
			handle->buffer[pos] = 0x0;
			pos++;
			handle->buffer[pos] = 0x0;
			pos++;
		}
	}

	if (handle->will_topic != NULL) {
		pos += set_string_to_buffer(&handle->buffer[pos], handle->will_topic);
		pos += set_string_to_buffer(&handle->buffer[pos], handle->will_payload);
	}

	if (handle->user != NULL) {
		pos += set_string_to_buffer(&handle->buffer[pos], handle->user);
	}

	if(handle->pass != 0) {
		pos += set_string_to_buffer(&handle->buffer[pos], handle->pass);
	}

	handle->buffer[1] = pos - 2;

	return pos;
}

uint16_t read_mqtt_frame(HANDLE_MQTT* handle, uint16_t maxsize, uint16_t timeout)
{
	uint16_t pos = 0;

	int32_t len = receive_buffer(handle->socket, &handle->buffer[pos], (size_t)1, timeout);
	if(len == SOCKET_ERROR) handle->is_connected = false;

	if(len != 1) return 0;

	pos++;

	uint32_t value = 0;
	uint32_t multiplier = 1;
	uint8_t encoded_byte;

	do
	{
		len = receive_buffer(handle->socket, &handle->buffer[pos], 1, timeout);
		if(len == SOCKET_ERROR) handle->is_connected = false;

		if (len != 1) return 0;

		encoded_byte = handle->buffer[pos];
		pos++;

		uint32_t intermediate = encoded_byte & 0x7F;
		intermediate *= multiplier;
		value += intermediate;
		multiplier *= 128;

		if (multiplier > (128UL * 128UL * 128UL)) {
			PRINT_DBG("Bad packet length");
			return 0;
		}
	} while (encoded_byte & 0x80);

  if (value > (maxsize - pos - 1))
	  len = receive_buffer(handle->socket,  &handle->buffer[pos], (maxsize - pos - 1), timeout);
  else len = receive_buffer(handle->socket, &handle->buffer[pos], value, timeout);

  return (pos + len);
}

uint16_t process_mqtt_frame_until(HANDLE_MQTT* handle, uint8_t waitforpackettype, uint16_t timeout)
{
	uint16_t len;

	while(len > 0)
	{
		len = read_mqtt_frame(handle, MAX_BUFFER_SIZE, timeout);

		if(len <= 0){
			break;
		}

		if ((handle->buffer[0] >> 4) == waitforpackettype)
		{
			return len;
		}
	}
	return 0;
}

bool mqtt_pub_ack(HANDLE_MQTT* handle, uint16_t packetid)
{
	handle->buffer[0] = MQTT_CTRL_PUBACK << 4;
	handle->buffer[1] = 2;
	handle->buffer[2] = packetid >> 8;
	handle->buffer[3] = packetid;

	PRINT_INFO("MQTT pub ack packet");

	if(send_buffer(handle->socket, handle->buffer, 4, 0) == 0)
		return false;

	return true;
}

uint16_t read_mqtt_subscription(HANDLE_MQTT* handle, int16_t timeout)
{
	uint16_t topiclen, datalen = 0;

	uint16_t len = read_mqtt_frame(handle, MAX_BUFFER_SIZE, timeout);

	if (len < 3) return 0;

	if ((handle->buffer[0] & 0xF0) != (MQTT_CTRL_PUBLISH) << 4) return 0;

	topiclen = handle->buffer[3];

	uint8_t packet_id_len = 0;
	uint16_t packetid = 0;

	if ((handle->buffer[0] & 0x6) == 0x2)
	{
		packet_id_len = 2;
		packetid = handle->buffer[topiclen + 4];
		packetid <<= 8;
		packetid |= handle->buffer[topiclen + 5];
	}

	datalen = len - topiclen - packet_id_len - 4;

	if((handle->protocol_level > MQTT_PROTOCOL_LEVEL_3) &&  ((handle->buffer[0] & 0x6) == 0x2))
		mqtt_pub_ack(handle, packetid);

	handle->response_publish.packet_id = packetid;
	handle->response_publish.message = handle->buffer + 4 + topiclen + packet_id_len;
	handle->response_publish.message_size = datalen;
	handle->response_publish.topic = handle->buffer + 4;
	handle->response_publish.topic_size = topiclen;

	return datalen;
}


bool mqtt_ping(HANDLE_MQTT* handle)
{
	handle->buffer[0] = MQTT_CTRL_PINGREQ << 4;
	handle->buffer[1] = 0;
	PRINT_INFO("MQTT ping packet");

	int32_t res = send_buffer(handle->socket, handle->buffer, 2, 0);

	if (res != 2)
	{
		PRINT_INFO("Failed to send ping packet");
		return false;
	}

	return true;
}

void mqtt_disconnect(HANDLE_MQTT* handle)
{
	handle->buffer[0] = MQTT_CTRL_DISCONNECT << 4;
	handle->buffer[1] = 0;

	PRINT_INFO("MQTT disconnect");

	int32_t res = send_buffer(handle->socket, handle->buffer, 2, 0);
	if (res != 2)
	{
		PRINT_INFO("Failed to send disconnect packet");
		return;
	}

	handle->is_connected = false;
	close_socket(handle->socket);
}

uint16_t create_mqtt_publish_frame(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t *data, uint16_t len_data, uint8_t qos)
{
	memset(handle->buffer, 0, MAX_BUFFER_SIZE);

	uint8_t pos = 0;
	uint16_t len = 2; // two bytes to set the topic size

	len += strlen((const char*)topic); // topic length

	if(qos > 0) len += 2; // qos packet id

	len += len_data;

	handle->buffer[pos] = MQTT_CTRL_PUBLISH << 4 | qos << 1;
	pos++;

	do
	{
		uint8_t digit = len % 0x80;
		len /= 0x80;

		if (len > 0) digit |= 0x80;
		handle->buffer[pos] = digit;
		pos++;
	} while ( len > 0 );

  pos += set_string_to_buffer(&handle->buffer[pos], topic);

  if(qos > 0)
  {
	handle->buffer[pos] = (handle->packet_id_counter >> 8) & 0xFF;
    pos++;
    handle->buffer[pos] = handle->packet_id_counter & 0xFF;
    pos++;

    handle->packet_id_counter++;
  }

  memcpy(&handle->buffer[pos], data, len_data);
  pos += len_data;

  return pos;
}

uint8_t create_mqtt_subscribe_frame(HANDLE_MQTT* handle, const uint8_t* topic, uint8_t qos)
{
	memset(handle->buffer, 0, MAX_BUFFER_SIZE);

	uint8_t pos = 0;

	handle->buffer[pos] = MQTT_CTRL_SUBSCRIBE << 4 | MQTT_QOS_1 << 1;

	pos += 2;

	handle->buffer[pos] = (handle->packet_id_counter >> 8) & 0xFF;
	pos++;
	handle->buffer[pos] = handle->packet_id_counter & 0xFF;
	pos++;

	handle->packet_id_counter++;

	pos += set_string_to_buffer(&handle->buffer[pos], topic);

	handle->buffer[pos] = qos;
	pos++;

	handle->buffer[1] = pos - 2;

	return pos;
}

uint8_t create_mqtt_unsubscribe_frame(HANDLE_MQTT* handle, const uint8_t* topic)
{
	memset(handle->buffer, 0, MAX_BUFFER_SIZE);

	uint8_t pos = 0;

	handle->buffer[pos] = MQTT_CTRL_UNSUBSCRIBE << 4 | 0x1;

	pos += 2;

	handle->buffer[pos] = (handle->packet_id_counter >> 8) & 0xFF;
	pos++;
	handle->buffer[pos] = handle->packet_id_counter & 0xFF;
	pos++;

	handle->packet_id_counter++;

	pos += set_string_to_buffer(&handle->buffer[pos], topic);

	handle->buffer[1] = pos - 2;

	return pos;
}
