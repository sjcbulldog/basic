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

DSTATUS disk_status(BYTE drv)
{
    DSTATUS stat = RES_OK;

    if (0U == SD_initVar) {
        return STA_NOINIT;
    }

    return stat ;
}  

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE drv)
{
    cy_rslt_t result;

    if (0U == SD_initVar) {
        result = sd_card_init();
        if (CY_RSLT_SUCCESS != result) {
            return STA_NOINIT;
        }

        SD_initVar = 1U;
    }

    return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (BYTE drv, BYTE *buff, LBA_t sector, UINT count)
{
    cy_rslt_t result;

    if (0U == SD_initVar) {
        return RES_NOTRDY;
    }

    result = sd_card_read(sector, buff, (uint32_t *)&count);
    if (CY_RSLT_SUCCESS != result) {
        printf("sd_card_read error: sector=%d count=%d\r\n", (int)sector, (int)count);
        return RES_ERROR;
    }

    return RES_OK ;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (BYTE drv, const BYTE *buff, LBA_t sector, UINT count)
{
    cy_rslt_t result;

    if (0U == SD_initVar) {
        return RES_NOTRDY;
    }

    result = sd_card_write(sector, buff, (uint32_t *)&count);
    if (CY_RSLT_SUCCESS != result) {
        printf("sd_card_write error: sector=%d count=%d\r\n", (int)sector, (int)count);
        return RES_ERROR;
    }

    return RES_OK ;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (BYTE drv, BYTE cmd, void *buff)
{
    DRESULT res = RES_OK ;

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

    return res ;
}

