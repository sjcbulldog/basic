#pragma once

#include "basicline.h"
#include "basicexpr.h"
#include "basiccfg.h"

#define BASIC_OPERAND_TYPE_VAR          (1)
#define BASIC_OPERAND_TYPE_CONST        (2)
#define BASIC_OPERAND_TYPE_OPERATOR     (3)
#define BASIC_OPERAND_TYPE_FUNCTION     (4)
#define BASIC_OPERAND_TYPE_USERFN       (5)
#define BASIC_OPERAND_TYPE_BOUNDV       (6)

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

typedef struct basic_var_args
{
    uint32_t varindex_ ;
    int dimcnt_ ;
    basic_operand_t** dims_;
} basic_var_args_t ;

typedef struct function_table
{
    int num_args_;
    const char* string_;
    basic_value_t* (*eval_)(int count, basic_value_t **args, basic_err_t *err);
} function_table_t;

typedef struct basic_expr_user_fn
{
    uint32_t index_;
    char* name_;
    uint32_t argcnt_;
    char** args_;
    uint32_t expridx_;
    struct basic_expr_user_fn* next_;
} basic_expr_user_fn_t ;

typedef struct basic_function_args
{
    function_table_t* func_;
    basic_operand_t** args_;
} basic_function_args_t ;

typedef struct user_function_args
{
    basic_expr_user_fn_t* func_;
    basic_operand_t** args_;
} user_function_args_t ;

typedef struct basic_operand
{
    uint8_t type_ ;
    union {
        basic_var_args_t var_ ;
        basic_value_t *const_value_ ;
        basic_operator_args_t operator_args_ ;
        basic_function_args_t function_args_;
        user_function_args_t userfn_args_ ;
        const char *boundv_ ;
    } operand_ ;
} basic_operand_t ;

typedef struct basic_var
{
    char name_[BASIC_MAX_VARIABLE_LENGTH] ;
    uint32_t index_ ;
    uint32_t dimcnt_ ;
    uint32_t *dims_;
    basic_value_t *value_ ;
    double *darray_ ;
    char **sarray_ ;
    struct basic_var *next_ ;
} basic_var_t ;

typedef struct basic_expr
{
    basic_operand_t *top_ ;
    uint32_t index_ ;
    struct basic_expr *next_ ;
} basic_expr_t ;

typedef struct expr_ctxt
{
    int operand_top_;
    int operator_top_;
    basic_operand_t* operands_[BASIC_MAX_EXPR_DEPTH];
    basic_operand_t* operators_[BASIC_MAX_EXPR_DEPTH];
    char parsebuffer[BASIC_PARSE_BUFFER_LENGTH];
    int argcnt_;
    char** argnames_;
} expr_ctxt_t;

