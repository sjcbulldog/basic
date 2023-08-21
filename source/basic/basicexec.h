#pragma once

#include "basicline.h"
#include "basicerr.h"

int basic_exec_line(basic_line_t *line) ;
void basic_store_line(basic_line_t *line) ;

basic_line_t *basic_cls(basic_line_t *line, basic_err_t *err) ;
basic_line_t *basic_clear(basic_line_t *line, basic_err_t *err) ;

static inline uint32_t getU32(basic_line_t *line, int index)
{
    uint32_t ret = 0 ;

    ret |= (line->tokens_[index++] << 0) ;
    ret |= (line->tokens_[index++] << 8) ;
    ret |= (line->tokens_[index++] << 16) ;
    ret |= (line->tokens_[index++] << 24) ;

    return ret;
}
