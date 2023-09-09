#ifdef DESKTOP
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "basicstr.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifndef DESKTOP
#define _strdup strdup
#endif

#define MY_STR_BLOCK_SIZE       (32)

typedef struct one_string
{
    char *string_ ;
    uint32_t allocated_ ;
    struct one_string *next_ ;
    uint32_t ref_cnt_ ;
    bool frozen_ ;
} one_string_t ;

static one_string_t *string_table = NULL ;

uint32_t basic_str_create_str(const char *strval)
{
    for(one_string_t *one = string_table ; one != NULL ; one = one->next_) {
        if (one->string_ != NULL && strcmp(one->string_, strval) == 0 && one->frozen_) {
            one->ref_cnt_++;
            return (uint32_t)one ;
        }
    }

    one_string_t *str = (one_string_t *)malloc(sizeof(one_string_t)) ;
    if (str == NULL)
        return BASIC_STR_INVALID ;

    str->next_ = string_table ;
    str->string_ = _strdup(strval) ;
    str->allocated_ = (uint32_t)strlen(strval) + 1 ;
    str->frozen_ = true ;
    str->ref_cnt_ = 1 ;
    string_table = str ;
    return (uint32_t)str ;
}

uint32_t basic_str_create()
{
    one_string_t *str = (one_string_t *)malloc(sizeof(one_string_t)) ;
    if (str == NULL)
        return BASIC_STR_INVALID ;

    str->next_ = string_table ;
    str->string_ = NULL ;
    str->allocated_ = 0 ;
    str->frozen_ = false ;
    str->ref_cnt_ = 0xffffffff ;
    string_table = str ;
    return (uint32_t)str ;
}

static void basic_str_destroy_int(one_string_t *str)
{
    if (str == string_table) {
        //
        // It is the first entry in the table
        //
        string_table = string_table->next_ ;
    }
    else {
        one_string_t *var ;
        for(var = string_table ; var != NULL && var->next_ != str ; var = var->next_) /* EMPTY */ ;
        assert(var != NULL) ;
        var->next_ = str->next_ ;
	}

    if (str->string_)
        free(str->string_) ;

    free(str);
}

void basic_str_destroy(uint32_t index)
{
    one_string_t *str = (one_string_t *)index ;

    if (str->frozen_) {
        str->ref_cnt_-- ;
        if (str->ref_cnt_ == 0)
            basic_str_destroy_int(str);
    }
    else {
        basic_str_destroy_int(str) ;
    }
}

void basic_str_clear_all()
{
    while (string_table != NULL) {
        basic_str_destroy_int(string_table);
    }
}

bool basic_str_add_int(uint32_t h, int num)
{
    char buf[32] ;
    sprintf(buf, "%d", num) ;
    return basic_str_add_str(h, buf) ;
}

bool basic_str_add_double(uint32_t h, double num)
{
    char buf[32] ;
    sprintf(buf, "%f", num) ;
    return basic_str_add_str(h, buf);
}

extern bool basic_str_add_str(uint32_t h, const char *str)
{
    one_string_t *one = (one_string_t *)h ;

    assert(one->frozen_ == false) ;
    uint32_t needed = (uint32_t)((one->string_ ? strlen(one->string_) : 0) + strlen(str) + 1);
    if (needed > one->allocated_) {
        int size = needed / MY_STR_BLOCK_SIZE ;
        if ((needed % MY_STR_BLOCK_SIZE) != 0)
            size++ ;

        if (one->allocated_ == 0) {
            one->string_ = (char *)malloc(size * MY_STR_BLOCK_SIZE);
        }
        else {
            one->string_ = (char *)realloc(one->string_, size * MY_STR_BLOCK_SIZE);
        }

        if (one->string_ == NULL) {
            one->allocated_ = 0 ;
            one->string_ = NULL ;
        }
        else {
            if (one->allocated_ == 0) {
                strcpy(one->string_, str) ;
            }
            else {
                strcat(one->string_, str);
            }
            one->allocated_ = size * MY_STR_BLOCK_SIZE ;
        }
    }
    else {
        strcat(one->string_, str) ;
    }

    return true ;
}

bool basic_str_add_handle(uint32_t h, uint32_t hadd)
{
    if (!basic_str_add_str(h, basic_str_value(hadd)))
        return false ;

    return true ;
}

const char *basic_str_value(uint32_t h)
{
    one_string_t *one = (one_string_t *)h ;
    return one->string_ ;
}

uint32_t basic_str_memsize(bool overhead)
{
    uint32_t ret = 0 ;
    one_string_t *t ;

    for(t = string_table ; t != NULL ; t = t ->next_) {
        if (overhead) {
            ret += t->allocated_ ;
            ret += sizeof(one_string_t) ;
        }
        else {
            ret += (uint32_t)strlen(t->string_) + 1 ;
        }
    }

    return ret ;
}
