#pragma once

#include <stdbool.h>
#include <cy_result.h>
#include <string.h>
#include "basicline.h"
#include "basicerr.h"

typedef cy_rslt_t (*basic_out_fn_t)(const char *, size_t) ;

extern bool basic_line_proc(const char *line, basic_out_fn_t outfn) ;
extern void basic_prompt(basic_out_fn_t outfn) ;

extern void basic_destroy_line(basic_line_t *line) ;
extern const char *basic_token_to_str(uint8_t token) ;

extern bool basic_proc_load(const char *filename, basic_err_t *err, basic_out_fn_t outfn);
extern bool basic_proc_save(const char *filename, basic_err_t *err, basic_out_fn_t outfn);

extern bool basic_is_keyword(const char *line) ;