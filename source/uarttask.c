#include "uarttask.h"
#include "basictask.h"
#include "cyhal.h"
#include <cy_retarget_io.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

static QueueHandle_t inputqueue;
static int cindex = 0;
static bool otherinuse = false;
static char buffer[256];
static char other[256];
static char lf = '\n';
static char delete = 0x7f ;
static char backspace[3] = { 0x08, 0x20, 0x08 } ;

static void send_buffer()
{
    void *addr = &other[0];
    if (!otherinuse)
    {
        strcpy(other, buffer);
        xQueueSendFromISR(inputqueue, &addr, NULL);
    }
    cindex = 0;
}

static void uart_event_handler(void *arg, cyhal_uart_event_t event)
{
    size_t len, wlen;

    if ((event & CYHAL_UART_IRQ_RX_NOT_EMPTY) == CYHAL_UART_IRQ_RX_NOT_EMPTY)
    {
        while (cyhal_uart_readable(&cy_retarget_io_uart_obj) > 0)
        {
            len = 1;

            // Read one character from the UART
            cyhal_uart_read(&cy_retarget_io_uart_obj, &buffer[cindex], &len);

            if (buffer[cindex] == 0x3) {
                // Ctrl-C
                basic_break_isr();
                cindex = 0 ;
            }
            else if (buffer[cindex] == 0x7f || buffer[cindex] == 0x08)
            {
                if (cindex > 0)
                {
                    if (buffer[cindex] == 0x08) 
                    {
                        len = 3;
                        cyhal_uart_write(&cy_retarget_io_uart_obj, &backspace, &len);                        
                    }
                    else 
                    {
                        len = 1;
                        cyhal_uart_write(&cy_retarget_io_uart_obj, &delete, &len);
                    }

                    cindex--;
                }
            }
            else if (buffer[cindex] == 0x15)
            {
                // Ctrl-U
                buffer[cindex] = 0x7f ;
                while (cindex > 0) 
                {
                    len = 1;
                    cyhal_uart_write(&cy_retarget_io_uart_obj, &delete, &len);
                    cindex--;
                }
            }
            else
            {
                cyhal_uart_write(&cy_retarget_io_uart_obj, &buffer[cindex], &len);
                if (buffer[cindex] == '\r')
                {
                    //
                    // If the character is a return, translate to a line feed and echo
                    //
                    wlen = 1;
                    cyhal_uart_write(&cy_retarget_io_uart_obj, &lf, &wlen);
                    buffer[cindex] = '\n';
                }
                
                buffer[cindex + 1] = '\0';
                if (buffer[cindex] == '\n')
                {
                    send_buffer() ;
                }
                else
                {
                    cindex++;
                }
            }
        }
    }
}

static cy_rslt_t uart_output(const char *text, size_t len)
{
    static char cr = '\r';
    size_t wlen ;

    while (len-- > 0)
    {
        do {
            wlen = 1 ;
            cyhal_uart_write(&cy_retarget_io_uart_obj, (void *)text, &wlen);
        } while (wlen == 0) ;
        
        if (*text == '\n')
        {
            do {
                wlen = 1;
                cyhal_uart_write(&cy_retarget_io_uart_obj, (void *)&cr, &wlen);
            }
            while (wlen == 0) ;
        }
        text++;
    }
    return true;
}

void uart_task(void *param)
{
    char *text;

    inputqueue = xQueueCreate(8, sizeof(char *));
    if (inputqueue == NULL)
    {
        printf("Error: could not create basic line queue\n");
        return;
    }

    cyhal_uart_register_callback(&cy_retarget_io_uart_obj, uart_event_handler, NULL);
    cyhal_uart_enable_event(&cy_retarget_io_uart_obj, CYHAL_UART_IRQ_RX_NOT_EMPTY, 3, true);
    basic_prompt(uart_output);

    while (1)
    {
        if (xQueueReceive(inputqueue, &text, (TickType_t)0x7fffffff) == pdPASS)
        {
            basic_task_queue_line(text, uart_output);
            otherinuse = false;
        }
    }
}