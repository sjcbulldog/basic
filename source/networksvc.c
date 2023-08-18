#include "networksvc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cy_secure_sockets.h"
#include "basicproc.h"
#include <stdio.h>

static cy_socket_t gSocket ;
static int rx_buffer_received = 0 ;
static char rx_buffer_data[512];

static const char *prompt = "Basic06> " ;

#define MAKE_IPV4_ADDRESS(a, b, c, d)     ((((uint32_t) d) << 24) | \
                                          (((uint32_t)  c) << 16) | \
                                          (((uint32_t)  b) << 8) |\
                                          ((uint32_t)   a))
#define TCP_SERVER_IP_ADDRESS                     MAKE_IPV4_ADDRESS(192,168,10,1)

static cy_socket_sockaddr_t tcp_server_addr = {
        .ip_address.ip.v4 = TCP_SERVER_IP_ADDRESS,
        .ip_address.version = CY_SOCKET_IP_VER_V4,
        .port = 23
    };

static cy_rslt_t sendLine(const char *buffer, int length)
{
    uint32_t bytes = length ;
    uint32_t sent = 0 ;
    cy_rslt_t res ;

    while (bytes > 0) {
        res = cy_socket_send(gSocket, buffer, bytes, CY_SOCKET_FLAGS_NONE, &sent) ;
        if (res != CY_RSLT_SUCCESS)
            return res ;

        bytes -= sent ;
    }

    return CY_RSLT_SUCCESS ;
}

static cy_rslt_t tcp_connection_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result;
 
    /* Client socket address. */
    cy_socket_sockaddr_t client_addr;
 
    /* Size of the client socket address. */
    uint32_t client_addr_len;

    /* Accept a new incoming connection from a TCP client.*/
    result = cy_socket_accept(socket_handle, &client_addr, &client_addr_len, &gSocket);

    sendLine(prompt, strlen(prompt)) ;
 
    return result;
}

static char mbuffer[32] ;
static void tcp_server_recv_handler(cy_socket_t socket_handle, void *arg)
{
    cy_rslt_t result ;

    /* Variable to store the number of bytes received. */
    uint32_t bytes_received = 0;

    /* Receive the incoming message from the TCP server. */
    result = cy_socket_recv(socket_handle, mbuffer, 8, CY_SOCKET_FLAGS_NONE, &bytes_received);
    printf("%d %d\n", bytes_received, mbuffer[0]);

    if (result == CY_RSLT_SUCCESS && bytes_received > 0) 
    {
        memcpy(rx_buffer_data + rx_buffer_received, mbuffer, bytes_received) ;
        rx_buffer_received += bytes_received ;
        if (rx_buffer_received > 0 && (rx_buffer_data[rx_buffer_received - 1] == '\n' || rx_buffer_data[rx_buffer_received - 1] == '\r'))
        {
            rx_buffer_data[rx_buffer_received--] = '\0' ;
            while (rx_buffer_data[rx_buffer_received] == '\n' || rx_buffer_data[rx_buffer_received] == '\r') {
                rx_buffer_data[rx_buffer_received--] = '\0' ;
            }

            basicProcLine((const char *)rx_buffer_data) ;
            rx_buffer_received = 0 ;
        }
    }
}

static bool createSocket() 
{
    cy_rslt_t result;

    result = cy_socket_init() ;
    if (result != CY_RSLT_SUCCESS)
        return false ; 

    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_STREAM, CY_SOCKET_IPPROTO_TCP, &gSocket);
    if (result != CY_RSLT_SUCCESS)
        return false ;

    cy_socket_opt_callback_t tcp_connection_option = {
        .callback = (cy_socket_callback_t)tcp_connection_handler,
        .arg = NULL
    };

    result = cy_socket_setsockopt(gSocket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_CONNECT_REQUEST_CALLBACK, &tcp_connection_option, sizeof(cy_socket_opt_callback_t));
    if (result != CY_RSLT_SUCCESS)
        return false ;

        /* Variable used to set the socket options. */
    cy_socket_opt_callback_t tcp_recv_option = {
        .callback = (cy_socket_callback_t)tcp_server_recv_handler,
        .arg = NULL
    };
 
    /* Register the callback function to handle the messages received from the TCP client. */
    result = cy_socket_setsockopt(gSocket, CY_SOCKET_SOL_SOCKET, CY_SOCKET_SO_RECEIVE_CALLBACK, &tcp_recv_option, sizeof(cy_socket_opt_callback_t));
    if (result != CY_RSLT_SUCCESS)
        return false ;

    result = cy_socket_bind(gSocket, &tcp_server_addr, sizeof(tcp_server_addr));
    if (result != CY_RSLT_SUCCESS)
        return false ;
 
    /* Start listening on the TCP server socket. */
    result = cy_socket_listen(gSocket, 1);
    if (result != CY_RSLT_SUCCESS)
        return false ;

    return true ;    
}

void network_service_task(void *param)
{
    createSocket() ;
    while (1) {
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}