#include "cyhal.h"
#include "cybsp.h"
#include <FreeRTOS.h>
#include <semphr.h>

#include "ff.h"
#include "USB.h"
#include "USB_MSD.h"
#include "cycfg_emusbdev.h"
#include <stdio.h>

SemaphoreHandle_t rtos_fs_mutex;
extern FATFS SDFatFs;

/*****************************************************************************
* Macros
*****************************************************************************/
#define BUFFER_SIZE       8192U


/*****************************************************************************
* Static data
*****************************************************************************/
static uint32_t sector_buffer[BUFFER_SIZE / 4];     /* Used as sector buffer in order to do read/write sector bursts (~8 sectors at once) */


/*****************************************************************************
* Function Name: usb_add_msd
******************************************************************************
* Summary:
*  Add MSD device to the USB Stack and the logical unit.
*
* Parameters:
*  None
*
* Return:
*  None
*
*****************************************************************************/
static void usb_add_msd(void)
{
    static U8            usb_out_buffer[USB_FS_BULK_MAX_PACKET_SIZE];

    USB_MSD_INIT_DATA    InitData;
    USB_MSD_INST_DATA    InstData;
    USB_ADD_EP_INFO      EPIn;
    USB_ADD_EP_INFO      EPOut;

    memset(&InitData, 0, sizeof(InitData));
    EPIn.Flags           = 0;                             /* Flags not used */
    EPIn.InDir           = USB_DIR_IN;                    /* IN direction (Device to Host) */
    EPIn.Interval        = 0;
    EPIn.MaxPacketSize   = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64 for Bulk in full-speed) */
    EPIn.TransferType    = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPIn        = USBD_AddEPEx(&EPIn, NULL, 0);

    EPOut.Flags          = 0;                             /* Flags not used */
    EPOut.InDir          = USB_DIR_OUT;                   /* OUT direction (Host to Device) */
    EPOut.Interval       = 0;
    EPOut.MaxPacketSize  = USB_FS_BULK_MAX_PACKET_SIZE;   /* Maximum packet size (64 for Bulk out full-speed) */
    EPOut.TransferType   = USB_TRANSFER_TYPE_BULK;        /* Endpoint type - Bulk */
    InitData.EPOut       = USBD_AddEPEx(&EPOut, usb_out_buffer, sizeof(usb_out_buffer));

    /* Add MSD device */
    USBD_MSD_Add(&InitData);

    /* Add logical unit 0 */
    memset(&InstData, 0, sizeof(InstData));
    InstData.pAPI                       = &USB_MSD_StorageByName;
    InstData.DriverData.pStart          = (void *)"";
    InstData.DriverData.pSectorBuffer   = sector_buffer;
    InstData.DriverData.NumBytes4Buffer = sizeof(sector_buffer);
    InstData.pLunInfo                   = &usb_lun0Info;
    USBD_MSD_AddUnit(&InstData);
}

/*****************************************************************************
* Function Name: usb_comm_init
******************************************************************************
* Summary:
*  Initializes the USB hardware block and emUSB-Device MSD class.
*
* Parameters:
*  None
*
* Return:
*  None
*
*****************************************************************************/
void usb_comm_init(void)
{
    /* Initialize the USB Device stack */
    USBD_Init();

    /* Add mass storage device to USB stack */
    usb_add_msd();

    /* Set USB device information for enumeration */
    USBD_SetDeviceInfo(&usb_deviceInfo);

    /* Start the USB device stack */
    USBD_Start();
}

void usb_task(void *arg)
{
    rtos_fs_mutex = xSemaphoreCreateMutex() ;

    USBD_SetLogFilter(USB_MTYPE_INFO) ;

    if (NULL != rtos_fs_mutex)
    {
        /* Check if other tasks are accessing the file system by trying to obtain the mutex */
        if (pdTRUE == xSemaphoreTake(rtos_fs_mutex, portMAX_DELAY))
        {
            /* Initialize and enumerate the USB */
            usb_comm_init();

            /* Release the mutex so that file system can be used by other tasks */
            xSemaphoreGive(rtos_fs_mutex);
        }
    }
    while (1)
    {
        if (NULL != rtos_fs_mutex)
        {
            /* Check if other tasks are accessing the file system by trying to obtain the mutex */
            if (pdTRUE == xSemaphoreTake(rtos_fs_mutex, portMAX_DELAY))
            {
                /* check for configuration */
                if ((USBD_GetState() & (USB_STAT_CONFIGURED | USB_STAT_SUSPENDED)) == USB_STAT_CONFIGURED)
                {
                    USBD_MSD_Poll();
                }

                /* Release the mutex so that file system can be used by other tasks */
                xSemaphoreGive(rtos_fs_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}