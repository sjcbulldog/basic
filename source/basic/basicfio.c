#include "basicexec.h"
#include "basicerr.h"
#include "basicproc.h"
#include "basicexpr.h"
#include "basicstr.h"
#include <FreeRTOS.h>
#include <ff.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <FreeRTOS.h>
#include <semphr.h>

#define USE_LOCKS

extern basic_line_t *program ;
extern uint32_t lineToString(basic_line_t *line) ;

static char filename[64] ;
static char filename2[64] ;

extern SemaphoreHandle_t rtos_fs_mutex;
extern void basic_clear_int() ;

#ifdef USE_LOCKS
static bool lockfs(basic_err_t *err)
{
    if (rtos_fs_mutex != NULL) {
        //
        // If the Mass Storage task is running, this will be non-null
        //

        if (pdTRUE != xSemaphoreTake(rtos_fs_mutex, portMAX_DELAY)) {
            *err = BASIC_ERR_IO_ERROR ;
            return false ;
        }
    }

    return true ;
}

static bool unlockfs(basic_err_t *err)
{
    if (pdTRUE != xSemaphoreGive(rtos_fs_mutex)) {
        *err = BASIC_ERR_IO_ERROR ;
        return false ;
    }

    return true ;
}
#else
static bool lockfs(basic_err_t *err)
{
    return true ;
}

static bool unlockfs(basic_err_t *err)
{
    return true ;
}
#endif

void basic_save(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    FIL fp ;
    FRESULT res ;
    UINT written ;

    *err = BASIC_ERR_NONE ;
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }    

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr, 0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    if (!lockfs(err))
        return ;

    res = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return ;
    }

    basic_line_t *pgm = program ;
    while (pgm)
    {
        uint32_t str = lineToString(pgm);
        if (str == BASIC_STR_INVALID) {
            f_close(&fp) ;
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return ;
        }

        pgm = pgm->next_ ;

        if (!basic_str_add_str(str, "\n")) {
            f_close(&fp) ;
            basic_str_destroy(str);
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return ;                
        }        

        const char *strval = basic_str_value(str) ;
        UINT towrite = strlen(strval) ;
        res = f_write(&fp, strval, towrite, &written) ;
        basic_str_destroy(str) ;
        if (res != FR_OK || towrite != written) {
            f_close(&fp) ;
            *err = BASIC_ERR_IO_ERROR ;
            return ;
        }
    }

    f_close(&fp) ;
    unlockfs(err) ;
}

void basic_load(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr,  0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    basic_clear_int() ;

    if (!lockfs(err))
        return ;

    basic_proc_load(filename, err, outfn) ;

    unlockfs(err) ;
}

void basic_flist(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    DIR dp ;
    FILINFO info ;
    const int tabno = 16 ;
    char outline[64], num[12] ;

    *err = BASIC_ERR_NONE ;    

    if (!lockfs(err))
        return ;

    FRESULT res = f_opendir (&dp, "/") ;
    if (res != FR_OK) {
        *err = BASIC_ERR_SDCARD_ERROR ;
        if (rtos_fs_mutex != NULL) {
            xSemaphoreGive(rtos_fs_mutex);
        }  
        return ;
    }

    strcpy(outline, "FileName        Size\n");
    (*outfn)(outline, strlen(outline));
    strcpy(outline, "============================\n") ;
    (*outfn)(outline, strlen(outline));

    const char *name ;
    while (true) {
        res = f_readdir (&dp, &info);
        if (res != FR_OK || info.fname[0] == '\0')
            break;

        const char *ext = strrchr(info.fname, '.') ;
        if (strcasecmp(ext, ".bas") != 0)
            continue ;

        name = info.fname ;
        if (name[0] == '\0')
            name = info.altname ;

        int index = 0 ;
        int src = 0 ;
        while (name[src] != '\0')
            outline[index++] = name[src++] ;
        
        while(index < tabno)
            outline[index++] = ' ' ;

        sprintf(num, "%ld", info.fsize);
        src = 0 ;
        while (num[src] != '\0')
            outline[index++] = num[src++] ;
       
        outline[index++] = '\n' ;
        outline[index] = '\0' ;

        (*outfn)(outline, strlen(outline));
    }

    (*outfn)("\n", 1);
    f_closedir(&dp);

    unlockfs(err) ;    
}

void basic_del(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr,  0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    if (!lockfs(err))
        return ;

    if (f_unlink(filename) != FR_OK) {
        *err = BASIC_ERR_IO_ERROR ;
    }

    unlockfs(err) ;
}

void basic_rename(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr,  0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    expr = getU32(line, 5);
    value = basic_expr_eval(expr,  0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcpy(filename2, "/") ;
    strcat(filename2, value->value.svalue_);

    if (!lockfs(err))
        return ;

    if (f_rename(filename, filename2) != FR_OK) {
        *err = BASIC_ERR_IO_ERROR ;
    }

    unlockfs(err);
}
