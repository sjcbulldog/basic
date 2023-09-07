#include "basictask.h"
#include "basicproc.h"
#include <FreeRTOS.h>
#include <queue.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct queue_entry
{
    char *line_ ;
    basic_out_fn_t outfn_ ;
} queue_entry_t ;

static bool store_input_ = false ;
static QueueHandle_t line_queue ;

void basic_task_store_input(bool enabled)
{
    queue_entry_t *entry ;

    store_input_ = enabled ;

    //
    // Flush any existing lines
    //
    while (uxQueueMessagesWaiting(line_queue) > 0) {
        xQueueReceive(line_queue, &entry, (TickType_t)0x7fffffff) ;
    }
}

char *basic_task_get_line()
{
    char *ret = NULL ;
    queue_entry_t *entry ;

    if (xQueueReceive(line_queue, &entry, (TickType_t)0x7fffffff) == pdPASS) {
        ret = entry->line_ ;
        free(entry) ;
    }

    return ret ;
}

void basic_task(void *param)
{
    queue_entry_t *entry ;

    line_queue = xQueueCreate(8, sizeof(queue_entry_t)) ;
    if (line_queue == NULL) {
        printf("Error: could not create basic line queue\n") ;
        return ;
    }

    while (true) {
        if (store_input_) {
            vTaskDelay(100 / portTICK_PERIOD_MS) ;
        }
        else if (xQueueReceive(line_queue, &entry, (TickType_t)0x7fffffff) == pdPASS) {
            basic_line_proc(entry->line_, entry->outfn_);
            free(entry->line_) ;
            free(entry);
            basic_prompt(entry->outfn_);
        }
    }
}

bool basic_task_queue_line(const char *line, basic_out_fn_t outfn)
{
    queue_entry_t *entry = (queue_entry_t *)malloc(sizeof(queue_entry_t)) ;
    if (entry == NULL) {
        return false ;
    }
    entry->line_ = strdup(line) ;
    if (entry->line_ == NULL) {
        free(entry) ;
        return false ;
    }
    entry->outfn_ = outfn ;

    while (true) {
        if (xQueueSend(line_queue, &entry, 0) == pdPASS)
            break ;

        vTaskDelay(200 / portTICK_PERIOD_MS) ;
    }

    return true ;
}