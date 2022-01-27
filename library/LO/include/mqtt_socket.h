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

#ifndef MQTT_SOCKET_H_
#define MQTT_SOCKET_H_

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define SOCKET_ERROR			-1

typedef enum tls_type {
	TLS_NONE,
	TLS_1_0,	// deprecated
	TLS_1_1,	// deprecated
	TLS_1_2
}enum_tls_type;

extern int32_t create_socket();
extern void close_socket(int32_t socket);
extern int32_t connect_socket(int32_t socket, int32_t ip, uint16_t port, enum_tls_type tls_type);
extern int32_t send_buffer(int32_t socket, const uint8_t* buffer, int32_t len, int flags);
extern int32_t receive_buffer(int32_t socket, const uint8_t* buffer, int32_t len, uint32_t timeout);

#endif /* MQTT_SOCKET_H_ */
