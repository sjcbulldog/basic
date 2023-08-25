#pragma once

#include "basicline.h"
#include "basicexpr.h"

#define BASIC_OPERAND_TYPE_NONE         (0)
#define BASIC_OPERAND_TYPE_VAR          (1)
#define BASIC_OPERAND_TYPE_CONST        (2)
#define BASIC_OPERAND_TYPE_OPERATOR     (3)

typedef enum operator_type {
    BASIC_OPERATOR_PLUS = 1,
    BASIC_OPERATOR_MINUS = 2,
    BASIC_OPERATOR_TIMES = 3,
    BASIC_OPERATOR_DIVIDE = 4,
} operator_type_t ;

typedef struct operator_table
{
    operator_type_t oper_ ;
    const char *string_ ;
    int prec_ ;
} operator_table_t ;

typedef struct basic_operand basic_operand_t ;

typedef struct basic_operator_args
{
    operator_table_t *operator_ ;
    basic_operand_t *left_ ;
    basic_operand_t *right_ ;
} basic_operator_args_t ;

typedef struct basic_operand
{
    uint8_t type_ ;
    union {
        uint32_t var_ ;
        basic_value_t *const_value_ ;
        basic_operator_args_t operator_args_ ;
    } operand_ ;
} basic_operand_t ;

typedef struct basic_var
{
    char name_[BASIC_MAX_VARIABLE_LENGTH] ;
    uint32_t index_ ;
    uint32_t dimcnt_ ;
    int *dims_;
    basic_value_t *value_ ;
    struct basic_var *next_ ;
} basic_var_t ;

typedef struct basic_expr
{
    basic_operand_t *top_ ;
    int index_ ;
    struct basic_expr *next_ ;
} basic_expr_t ;

