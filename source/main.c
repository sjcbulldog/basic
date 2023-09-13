
#include "initfs.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "ctrltask.h"
#include "basicstr.h"

/* RTOS header file. */
#include <FreeRTOS.h>
#include <task.h>

/* TCP client task header file. */
/*******************************************************************************
* Macros
********************************************************************************/
/* RTOS related macros. */


/*******************************************************************************
* Global Variables
********************************************************************************/
/* This enables RTOS aware debugging. */
volatile int uxTopUsedPriority;

int main()
{
    cy_rslt_t result;

    /* This enables RTOS aware debugging in OpenOCD. */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    /* Initialize the board support package. */
    result = cybsp_init() ;
    CY_ASSERT(result == CY_RSLT_SUCCESS);

    /* To avoid compiler warnings. */
    (void) result;

    /* Enable global interrupts. */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port. */
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);

    // Initialize the User LED
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("Basic06 Initializing\n");

    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);

    /* Create the tasks. */
    xTaskCreate(control_task, "ControlTask", 1024, NULL, 6, NULL);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);
}

 /* [] END OF FILE */
