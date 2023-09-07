#pragma once

#include "basicline.h"
#include "basicerr.h"
#include "basicproc.h"

int basic_exec_line(exec_context_t *ctx, basic_out_fn_t outfn) ;
void basic_store_line(basic_line_t *line) ;

void basic_cls(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
void basic_clear(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;

static inline uint32_t getU32(basic_line_t *line, int index)
{
    uint32_t ret = 0 ;

    ret |= (line->tokens_[index++] << 0) ;
    ret |= (line->tokens_[index++] << 8) ;
    ret |= (line->tokens_[index++] << 16) ;
    ret |= (line->tokens_[index++] << 24) ;

    return ret;
}

static inline double getDouble(basic_line_t *line, int index)
{
    double value ;
    double *ptr = &value ;

    for(int i = 0 ;i < sizeof(double) ; i++) {
        *ptr++ = line->tokens_[index++] ;
    }

    return value ;
}
