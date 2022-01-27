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

#include "mqtt_socket.h"

#include "plf_config.h"
#include "liveobjects_conf.h"

#include "cellular_app.h"
#include "cellular_app_socket.h"
#include "cellular_app_trace.h"

#include "rtosal.h"

#include "com_sockets.h" /* includes all other includes */

#include "cellular_runtime_custom.h"


#define millis() ((unsigned long)HAL_GetTick())

int32_t create_socket()
{
	/* Create a TCP socket */
	return com_socket(COM_AF_INET, COM_SOCK_STREAM, COM_IPPROTO_TCP);
}

int32_t connect_socket(int32_t socket, int32_t ip, uint16_t port, enum_tls_type tls_type)
{
	uint32_t ret;
	com_sockaddr_in_t address;

	#if (USE_MBEDTLS == 1)
		if(tls_type == TLS_1_0)
		{
			/* Root CA of MQTT sever */
			static uint8_t mqttclient_root_ca[] = MQTTCLIENT_ROOT_CA;
			bool false_val = false;

			uint32_t timeout = 0U;

			/* set socket parameters */
			ret = net_setsockopt(socket, NET_SOL_SOCKET, NET_SO_RCVTIMEO, (const void *)&timeout, sizeof(timeout));

			if (ret == NET_OK)
			{
				ret = net_setsockopt(socket, NET_SOL_SOCKET, NET_SO_SECURE, (const void *)NULL, 0U);
			}

			/* set CA root for mbedTLS authentication  */
			if (ret == NET_OK)
			{
				ret = net_setsockopt(socket, NET_SOL_SOCKET, NET_SO_TLS_CA_CERT, (void *)mqttclient_root_ca,
								 (crs_strlen((uint8_t *)mqttclient_root_ca) + 1U));
			}
			if (ret == NET_OK)
			{
				ret = net_setsockopt(socket, NET_SOL_SOCKET, NET_SO_TLS_SERVER_VERIFICATION, &false_val, sizeof(bool));
			}
		}
	#else
	  ret = 0U;
	#endif /* (USE_MBEDTLS == 1) */

	if (ret == 0U)
	{
        address.sin_family      = (uint8_t)COM_AF_INET;
        address.sin_port        = COM_HTONS(port);
        address.sin_addr.s_addr = ip;

		/* connection to mqtt server */
        ret = (uint32_t)com_connect(socket, (com_sockaddr_t const *)&address, (int32_t)sizeof(com_sockaddr_in_t));
	}

	if (ret == 0U)
	{
		PRINT_INFO("socket connect success");
	}
	else
	{
		PRINT_INFO("socket connect failed");
		close_socket(socket);
	}

	return ret;
}

void close_socket(int32_t socket)
{
	if (com_closesocket(socket) == COM_SOCKETS_ERR_OK)
	{
		PRINT_INFO("socket close success");
	}
	else
	{
		PRINT_INFO("socket close failed");
	}
}

int32_t send_buffer(int32_t socket, const uint8_t* buffer, int32_t len, int flags)
{
	int32_t sent = 0;

    while(sent < len)
    {
    	int32_t res = com_send((int32_t) socket, (uint8_t *)buffer, len, flags);

        if (res < 1)
        	return SOCKET_ERROR;

        sent += (size_t) res;
    }
    return sent;
}

int32_t receive_buffer(int32_t socket, const uint8_t* buffer, int32_t len, uint32_t timeout)
{
    uint8_t* p_buffer = (uint8_t*)buffer;
    int32_t len_ret = 0;

    unsigned long current_ms = millis();
    do
    {
    	int32_t res = com_recv((int32_t)socket, (uint8_t *)p_buffer, len, COM_MSG_DONTWAIT);

        if (res > 0)
        {
            /* successfully read bytes from the socket */
        	p_buffer += res;
        	len_ret += res;

            current_ms = millis();
        }
        else if ((res < 0) && (res != COM_SO_RCVTIMEO))
        {
            return SOCKET_ERROR;
        }
    } while ((len_ret < len) && ((millis() - current_ms) < timeout));

    return len_ret;
}
