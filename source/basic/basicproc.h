#pragma once

#include <stdbool.h>
#include "basicline.h"
#include "basicerr.h"

bool basic_line_proc(const char *line) ;

void basic_destroy_line(basic_line_t *line) ;
const char *basic_token_to_str(uint8_t token) ;
bool basic_proc_load(const char *filename, basic_err_t *);
bool basic_proc_save(const char *filename, basic_err_t *);