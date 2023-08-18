#include "initfs.h"
#include "ff.h"
#include "cybsp.h"
#include "cyhal.h"
#include <FreeRTOS.h>
#include <task.h>
#include <stdbool.h>
#include <stdio.h>

static BYTE work[FF_MAX_SS];
static FATFS SDFatFs;
static const char *DRIVE_LABEL_NAME = "Basic06" ;
static bool isDone = false ;

static bool init_file_system(bool forcefmt)
{
    bool ret = true ;

    FRESULT result;
    const MKFS_PARM fs_param =
    {
        .fmt = FM_FAT32,  /* Format option */
        .n_fat = 1,       /* Number of FATs */
        .align = 0,
        .n_root = 0,
        .au_size = 0
    };

    if (forcefmt)
    {
        printf("  Formatting file system...\n");
        result = f_mkfs("", &fs_param, work, sizeof(work));
        if (FR_OK == result)
        {
            f_mount(&SDFatFs, "", 1);
            f_setlabel(DRIVE_LABEL_NAME);
        }
        else
        {
            printf("    not able to create a file system!\n");
            return false ;
        }
    }

    /* Attempt to mount the external memory */
    if (!forcefmt)
        printf("  checking existing file system\n");

    result = f_mount(&SDFatFs, "", 1);

    switch (result)
    {
        case FR_OK:
            if (!forcefmt)
                printf("  existing file system valid\n") ;
            break ;

        case FR_NO_FILESYSTEM:
            /* No file system, create a FAT system */
            printf("  no file system, formatting SD card\n");
            result = f_mkfs("", &fs_param, work, sizeof(work));
            if (FR_OK == result)
            {
                f_mount(&SDFatFs, "", 1);
                f_setlabel(DRIVE_LABEL_NAME);
                printf("  file system created\n") ;
            }
            else
            {
                printf("    error - not able to create a file system!\n");
                ret = false ;
            }
            break;

        case FR_NOT_READY:
            printf("    SD Card not present! Insert one to the SD card slot\n");
            ret = false ;
            break;

        default:
            ret = false ;
            break;
    }

    return ret ;
}

void filesystem_init_task(void *param)
{
    isDone = false ;

    printf("  Initializing file system ...\n") ;
    init_file_system(false) ;

    isDone = true ;
    while (1) {
        vTaskDelay(10000000) ;
    }
}

bool filesystem_init_done()
{
    return isDone ;
}