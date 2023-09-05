#pragma once

#include <FreeRTOS.h>
#include <semphr.h>

extern SemaphoreHandle_t rtos_fs_mutex;
extern void usb_task(void *arg) ;
