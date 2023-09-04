#include <stdio.h>
#include <cy_result.h>
#include "basicexec.h"
#include "basicerr.h"
#include "basicproc.h"
#include "basicexpr.h"
#include "basicstr.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

extern basic_line_t *program ;

extern uint32_t lineToString(basic_line_t *line) ;
bool basic_line_proc(const char *line, basic_out_fn_t outfn) ;

static char filename[64] ;
static char buffer[512] ;

bool basic_proc_load(const char *fname, basic_err_t *err, basic_out_fn_t outfn)
{
    FILE *fp = fopen(fname, "r") ;

    if (fp == NULL) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return false ;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (!basic_line_proc(buffer, infn, outfn)) {
            basic_clear(NULL, err, outfn) ;
            return false ;
        }
    }

    fclose(fp) ;
    return true;
}

void basic_save(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    FILE *fp ;

    *err = BASIC_ERR_NONE ;
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }    

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcat(filename, value->value.svalue_);

    fp = fopen(filename, "w");
    if (fp == NULL) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return ;
    }

    basic_line_t *pgm = program ;
    while (pgm)
    {
        uint32_t str = lineToString(pgm);
        if (str == BASIC_STR_INVALID) {
            fclose(fp) ;
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return ;
        }

        pgm = pgm->next_ ;

        if (!basic_str_add_str(str, "\n")) {
            fclose(fp) ;
            basic_str_destroy(str);
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return ;                
        }        

        const char *strval = basic_str_value(str) ;
        fputs(strval, fp) ;
        basic_str_destroy(str) ;
    }
    fclose(fp) ;
}

void basic_load(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;

    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_expr_eval(expr, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    strcat(filename, value->value.svalue_);
    basic_proc_load(filename, err, outfn) ;
}

void basic_flist(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
}

static cy_rslt_t outfn(const char *text, int size)
{
    fwrite(text, 1, size, stdout) ;

    return 0;
}

static char inbuf[512] ;
int main(int ac, char **av)
{
    ac-- ;
    av++ ;

    FILE *f = fopen(*av, "r") ;
    if (f == NULL) {
        printf("File '%s' not found\n", *av) ;
        return 1 ;
    }

    while (1)
    {
        if (fgets(inbuf, sizeof(inbuf), f) == NULL)
            break; 

        if (!basic_line_proc(inbuf, outfn))
            break;
    }

    fclose(f) ;
    return 0 ;
}
