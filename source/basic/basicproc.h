#pragma once

#include <stdbool.h>
#include <cy_result.h>
#include "basicline.h"
#include "basicerr.h"

typedef cy_rslt_t (*basic_out_fn_t)(const char *, int) ;

bool basic_line_proc(const char *line, basic_out_fn_t outfn) ;
void basic_prompt(basic_out_fn_t outfn) ;

void basic_destroy_line(basic_line_t *line) ;
const char *basic_token_to_str(uint8_t token) ;

bool basic_proc_load(const char *filename, basic_err_t *err, basic_out_fn_t outfn);
bool basic_proc_save(const char *filename, basic_err_t *err, basic_out_fn_t outfn);