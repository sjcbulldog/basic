#include "mystr.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define MY_STR_COUNT            (32)
#define MY_STR_BLOCK_SIZE       (32)

typedef struct one_string
{
    char *string_ ;
    uint32_t index_ ;
    uint32_t allocated_ ;
    bool inuse_ ;
} one_string_t ;

static one_string_t storage[MY_STR_COUNT];

void str_init()
{
    for(int i = 0 ; i < sizeof(storage) / sizeof(storage[0]) ; i++) {
        storage[i].inuse_ = false ;
        storage[i].allocated_ = 0 ;
    }
}

uint32_t str_create()
{
    for(int i = 0 ; i < sizeof(storage) / sizeof(storage[0]) ; i++) {
        if (!storage[i].inuse_) {
            storage[i].inuse_ = true ;
            storage[i].index_ = 0 ;
            return i ;
        }
    }

    return STR_INVALID ;
}

void str_destroy(uint32_t index)
{
    storage[index].inuse_ = false ;
}

bool str_add_int(uint32_t h, int num)
{
    char buf[32] ;
    sprintf(buf, "%d", num) ;
    return str_add_str(h, buf) ;
}

bool str_add_double(uint32_t h, double num)
{
    char buf[32] ;
    sprintf(buf, "%f", num) ;
    return str_add_str(h, buf);
}

extern bool str_add_str(uint32_t h, const char *str)
{
    one_string_t *one = &storage[h] ;

    int len = strlen(str) ;
    int total = one->index_ + len + 1 ;
    if (total > one->allocated_) {
        int size = total / MY_STR_BLOCK_SIZE ;
        if (total % MY_STR_BLOCK_SIZE != 0)
            size++ ;

        if (one->allocated_ == 0) {
            one->string_ = (char *)malloc(size * MY_STR_BLOCK_SIZE);
        }
        else {
            one->string_ = (char *)realloc(one->string_, size * MY_STR_BLOCK_SIZE);
        }

        one->allocated_ = size * MY_STR_BLOCK_SIZE ;

        if (one->string_ == NULL) {
            return false ;
        }
    }

    while (*str) {
        one->string_[one->index_++] = *str++ ;
    }

    return true ;
}

bool str_add_handle(uint32_t h, uint32_t hadd)
{
    if (!str_add_str(h, str_value(hadd)))
        return false ;

    return true ;
}

const char *str_value(uint32_t h)
{
    one_string_t *one = &storage[h] ;
    
    one->string_[one->index_] = '\0';
    return one->string_ ;
}
