#pragma once

#include <stdint.h>
#include <stdbool.h>

#define BASIC_STR_INVALID ((uint32_t) -1)

extern uint32_t basic_str_create() ;
extern uint32_t basic_str_create_str(const char *strval) ;
extern void basic_str_destroy(uint32_t h) ;
extern bool basic_str_add_int(uint32_t h, int num) ;
extern bool basic_str_add_double(uint32_t h, double num) ;
extern bool basic_str_add_str(uint32_t h, const char *) ;
extern bool basic_str_add_handle(uint32_t h, uint32_t add) ;
extern const char *basic_str_value(uint32_t h) ;
extern uint32_t basic_str_memsize(bool overhead) ;
extern void basic_str_clear_all() ;