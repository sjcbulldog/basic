#pragma once

#include "basicproc.h"

extern void basic_task(void *param) ;
extern bool basic_task_queue_line(const char *, basic_out_fn_t) ;
extern void basic_task_store_input(bool enabled) ;
extern char *basic_task_get_line() ;