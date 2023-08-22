#include "networksvc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cy_secure_sockets.h"
#include "basicproc.h"
#include "basicexec.h"
#include "basictask.h"
#include <stdio.h>
#include <ctype.h>

#define TCP_KEEP_ALIVE_IDLE_TIME_MS (1000000u)
#define TCP_KEEP_ALIVE_INTERVAL_MS (1000u)
#define TCP_KEEP_ALIVE_RETRY_COUNT (2000u)

static cy_socket_t gSocket;
static cy_socket_t gClient;
static int rx_buffer_received = 0;
static char rx_buffer_data[512];
static char mbuffer[32];
static bool connected = false;
static uint8_t response[3];


#define MAKE_IPV4_ADDRESS(a, b, c, d) ((((uint32_t)d) << 24) | \
                                       (((uint32_t)c) << 16) | \
                                       (((uint32_t)b) << 8) |  \
                                       ((uint32_t)a))
#define TCP_SERVER_IP_ADDRESS MAKE_IPV4_ADDRESS(192, 168, 10, 1)

static cy_socket_sockaddr_t tcp_server_addr = {
    .ip_address.ip.v4 = TCP_SERVER_IP_ADDRESS,
    .ip_address.version = CY_SOCKET_IP_VER_V4,
    .port = 23};

cy_rslt_t network_svc_send_data(const char *buffer, int length)
{
    uint32_t bytes = length;
    uint32_t sent = 0;
    cy_rslt_t res;

    while (bytes > 0)
    {
        res = cy_socket_send(gClient, buffer, bytes, CY_SOCKET_FLAGS_NONE, &sent);
        if (res != CY_RSLT_SUCCESS)
            return res;

        bytes -= sent;
    }

    return CY_RSLT_SUCCESS;
}

static cy_rslt_t tcp_connection_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result;
    uint32_t keep_alive_interval = TCP_KEEP_ALIVE_INTERVAL_MS;
    uint32_t keep_alive_count = TCP_KEEP_ALIVE_RETRY_COUNT;
    uint32_t keep_alive_idle_time = TCP_KEEP_ALIVE_IDLE_TIME_MS;

    /* Client socket address. */
    cy_socket_sockaddr_t client_addr;

    /* Size of the client socket address. */
    uint32_t client_addr_len;

    /* Accept a new incoming connection from a TCP client.*/
    result = cy_socket_accept(socket_handle, &client_addr, &client_addr_len, &gClient);
    if (result != CY_RSLT_SUCCESS)
        return false;

    result = cy_socket_setsockopt(gClient, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_INTERVAL,
                                  &keep_alive_interval, sizeof(keep_alive_interval));
    if (result != CY_RSLT_SUCCESS)
        return false;

    /* Set the retry count for TCP keep alive packet. */
    result = cy_socket_setsockopt(gClient, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_COUNT,
                                  &keep_alive_count, sizeof(keep_alive_count));
    if (result != CY_RSLT_SUCCESS)
        return false;

    /* Set the network idle time before sending the TCP keep alive packet. */
    result = cy_socket_setsockopt(gClient, CY_SOCKET_SOL_TCP,
                                  CY_SOCKET_SO_TCP_KEEPALIVE_IDLE_TIME,
                                  &keep_alive_idle_time, sizeof(keep_alive_idle_time));
    if (result != CY_RSLT_SUCCESS)
        return false;

    connected = true;

    printf("  telnet client connected\n");

    return result;
}

static cy_rslt_t createTcpSocket(void)
{
    cy_rslt_t result;
    /* TCP socket receive timeout period. */
    uint32_t tcp_recv_timeout = 500;

    /* Variables used to set socket options. */
    // cy_socket_opt_callback_t tcp_receive_option;
    cy_socket_opt_callback_t tcp_connection_option;
    // cy_socket_opt_callback_t tcp_disconnection_option;

    /* Create a TCP socket */
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM, CY_SOCKET_IPPROTO_TCP, &gSocket);
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Failed to create socket! Error code: 0x%08x\n", (unsigned int)result);
        return result;
    }

    /* Set the TCP socket receive timeout period. */
    result = cy_socket_setsockopt(gSocket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RCVTIMEO, &tcp_recv_timeout, sizeof(tcp_recv_timeout));
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Set socket option: CY_SOCKET_SO_RCVTIMEO failed\n");
        return result;
    }

    /* Register the callback function to handle connection request from a TCP client. */
    tcp_connection_option.callback = tcp_connection_handler;
    tcp_connection_option.arg = NULL;

    result = cy_socket_setsockopt(gSocket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK, &tcp_connection_option, sizeof(cy_socket_opt_callback_t));
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Set socket option: CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK failed\n");
        return result;
    }

    /* Bind the TCP socket created to Server IP address and to TCP port. */
    result = cy_socket_bind(gSocket, &tcp_server_addr, sizeof(tcp_server_addr));
    if (result != CY_RSLT_SUCCESS)
    {
        printf("Failed to bind to socket! Error code: 0x%08lx\n", (uint32_t)result);
    }

    return result;
}

static bool createSocket()
{
    cy_rslt_t result;

    result = cy_socket_init();
    if (result != CY_RSLT_SUCCESS)
        return false;

    result = createTcpSocket();
    if (result != CY_RSLT_SUCCESS)
        return false;

    return true;
}

static bool listenSocket()
{
    cy_rslt_t result;

    result = cy_socket_listen(gSocket, 2);
    if (result != CY_RSLT_SUCCESS)
        return false;

    return true;
}

static bool isEmptyLine(const char *line)
{
    while (isspace((uint8_t)*line))
        line++;

    return *line == '\0';
}

void network_service_task(void *param)
{
    cy_rslt_t result;
    uint32_t bytes_received;

    createSocket();
    listenSocket();

    while (1)
    {
        if (connected)
        {
            /* Receive the incoming message from the TCP server. */
            result = cy_socket_recv(gClient, mbuffer, 1, CY_SOCKET_FLAGS_NONE, &bytes_received);
            if (result == CY_RSLT_MODULE_SECURE_SOCKETS_WOULDBLOCK || result == CY_RSLT_MODULE_SECURE_SOCKETS_TIMEOUT)
            {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            else if (result == CY_RSLT_MODULE_SECURE_SOCKETS_NOT_CONNECTED)
            {
                basic_err_t err ;

                //
                // The telnet session has disconnected
                //
                printf("  telnet session disconnected - waiting for new connection\n") ;
                connected = 0;
                basic_clear(NULL, &err, network_svc_send_data) ;
                listenSocket();
            }
            else if (result != CY_RSLT_SUCCESS)
            {
                printf("  error reading from socket, code = %08lx\n", result);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            else if (bytes_received > 0)
            {
                rx_buffer_data[rx_buffer_received++] = mbuffer[0];

                if (rx_buffer_data[0] == 255 && rx_buffer_received == 3)
                {
                    if (rx_buffer_data[1] == 253)
                    {
                        response[0] = 255;
                        response[1] = 252;
                        response[2] = rx_buffer_data[2];
                        network_svc_send_data((const char *)response, 3);
                    }
                    rx_buffer_received = 0;
                }
                else if (rx_buffer_data[rx_buffer_received - 1] == '\n' || rx_buffer_data[rx_buffer_received - 1] == '\r')
                {
                    rx_buffer_data[rx_buffer_received] = '\0';

                    if (!isEmptyLine(rx_buffer_data))
                    {
                        if (!basic_queue_line((const char *)rx_buffer_data, network_svc_send_data)) {
                            // TODO - out of memory error
                        }
                    }
                    
                    rx_buffer_received = 0;
                }
            }
        }
        else
        {
            // TODO - need to convert to an event that is triggered by a connection
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
}