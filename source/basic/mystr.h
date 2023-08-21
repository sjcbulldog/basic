#pragma once

#include <stdint.h>
#include <stdbool.h>

#define STR_INVALID ((uint32_t) -1)

extern void str_init() ;
extern uint32_t str_create() ;
extern void str_destroy(uint32_t h) ;
extern bool str_add_int(uint32_t h, int num) ;
extern bool str_add_double(uint32_t h, double num) ;
extern bool str_add_str(uint32_t h, const char *) ;
extern bool str_add_handle(uint32_t h, uint32_t add) ;
extern const char *str_value(uint32_t h) ;
