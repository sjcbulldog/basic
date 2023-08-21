#include "ctrltask.h"
#include "networkconn.h"
#include "networksvc.h"
#include "initfs.h"

#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

#define NETWORKCONN_TASK_STACK_SIZE         (5 * 1024)
#define NETWORKCONN_TASK_PRIORITY           (2)
#define NETWORKSVC_TASK_STACK_SIZE          (64 * 1024)
#define NETWORKSVC_TASK_PRIORITY            (3)
#define FSINIT_TASK_STACK_SIZE              (5 * 1024)
#define FSINIT_TASK_PRIORITY                (2)

static TaskHandle_t fs_init_handle ;
static TaskHandle_t netconn_handle ;

void control_task(void *param)
{
    printf("  Control task started\n") ;

    printf("  Starting file system init task\n") ;
    xTaskCreate(filesystem_init_task, "FilesystemInit", 8 * 1024, NULL, 2, &fs_init_handle);
    while (!filesystem_init_done()) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    printf("  Starting network stack task\n") ;
    xTaskCreate(network_connect_task, "NetworkConnect", NETWORKCONN_TASK_STACK_SIZE, NULL, NETWORKCONN_TASK_PRIORITY, &netconn_handle);
    while (!network_connect_done()) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    printf("\n\nBasic06 Ready\n") ;

    xTaskCreate(network_service_task, "NetworkService", NETWORKCONN_TASK_STACK_SIZE, NULL, NETWORKCONN_TASK_PRIORITY, NULL);
    while (true) {
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}