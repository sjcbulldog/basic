/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various existing       */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"               /* Obtains integer types */
#include "diskio.h"          /* Declarations of disk functions */
#include "cyhal.h"
#include "sd_card.h"
#include <stdio.h>

/* Definitions of physical drive number for each drive */
#define DEV_RAM          1     /* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC          0     /* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB          2     /* Example: Map USB MSD to physical drive 2 */
/* SD initialization flag */
uint8_t SD_initVar = 0U;


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
     BYTE pdrv          /* Physical drive nmuber to identify the drive */
)
{
     DSTATUS stat = RES_OK;

     switch (pdrv) {
     //case DEV_RAM :
          //result = RAM_disk_status();

          // translate the reslut code here

          //return stat;

     case DEV_MMC :
          //result = MMC_disk_status();

          
        if(0U == SD_initVar) {
          return STA_NOINIT;
          }
    // translate the reslut code here
     return stat;     

     //case DEV_USB :
          //result = USB_disk_status();

          // translate the reslut code here

          //return stat;
     }
     return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
     BYTE pdrv                    /* Physical drive nmuber to identify the drive */
)
{
    DSTATUS stat = RES_OK;
    cy_rslt_t result;

     switch (pdrv) {
     //case DEV_RAM :
          //result = RAM_disk_initialize();

          // translate the reslut code here

          //return stat;

     case DEV_MMC :
             if (0U == SD_initVar) {
            /* Initialize the SD card */
          //result = MMC_disk_initialize();
            result = sd_card_init();
               /* translate the result code here */

            if (CY_RSLT_SUCCESS != result) {
                return STA_NOINIT;
            }
            SD_initVar = 1U;
        }

          
          return stat;

     //case DEV_USB :
          //result = USB_disk_initialize();

          // translate the reslut code here

          //return stat;
     }
     return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
     BYTE pdrv,          /* Physical drive nmuber to identify the drive */
     BYTE *buff,          /* Data buffer to store read data */
     LBA_t sector,     /* Start sector in LBA */
     UINT count          /* Number of sectors to read */
)
{
     DRESULT res = RES_OK;
     cy_rslt_t result;

     switch (pdrv) {
     //case DEV_RAM :
          // translate the arguments here

          //result = RAM_disk_read(buff, sector, count);

          // translate the reslut code here

          //return res;

     case DEV_MMC :
             if (0U == SD_initVar) {
            return RES_NOTRDY;
        }
          // translate the arguments here

          //result = MMC_disk_read(buff, sector, count);
        result = sd_card_read(sector, buff, (uint32_t *)&count);
          
          /* translate the result code here */
        if (CY_RSLT_SUCCESS != result) {
            printf("sd_card_read error: sector=%d count=%d\r\n", (int)sector, (int)count);
            return RES_ERROR;
        }
          // translate the reslut code here

          return res;

     //case DEV_USB :
          // translate the arguments here

          //result = USB_disk_read(buff, sector, count);

          // translate the reslut code here

          //return res;
     }

     return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
     BYTE pdrv,               /* Physical drive nmuber to identify the drive */
     const BYTE *buff,     /* Data to be written */
     LBA_t sector,          /* Start sector in LBA */
     UINT count               /* Number of sectors to write */
)
{
    DRESULT res = RES_OK;
    cy_rslt_t result;

     switch (pdrv) {
     //case DEV_RAM :
          // translate the arguments here

          //result = RAM_disk_write(buff, sector, count);

          // translate the reslut code here

          //return res;

     case DEV_MMC :
             if (0U == SD_initVar) {
            return RES_NOTRDY;
        }
          // translate the arguments here

          //result = MMC_disk_write(buff, sector, count);
         result = sd_card_write(sector, buff, (uint32_t *)&count);
        if (CY_RSLT_SUCCESS != result) {
            printf("sd_card_write error: sector=%d count=%d\r\n", (int)sector, (int)count);
            return RES_ERROR;
        }

          // translate the reslut code here

          return res;

     //case DEV_USB :
          // translate the arguments here

          //result = USB_disk_write(buff, sector, count);

          // translate the reslut code here

          //return res;
     }

     return RES_PARERR;
}

#endif



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
     BYTE pdrv,          /* Physical drive nmuber (0..) */
     BYTE cmd,          /* Control code */
     void *buff          /* Buffer to send/receive control data */
)
{
    DRESULT res = RES_OK;

     switch (pdrv) {
     //case DEV_RAM :

          // Process of the command for the RAM drive

          //return res;

     case DEV_MMC :
             if (0U == SD_initVar) {
            return RES_NOTRDY;
        }
          // Process of the command for the MMC/SD card
        switch(cmd) {
            case CTRL_SYNC:
                break;
            case GET_SECTOR_COUNT: /* Get media size */
                *(DWORD *) buff = sd_card_max_sector_num();
                break;
            case GET_SECTOR_SIZE: /* Get sector size */
                *(WORD *) buff = sd_card_sector_size();
                break;
            case GET_BLOCK_SIZE: /* Get erase block size */
                *(DWORD *) buff = 8;
                break;
            default:
                res = RES_PARERR;
                break;
        }

          

          return res;

     //case DEV_USB :

          // Process of the command the USB drive

          //return res;
     }

     return RES_PARERR;
}

