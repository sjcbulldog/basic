#include "ctrltask.h"
#include "networkconn.h"
#include "networksvc.h"
#include "initfs.h"
#include "basictask.h"
#include "uarttask.h"
#include "mstask.h"

#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

#define NETWORKCONN_TASK_STACK_SIZE         (10 * 1024)
#define NETWORKCONN_TASK_PRIORITY           (2)

#define NETWORKSVC_TASK_STACK_SIZE          (10 * 1024)
#define NETWORKSVC_TASK_PRIORITY            (3)

#define FSINIT_TASK_STACK_SIZE              (10 * 1024)
#define FSINIT_TASK_PRIORITY                (2)

#define UART_TASK_STACK_SIZE                (10 * 1024)
#define UART_TASK_PRIORITY                  (3)

#define BASIC_TASK_STACK_SIZE               (20 * 1024)
#define BASIC_TASK_PRIORITY                 (2)

#define USBMSD_TASK_SIZE                    (8 * 1024)
#define USBMSD_TASK_PRIORITY                (3)

static TaskHandle_t fs_init_handle ;
static TaskHandle_t netconn_handle ;
static TaskHandle_t netsvc_handle ;
static TaskHandle_t uart_handle ;
static TaskHandle_t basic_handle ;
static TaskHandle_t usbmsd_handle ;

static const bool useUART = true ;
static const bool useWIFI = false ;
static const bool useUSBMS = true ;

void control_task(void *param)
{
    printf("  Control task started\n") ;

    printf("  Starting file system init task\n") ;
    if (xTaskCreate(filesystem_init_task, "FilesystemInit", FSINIT_TASK_STACK_SIZE, NULL, FSINIT_TASK_PRIORITY, &fs_init_handle) != pdPASS) {
        printf("    - could not start file system init task\n") ;
        CY_ASSERT(false);
    }
    while (!filesystem_init_done()) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    printf("  Starting basic language processing task\n") ;
    if (xTaskCreate(basic_task, "BasicTask", BASIC_TASK_STACK_SIZE, NULL, BASIC_TASK_PRIORITY, &basic_handle) != pdPASS)
    {
        printf("    - could not start basic language processint task\n") ;
        CY_ASSERT(false);
    }

    if (useUSBMS)
    {
        printf("  Starting USB MSD task\n") ;
        if (xTaskCreate(usb_task, "USBMSD", USBMSD_TASK_SIZE, NULL, USBMSD_TASK_PRIORITY, &usbmsd_handle) != pdPASS) {
            printf("    - could not start USB mass storage task\n") ;
        }
    }

    if (useWIFI) {
        printf("  Starting wifi access point task\n\n\n") ;
        if (xTaskCreate(network_connect_task, "NetworkConnect", NETWORKCONN_TASK_STACK_SIZE, NULL, NETWORKCONN_TASK_PRIORITY, &netconn_handle) != pdPASS)
        {
            printf("    - could not start network access point task\n");
        }
        while (!network_connect_done()) {
            // Wait for the access point to come up
            vTaskDelay(500/portTICK_PERIOD_MS);
        }

        printf("  Starting telnet connection task\n");
        if (xTaskCreate(network_service_task, "NetworkService", NETWORKCONN_TASK_STACK_SIZE, NULL, NETWORKCONN_TASK_PRIORITY, &netsvc_handle) != pdPASS)
        {
            printf("    - could not start telnet connection task\n") ;
        }
    }

    if (useUART) {
        printf("  Starting UART input task\n") ;
        if (!xTaskCreate(uart_task, "UartTask", UART_TASK_STACK_SIZE, NULL, UART_TASK_PRIORITY, &uart_handle)) {
            printf("    - could not start UART input task\n") ;
        }
    }

    printf("\n\nBasic06 Ready\n") ;

    while (true) {
        //
        // We just hang out and let the other tasks do their work
        //
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}