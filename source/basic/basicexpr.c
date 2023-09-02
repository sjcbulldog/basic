#include "basicexpr.h"
#include "basicexprint.h"
#include "basiccfg.h"
#include "basicstr.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <math.h>

#ifndef DESKTOP
#define _stricmp strcasecmp
#define _strdup strdup
#include <cyhal.h>
static bool crypto_inited = false ;
static cyhal_trng_t trng_obj ;
#endif

static basic_expr_user_fn_t* get_user_fn_from_name(const char *name) ;
static basic_value_t *eval_node(basic_operand_t *op, int cntv, char **names, basic_value_t **values, basic_err_t *err) ;
static const char* parse_operand_top(const char* line, int argcntg, char **argnames, basic_operand_t** ret, basic_err_t* err);
static bool basic_operand_to_string(basic_operand_t* parent, basic_operand_t* oper, uint32_t str);

static uint32_t next_var_index = 1 ;
static basic_var_t *vars = NULL ;

static uint32_t next_expr_index = 1 ;
static basic_expr_t *exprs = NULL ;

static  uint32_t next_user_fn_index = 1;
static basic_expr_user_fn_t* userfns = NULL;

operator_table_t operators[] = 
{
    { BASIC_OPERATOR_PLUS, "+" , 4},
    { BASIC_OPERATOR_MINUS, "-", 4 },
    { BASIC_OPERATOR_TIMES, "*", 3 },
    { BASIC_OPERATOR_DIVIDE, "/", 3 },
    { BASIC_OPERATOR_POWER, "^", 2 },
    { BASIC_OPERATOR_NOT_EQUAL, "<>", 5},
    { BASIC_OPERATOR_EQUAL, "=", 5 },
    { BASIC_OPERATOR_GREATER, ">", 5 },
    { BASIC_OPERATOR_GREATER_EQ, ">", 5 },
    { BASIC_OPERATOR_LESS, "<", 5 },
    { BASIC_OPERATOR_LESS_EQ, "<=", 5 },
    { BASIC_OPERATOR_OR, "OR", 6 },
    { BASIC_OPERATOR_AND, "AND", 6 },
} ;

static basic_value_t* func_int(int count, basic_value_t** args, basic_err_t *err);
static basic_value_t* func_rnd(int count, basic_value_t** args, basic_err_t *err);
static basic_value_t *func_mem(int count, basic_value_t** args, basic_err_t *err) ;
static basic_value_t *func_sqrt(int count, basic_value_t** args, basic_err_t *err) ;

function_table_t functions[] =
{
    { 1, "INT", func_int },
    { 1, "RND", func_rnd },
    { 1, "MEM", func_mem },
    { 1, "SQRT", func_sqrt},
};

const char* basic_expr_parse_dims_const(const char* line, uint32_t* dimcnt, uint32_t* dims, basic_err_t* err)
{
    *dimcnt = 0;

    line = skipSpaces(line);
    if (*line != '(') {
        *err = BASIC_ERR_EXPECTED_OPENPAREN;
        return NULL;
    }
    line++;

    while (true) {
        if (*dimcnt == BASIC_MAX_DIMS) {
            *err = BASIC_ERR_TOO_MANY_DIMS;
            return NULL;
        }
        line = basic_expr_parse_int(line, (int *)&dims[*dimcnt], err);
        if (line == NULL)
            return NULL;

        if (dims[*dimcnt] < 0 || dims[*dimcnt] > BASIC_MAX_SINGLE_DIM) {
            *err = BASIC_ERR_INVALID_DIMENSION;
            return NULL;
        }

        (*dimcnt)++;

        line = skipSpaces(line);

        if (*line == ',') {
            // More dimensions
            line++;
        }
        else if (*line == ')') {
            // End of the dimensions
            line++;
            break;
        }
        else {
            *err = BASIC_ERR_INVALID_CHAR;
            return NULL;
        }
    }

    return line;
}

const char* basic_expr_parse_dims_expr(const char* line, int* dimcnt, uint32_t *dims, basic_err_t* err)
{
    *dimcnt = 0;

    line = skipSpaces(line);
    if (*line != '(') {
        *err = BASIC_ERR_EXPECTED_OPENPAREN;
        return NULL;
    }
    line++;

    while (true) {
        if (*dimcnt == BASIC_MAX_DIMS) {
            *err = BASIC_ERR_TOO_MANY_DIMS;
            return NULL;
        }
        line = basic_expr_parse(line, 0, NULL, &dims[*dimcnt], err);
        if (line == NULL)
            return NULL;

        (*dimcnt)++;

        line = skipSpaces(line);

        if (*line == ',') {
            // More dimensions
            line++;
        }
        else if (*line == ')') {
            // End of the dimensions
            line++;
            break;
        }
        else {
            *err = BASIC_ERR_INVALID_CHAR;
            return NULL;
        }
    }

    return line;
}

const char* basic_expr_parse_dims_operands(const char* line, uint32_t* dimcnt, basic_operand_t** dims, basic_err_t* err)
{
    *dimcnt = 0;

    line = skipSpaces(line);
    if (*line != '(') {
        *err = BASIC_ERR_EXPECTED_OPENPAREN;
        return NULL;
    }
    line++;

    while (true) {
        if (*dimcnt == BASIC_MAX_DIMS) {
            *err = BASIC_ERR_TOO_MANY_DIMS;
            return NULL;
        }
        line = parse_operand_top(line, 0, NULL, &dims[*dimcnt], err);
        if (line == NULL)
            return NULL;

        (*dimcnt)++;

        line = skipSpaces(line);

        if (*line == ',') {
            // More dimensions
            line++;
        }
        else if (*line == ')') {
            // End of the dimensions
            line++;
            break;
        }
        else {
            *err = BASIC_ERR_INVALID_CHAR;
            return NULL;
        }
    }

    return line;
}

const char *basic_expr_parse_int(const char *line, int *value, basic_err_t *err)
{
    *err = BASIC_ERR_NONE ;
    static char parsebuffer[32];

    line = skipSpaces(line) ;
    if (!isdigit((int)*line) && *line != '+' && *line != '-') {
        *err = BASIC_ERR_BAD_INTEGER_VALUE ;
        return NULL ;
    }

    bool digit = false ;
    int index = 0 ;
    if (*line == '-') {
        parsebuffer[index++] = *line++ ;
    }
    else if (*line == '+') {
        line++ ;
    }

    while (isdigit((int)*line)) {
        digit = true ;
        parsebuffer[index++] = *line++ ;
    }

    if (!digit) {
        *err = BASIC_ERR_BAD_INTEGER_VALUE ;
        return NULL ;
    }

    parsebuffer[index] = '\0' ;
    *value = atoi(parsebuffer) ;
    return line ;
}

bool basic_destroy_value(basic_value_t *value)
{
    if (value->type_ == BASIC_VALUE_TYPE_STRING) {
        free(value->value.svalue_);
    }

    free(value) ;

    return true ;
}

static basic_value_t *create_number_value(double v)
{
    basic_value_t *ret = (basic_value_t *)malloc(sizeof(basic_value_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_VALUE_TYPE_NUMBER ;
    ret->value.nvalue_ = v ;
    return ret;
}

static basic_value_t *create_string_value(const char *v)
{
    if (v == NULL) {
        v = "" ;
    }

    basic_value_t *ret = (basic_value_t *)malloc(sizeof(basic_value_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_VALUE_TYPE_STRING ;
    ret->value.svalue_ = (char *)malloc(strlen(v) + 1) ;
    if (ret->value.svalue_ == NULL) {
        free(ret) ;
        return NULL ;
    }
    strcpy(ret->value.svalue_, v) ;
    return ret;
}

static basic_value_t *create_string_value_keep(char *v)
{
    basic_value_t *ret = (basic_value_t *)malloc(sizeof(basic_value_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_VALUE_TYPE_STRING ;
    ret->value.svalue_ = v ;
    return ret;
}

const bool value_to_string(uint32_t str, basic_value_t *value)
{
    bool ret = true;

    if (value->type_ == BASIC_VALUE_TYPE_STRING) {
        if (!basic_str_add_str(str, "\""))
            ret = false;

        if (ret && !basic_str_add_str(str, value->value.svalue_))
            ret = false;

        if (!basic_str_add_str(str, "\""))
            ret = false;
    }
    else 
    {
        if ((value->value.nvalue_ - (int)value->value.nvalue_) < 1e-6) {
            ret = basic_str_add_int(str, (int)value->value.nvalue_);
        }
        else {
            ret = basic_str_add_double(str, value->value.nvalue_);
        }
    }

    return ret;
}

static basic_var_t* get_var_from_index(uint32_t index)
{
    for (basic_var_t* var = vars; var != NULL; var = var->next_) {
        if (var->index_ == index)
            return var;
    }

    return NULL;
}

bool basic_var_get(const char *name, uint32_t *index, basic_err_t *err)
{    
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (_stricmp(var->name_, name) == 0) {
            *index = var->index_ ;
            return true ;
        }
    }

    basic_var_t *newvar = (basic_var_t *)malloc(sizeof(basic_var_t)) ;
    if (newvar == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    strcpy(newvar->name_, name) ;
    newvar->value_ = NULL ;
    newvar->index_ = next_var_index++ ;
    newvar->next_ = vars ;
    newvar->dims_ = NULL ;
    newvar->dimcnt_ = 0 ;
    vars = newvar ;
    
    *index = newvar->index_ ;

    return true ;
}

bool basic_var_destroy(uint32_t index)
{
	basic_var_t *var ;

	if (vars->index_ == index) {
		var = vars ;
		vars = vars->next_ ;
	}
	else {
		var = vars ;
		while (var->next_ && var->next_->index_ != index)
			var = var->next_ ;

		if (var->next_ == NULL)
			return false ;

		basic_var_t *save = var ;
		var = save->next_ ;
		save->next_ = var->next_ ;
	}

	if (var->value_ != NULL) {
		basic_destroy_value(var->value_);
	}

	free(var) ;
	return true ;
}

const char *basic_var_get_name(uint32_t index)
{
    basic_var_t* var = get_var_from_index(index);
    if (var != NULL)
        return var->name_;

    return NULL ;
}

bool basic_var_set_value(uint32_t index, basic_value_t* value, basic_err_t* err)
{
    basic_var_t* var = get_var_from_index(index);
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE;
        return false;
    }

    if (var->name_[strlen(var->name_) - 1] == '$') {
        if (value->type_ != BASIC_VALUE_TYPE_STRING) {
            *err = BASIC_ERR_TYPE_MISMATCH;
            return false;
        }
    }
    else {
        if (value->type_ != BASIC_VALUE_TYPE_NUMBER) {
            *err = BASIC_ERR_TYPE_MISMATCH;
            return false;
        }
    }

    if (var->value_ != NULL) {
        basic_destroy_value(var->value_);
    }
    var->value_ = value;
    return true;
}

bool basic_var_set_value_number(uint32_t index, double value, basic_err_t* err)
{
    basic_value_t *nval = create_number_value(value) ;
    if (nval == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    return basic_var_set_value(index, nval, err) ;
}

basic_value_t *basic_var_get_value(uint32_t index)
{
    basic_var_t* var = get_var_from_index(index);
    if (var == NULL) {
        return NULL;
    }

    return var->value_;
}

int basic_var_get_dim_count(uint32_t index, basic_err_t *err)
{
    basic_var_t *var = get_var_from_index(index) ;
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE ;
        return -1 ;
    }

    if (var->dimcnt_ == 0) {
        *err = BASIC_ERR_NOT_ARRAY ;
        return -1 ;
    }

    *err = BASIC_ERR_NONE;
    return var->dimcnt_ ;
}

bool basic_var_get_dims(uint32_t index, uint32_t* dimcnt, uint32_t* dims, basic_err_t* err)
{
    basic_var_t* var = get_var_from_index(index);
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE;
        return -1;
    }

    if (var->dimcnt_ == 0) {
        *err = BASIC_ERR_NOT_ARRAY;
        return -1;
    }

    *dimcnt = var->dimcnt_;
    memcpy(dims, var->dims_, sizeof(uint32_t) * var->dimcnt_);
    return true;
}

static int compute_index(int dimcnt, uint32_t *maxdims, uint32_t *dims)
{
    int ret = 0 ;
    int mult = 1 ;
    for(int i = 0 ; i < dimcnt ; i++) {
        ret += (dims[i] - 1) * mult ;
        mult *= maxdims[i] ;
    }

    return ret ;
}

basic_value_t *basic_var_get_array_value(uint32_t index, uint32_t *dims, basic_err_t *err)
{
    basic_value_t *ret ;

    basic_var_t *var = get_var_from_index(index) ;
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE ;
        return NULL ;
    }

    if (var->dimcnt_ == 0) {
        *err = BASIC_ERR_NOT_ARRAY ;
        return NULL ;
    }

    int ain = compute_index(var->dimcnt_, var->dims_, dims) ;
    if (ain == -1) {
        *err = BASIC_ERR_INVALID_DIMENSION ;
        return NULL ;
    }

    if (var->sarray_ != NULL) {
        ret = create_string_value(var->sarray_[ain]);
    }
    else {
        ret = create_number_value(var->darray_[ain]);
    }

    return ret;
}

bool basic_var_set_array_value(uint32_t index, basic_value_t *value, uint32_t *dims, basic_err_t *err)
{
    basic_var_t *var = get_var_from_index(index) ;
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE ;
        return false ;
    }

    if (var->dimcnt_ == 0) {
        *err = BASIC_ERR_NOT_ARRAY ;
        return false ;
    }

    if (var->sarray_ != NULL) {
        // String array
        if (value->type_ != BASIC_VALUE_TYPE_STRING) {
            *err = BASIC_ERR_TYPE_MISMATCH ;
            return false ;
        }

        int ain = compute_index(var->dimcnt_, var->dims_, dims) ;
        if (ain == -1) {
            *err = BASIC_ERR_INVALID_DIMENSION ;
            return false ;
        }

        var->sarray_[ain] = _strdup(value->value.svalue_) ;
        if (var->sarray_[ain] == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return false ;
        }
    }
    else {
        // Double array
        if (value->type_ != BASIC_VALUE_TYPE_NUMBER) {
            *err = BASIC_ERR_TYPE_MISMATCH ;
            return false ;
        }    

        int ain = compute_index(var->dimcnt_, var->dims_, dims) ;
        if (ain == -1) {
            *err = BASIC_ERR_INVALID_DIMENSION ;
            return false ;
        }
        var->darray_[ain] = value->value.nvalue_ ;
    }

    return true ;
}

static bool isString(basic_var_t *var)
{
    return var->name_[strlen(var->name_) - 1] == '$' ;
}

bool basic_var_add_dims(uint32_t index, uint32_t dimcnt, uint32_t *dims, basic_err_t *err)
{
    basic_var_t *var = get_var_from_index(index) ;
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE ;
        return false ;
    }

    var->dimcnt_ = dimcnt ;
    var->dims_ = (uint32_t *)malloc(sizeof(uint32_t) * dimcnt) ;
    if (var->dims_ == NULL)
        return false ;

    memcpy(var->dims_, dims, sizeof(int) * dimcnt) ;

    int total = 1 ;
    for(uint32_t i = 0 ; i < dimcnt ; i++) {
        total *= dims[i] ;
    }

    if (isString(var)) {
        var->darray_ = NULL;
        var->sarray_ = (char **)malloc(sizeof(char *) * total) ;
        if (var->sarray_ == NULL) {
            var->dimcnt_ = 0 ;
            free(var->dims_);
            return false;
        }
        else {
            memset(var->sarray_, 0, sizeof(char *) * total) ;
        }
    }    
    else {
        var->sarray_ = NULL;
        var->darray_ = (double *)malloc(sizeof(double) * total) ;
        if (var->darray_ == NULL) {
            var->dimcnt_ = 0 ;
            free(var->dims_);
            return false;
        }
        else {
            for(int i = 0 ; i < total ; i++) {
                var->darray_[i] = 0.0 ;
            }
        }
    }

    return true ;
}

void basic_var_clear_all()
{
    while (vars != NULL) {
        basic_var_destroy(vars->index_) ;
    }
}

static void basic_destroy_operand(basic_operand_t *operand)
{
    switch(operand->type_) {
        case BASIC_OPERAND_TYPE_CONST:
            basic_destroy_value(operand->operand_.const_);
            break ;

        case BASIC_OPERAND_TYPE_OPERATOR:
            basic_destroy_operand(operand->operand_.operator_.left_) ;
            basic_destroy_operand(operand->operand_.operator_.right_) ;
            break ;

        case BASIC_OPERAND_TYPE_VAR:
            if (operand->operand_.var_.dimcnt_ > 0) {
                for(int i = 0 ; i < operand->operand_.var_.dimcnt_ ; i++) {
                    basic_destroy_operand(operand->operand_.var_.dims_[i]);
                }
                free(operand->operand_.var_.dims_) ;
            }
            break; 

        case BASIC_OPERAND_TYPE_FUNCTION:
            for (int i = 0; i < operand->operand_.function_.func_->num_args_; i++) {
                basic_destroy_operand(operand->operand_.function_.args_[i]);
            }
            free(operand->operand_.function_.args_);
            break;

        case BASIC_OPERAND_TYPE_USERFN:
            for (int i = 0; i < operand->operand_.userfn_.func_->argcnt_; i++) {
                basic_destroy_operand(operand->operand_.userfn_.args_[i]);
            }
            free(operand->operand_.userfn_.args_);
            break;    

        case BASIC_OPERAND_TYPE_BOUNDV:
            break; 

        default:
            assert(false) ;
            break ;        
    }
}

static bool create_expr(basic_operand_t *operand, uint32_t *index, basic_err_t *err)
{
    basic_expr_t *expr = (basic_expr_t *)malloc(sizeof(basic_expr_t)) ;
    if (expr == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    expr->index_ = next_expr_index++ ;
    expr->top_ = operand ;
    expr->next_ = exprs ;
    exprs = expr ;
    *index = expr->index_ ;

    return true ;
}

static basic_operand_t *create_const_operand(basic_value_t *value)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_CONST ;
    ret->operand_.const_ = value ;
    return ret ;
}

static basic_operand_t *create_var_operand(uint32_t varname, int dimcnt, basic_operand_t **dims)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_VAR ;
    ret->operand_.var_.varname_ = varname ;
    ret->operand_.var_.dimcnt_ = dimcnt ;
    if (dimcnt == 0) {
        ret->operand_.var_.dims_ = NULL ;
    }
    else {
        ret->operand_.var_.dims_ = (basic_operand_t **)malloc(sizeof(basic_operand_t *) * dimcnt) ;
        if (ret->operand_.var_.dims_ == NULL) {
            free(ret) ;
            return NULL ;
        }
        memcpy(ret->operand_.var_.dims_, dims, sizeof(basic_operand_t *) * dimcnt);
    }
    return ret ;    
}

static basic_operand_t* create_fun_operand(function_table_t *fun)
{
    basic_operand_t* ret = (basic_operand_t*)malloc(sizeof(basic_operand_t));
    if (ret == NULL)
        return NULL;

    ret->type_ = BASIC_OPERAND_TYPE_FUNCTION;
    ret->operand_.function_.func_ = fun;
    ret->operand_.function_.args_ = (basic_operand_t**)malloc(sizeof(basic_operand_t*) * fun->num_args_);
    if (ret->operand_.function_.args_ == NULL) {
        free(ret);
        return NULL;
    }

    memset(ret->operand_.function_.args_, 0, sizeof(basic_operand_t*) * fun->num_args_);
    return ret;
}

static basic_operand_t* create_userfn_operand(basic_expr_user_fn_t *ufn)
{
    basic_operand_t* ret = (basic_operand_t*)malloc(sizeof(basic_operand_t));
    if (ret == NULL)
        return NULL;

    ret->type_ = BASIC_OPERAND_TYPE_USERFN;
    ret->operand_.userfn_.func_ = ufn ;
    ret->operand_.userfn_.args_ = (basic_operand_t**)malloc(sizeof(basic_operand_t*) * ufn->argcnt_);
    if (ret->operand_.userfn_.args_ == NULL) {
        free(ret);
        return NULL;
    }

    memset(ret->operand_.function_.args_, 0, sizeof(basic_operand_t*) * ufn->argcnt_);
    return ret;
}

static basic_operand_t* create_bound_value(const char *v)
{
    basic_operand_t* ret = (basic_operand_t*)malloc(sizeof(basic_operand_t));
    if (ret == NULL)
        return NULL;

    ret->type_ = BASIC_OPERAND_TYPE_BOUNDV ;
    ret->operand_.boundv_ = v ;
    return ret ;
} 

static function_table_t* lookup_function(const char* name)
{
    for (int i = 0; i < sizeof(functions) / sizeof(functions[0]); i++) {
        if (_stricmp(name, functions[i].string_) == 0) {
            return &functions[i];
        }
    }

    return NULL;
}

static basic_value_t *func_mem(int count, basic_value_t **args, basic_err_t *err)
{
    if (count != 1) {
        *err = BASIC_ERR_ARG_COUNT_MISMATCH ;
        return NULL ;
    }

    if (args[0]->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    int mtype = (int)args[0]->value.nvalue_ ;

    if (mtype < 1 || mtype > 5) {
        *err = BASIC_ERR_INVALID_ARG_VALUE ;
        return NULL ;
    }

    struct mallinfo mall_info = mallinfo();

    extern uint8_t __HeapBase;  /* Symbol exported by the linker. */
    extern uint8_t __HeapLimit; /* Symbol exported by the linker. */

    uint8_t* heap_base = (uint8_t *)&__HeapBase;
    uint8_t* heap_limit = (uint8_t *)&__HeapLimit;
    uint32_t heap_size = (uint32_t)(heap_limit - heap_base);    

    int ret = 0 ;
    switch(mtype)
    {
        case 1:
            ret = heap_size ;
            break ;
        case 2:
            ret = mall_info.uordblks ;
            break ;
        case 3:
            ret = mall_info.arena ;
            break;
        case 4:
            ret = basic_str_memsize(false) ;
            break ;
        case 5:
            ret = basic_str_memsize(true) ;
            break ;            
    }

    basic_value_t *v = create_number_value(ret);
    if (v == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    return v ;
}

static basic_value_t* func_rnd(int count, basic_value_t** args, basic_err_t* err)
{
    if (count != 1) {
        *err = BASIC_ERR_BAD_ARG_COUNT;
        return NULL;
    }

    basic_value_t* v = args[0];
    if (v->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH;
        return NULL;
    }

    double value ;

#ifdef DESKTOP
    value = (double)rand() / (double)RAND_MAX ;
#else
    if (!crypto_inited)
    {
        cy_rslt_t res = cyhal_trng_init(&trng_obj);
        if (res != CY_RET_SUCCESS) {
            *err = BASIC_ERR_TRNG_FAILED ;
            return NULL ;
        }
        crypto_inited = true ;
    }

    value = (double)cyhal_trng_generate(&trng_obj) / (double)(0xffffffff);
#endif

    return create_number_value(value) ;
}

static basic_value_t* func_int(int count, basic_value_t** args, basic_err_t *err)
{
    if (count != 1) {
        *err = BASIC_ERR_BAD_ARG_COUNT;
        return NULL;
    }

    basic_value_t* v = args[0];
    if (v->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH;
        return NULL;
    }

    return create_number_value((int)v->value.nvalue_);
}

static basic_value_t* func_sqrt(int count, basic_value_t** args, basic_err_t *err)
{
    if (count != 1) {
        *err = BASIC_ERR_BAD_ARG_COUNT;
        return NULL;
    }

    basic_value_t* v = args[0];
    if (v->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH;
        return NULL;
    }

    if (v->value.nvalue_ < 0.0) {
        *err = BASIC_ERR_INVALID_ARG_VALUE ;
        return NULL ;
    }

    return create_number_value(sqrt(v->value.nvalue_)) ;
}

static int match_def_local(expr_ctxt_t *ctxt)
{
    if (ctxt->argcnt_ > 0) {
        for(int i = 0 ; i < ctxt->argcnt_ ; i++) {
            if (_stricmp(ctxt->parsebuffer, ctxt->argnames_[i]) == 0)
                return i ;
        }
    }

    return -1 ;
}

static const char *parse_operand(expr_ctxt_t *ctxt, const char *line, basic_operand_t **operand, basic_err_t *err)
{
    int bind = 0 ;
    int localv = 0 ;

    line = skipSpaces(line);
    if (*line == '(') {
        line++;
        line = parse_operand_top(line, ctxt->argcnt_, ctxt->argnames_, operand, err);
        if (line == NULL)
            return NULL;

        line = skipSpaces(line);
        if (*line != ')') {
            *err = BASIC_ERR_EXPECTED_CLOSEPAREN;
            return NULL;
        }
        line++;
    }
    else if (isdigit((uint8_t)*line) || *line == '-' || *line == '+' || *line == '.') {

        bool odd = false ;
        while ((*line == '-' || *line == '+') && bind < sizeof(ctxt->parsebuffer)) {
            if (*line == '-') {
                odd = !odd ;
            }
            line++ ;
        }

        if (odd) {
            ctxt->parsebuffer[bind++] = '-' ;
        }

        //
        // Parse a constant number value
        //
        while (isdigit((uint8_t)*line) && bind < sizeof(ctxt->parsebuffer)) {
            ctxt->parsebuffer[bind++] = *line++ ;

            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_NUMBER_TOO_LONG;
                return NULL;
            }
        }

        if (*line == '.') {
            ctxt->parsebuffer[bind++] = *line++ ;       
            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_NUMBER_TOO_LONG;
                return NULL;
            }
        }

        while (isdigit((uint8_t)*line) && bind < BASIC_PARSE_BUFFER_LENGTH) {
            ctxt->parsebuffer[bind++] = *line++ ;
            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_NUMBER_TOO_LONG;
                return NULL;
            }
        }

        if (*line == 'e' || *line == 'E') {
            ctxt->parsebuffer[bind++] = *line++ ;   
            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_NUMBER_TOO_LONG;
                return NULL;
            }
        }

        while (isdigit((uint8_t)*line) && bind < BASIC_PARSE_BUFFER_LENGTH) {
            ctxt->parsebuffer[bind++] = *line++ ;
            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_NUMBER_TOO_LONG;
                return NULL;
            }
        }
        ctxt->parsebuffer[bind] = '\0' ;

        double fv = atof(ctxt->parsebuffer) ;
        basic_value_t *value = create_number_value(fv);
        if (value == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        *operand = create_const_operand(value) ;
    }
    else if (*line == '"') {
        line++ ;
        while (*line && *line != '"' && bind < sizeof(ctxt->parsebuffer)) {
            ctxt->parsebuffer[bind++] = *line++ ;
            if (bind == BASIC_PARSE_BUFFER_LENGTH) {
                *err = BASIC_ERR_STRING_TOO_LONG;
                return NULL;
            }
        }

        if (*line != '"') {
            *err = BASIC_ERR_UNTERMINATED_STRING ;
            return NULL ;
        }

        line++ ;
        ctxt->parsebuffer[bind] = '\0' ;

        basic_value_t *value = create_string_value(ctxt->parsebuffer) ;
        if (value == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
        
        *operand = create_const_operand(value) ;
    }
    else if (isalpha((uint8_t)*line)) {
        while (*line && (isalpha((uint8_t)*line) || isdigit((uint8_t)*line)) && bind < BASIC_PARSE_BUFFER_LENGTH) {
            ctxt->parsebuffer[bind++] = toupper(*line++) ;
            if (bind > BASIC_MAX_VARIABLE_LENGTH) {
                *err = BASIC_ERR_VARIABLE_TOO_LONG;
                return NULL;
            }
        }
        ctxt->parsebuffer[bind] = '\0';

        function_table_t* fun = lookup_function(ctxt->parsebuffer);
        if (fun) {
            line = skipSpaces(line);
            if (*line != '(') {
                *err = BASIC_ERR_EXPECTED_OPENPAREN;
                return NULL;
            }
            line++;
            *operand = create_fun_operand(fun);
            if (*operand == NULL) {
                *err = BASIC_ERR_OUT_OF_MEMORY;
                return NULL;
            }

            for (int i = 0; i < fun->num_args_; i++) {
                line = parse_operand_top(line, 0, NULL, &(*operand)->operand_.function_.args_[i], err);
                if (line == NULL)
                    return NULL;
            }

            if (*line != ')') {
                *err = BASIC_ERR_EXPECTED_CLOSEPAREN;
                return NULL;
            }
            line++;
        }
        else {
            basic_expr_user_fn_t *ufn = get_user_fn_from_name(ctxt->parsebuffer) ;
            if (ufn) {
                line = skipSpaces(line);
                if (*line != '(') {
                    *err = BASIC_ERR_EXPECTED_OPENPAREN;
                    return NULL;
                }
                line++;
                *operand = create_userfn_operand(ufn);
                if (*operand == NULL) {
                    *err = BASIC_ERR_OUT_OF_MEMORY;
                    return NULL;
                }

                for (int i = 0; i < ufn->argcnt_; i++) {
                    line = parse_operand_top(line, 0, NULL, &(*operand)->operand_.userfn_.args_[i], err);
                    if (line == NULL)
                        return NULL;
                }

                if (*line != ')') {
                    *err = BASIC_ERR_EXPECTED_CLOSEPAREN;
                    return NULL;
                }
                line++;                
            }
            else if ((localv = match_def_local(ctxt)) != -1)
            {
                *operand = create_bound_value(ctxt->argnames_[localv]) ;
            } 
            else 
            {
                //
                // This is a variable
                //
                line = skipSpaces(line);
                uint32_t dimcnt = 0;
                basic_operand_t *dims[BASIC_MAX_DIMS];

                if (*line == '(') {
                    line = basic_expr_parse_dims_operands(line, &dimcnt, dims, err);
                    if (line == NULL)
                        return NULL;
                }

                uint32_t index = basic_str_create_str(ctxt->parsebuffer) ;
                if (index == BASIC_STR_INVALID) {
                    *err = BASIC_ERR_OUT_OF_MEMORY ;
                    return NULL ;
                }
                *operand = create_var_operand(index, dimcnt, dims);
            }
        }
    }
    else {
        *err = BASIC_ERR_EXPECTED_IDENTIFIER ;
        line = NULL ;
    }

    return line ;
}

static const char *parse_operator(const char *line, operator_table_t **oper, basic_err_t *err)
{
    line = skipSpaces(line) ;
    for(int i = 0 ; i < sizeof(operators) / sizeof(operators[0]) ; i++) {
        if (strncmp(line, operators[i].string_, strlen(operators[i].string_)) == 0) {
            *oper = &operators[i] ;
            *err = BASIC_ERR_NONE ;
            return line + strlen(operators[i].string_) ;
        }
    }

    *err = BASIC_ERR_INVALID_OPERATOR ;
    return line ;
}

bool basic_expr_operand_array_to_str(uint32_t str, int cnt, basic_operand_t **args)
{
    if (!basic_str_add_str(str, "(")) {
        basic_str_destroy(str) ;
        return BASIC_STR_INVALID ;
    }

    for (int i = 0; i < cnt; i++) {
        if (i != 0) {
            if (!basic_str_add_str(str, ",")) {
                basic_str_destroy(str);
                return BASIC_STR_INVALID;
            }
        }
        if (!basic_operand_to_string(NULL, args[i], str)) {
            basic_str_destroy(str);
            return BASIC_STR_INVALID;
        }
    }

    if (!basic_str_add_str(str, ")")) {
        basic_str_destroy(str) ;
        return BASIC_STR_INVALID ;
    }

    return str;
}

static bool basic_operand_to_string(basic_operand_t *parent, basic_operand_t *oper, uint32_t str)
{
    bool ret = true ;
    const char *v ;

    switch(oper->type_)
    {
        case BASIC_OPERAND_TYPE_CONST:
            if (!value_to_string(str, oper->operand_.const_))
                ret = false;
            break;

        case BASIC_OPERAND_TYPE_OPERATOR:
            {
                bool parens = false ;

                assert(parent == NULL || parent->type_ == BASIC_OPERAND_TYPE_OPERATOR);
                if (parent != NULL && parent->operand_.operator_.operator_->prec_ < oper->operand_.operator_.operator_->prec_) {
                    if (!basic_str_add_str(str, "("))
                        ret = false ;
                    parens = true ;
                }

                if (ret && !basic_operand_to_string(oper, oper->operand_.operator_.left_, str))
                    ret = false ;

                if (ret) {
                    if (!basic_str_add_str(str, " "))
                        ret = false ;

                    if (!basic_str_add_str(str, oper->operand_.operator_.operator_->string_))
                        ret = false ;

                    if (!basic_str_add_str(str, " "))
                        ret = false ;                        
                }

                if (ret) {
                    if (!basic_operand_to_string(oper, oper->operand_.operator_.right_, str))
                        ret = false ;
                }

                if (parens) {
                    if (!basic_str_add_str(str, ")"))
                        ret = false ;   
                }
            }
            break;

        case BASIC_OPERAND_TYPE_VAR:
            v = basic_str_value(oper->operand_.var_.varname_);
            if (!basic_str_add_str(str, v))
                ret = false;

            if (oper->operand_.var_.dimcnt_ > 0) {
                if (ret && !basic_expr_operand_array_to_str(str, oper->operand_.var_.dimcnt_, oper->operand_.var_.dims_))
                    ret = false;

            }
            break ;

        case BASIC_OPERAND_TYPE_FUNCTION:
            if (!basic_str_add_str(str, oper->operand_.function_.func_->string_)) {
                ret = false;
            }

            if (ret && !basic_expr_operand_array_to_str(str, oper->operand_.function_.func_->num_args_, oper->operand_.function_.args_))
                ret = false;
            break ;

        case BASIC_OPERAND_TYPE_USERFN:
            if (!basic_str_add_str(str, oper->operand_.userfn_.func_->name_)) {
                ret = false;
            }

            if (ret && !basic_expr_operand_array_to_str(str, oper->operand_.userfn_.func_->argcnt_, oper->operand_.userfn_.args_))
                ret = false;
            break ;

        case BASIC_OPERAND_TYPE_BOUNDV:
            if (!basic_str_add_str(str, oper->operand_.boundv_))
                ret = false ;
            break ;

    }
    
    return ret;
}


static bool push_operand(expr_ctxt_t *ctxt, basic_operand_t *o)
{
    assert(o != NULL) ;

    if (ctxt->operand_top_ == BASIC_MAX_EXPR_DEPTH)
        return false;

    ctxt->operands_[ctxt->operand_top_++]  = o ;
    return true ;
}

static basic_operand_t *pop_operand(expr_ctxt_t* ctxt)
{
    assert(ctxt->operand_top_ != 0);
    return ctxt->operands_[--ctxt->operand_top_] ;
}

static bool push_operator(expr_ctxt_t* ctxt, basic_operand_t *o)
{
    assert(o != NULL) ;

    assert(o->type_ == BASIC_OPERAND_TYPE_OPERATOR);
    if (ctxt->operator_top_ == BASIC_MAX_EXPR_DEPTH)
        return false;

    ctxt->operators_[ctxt->operator_top_++] = o ;
    return true ;
}

static basic_operand_t *pop_operator(expr_ctxt_t* ctxt)
{
    assert(ctxt->operator_top_ != 0) ;
    return ctxt->operators_[--ctxt->operator_top_] ;
}

static basic_operand_t *peek_operator(expr_ctxt_t* ctxt)
{
    assert(ctxt->operator_top_ != 0) ;
    return ctxt->operators_[ctxt->operator_top_ - 1] ;    
}

static basic_operand_t *createOperator(operator_table_t *t)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t));
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_OPERATOR ;
    ret->operand_.operator_.operator_ = t ;
    return ret;
}

static void reduce(expr_ctxt_t *ctxt)
{
    basic_operand_t *op1 = pop_operator(ctxt) ;
    assert(op1->type_ == BASIC_OPERAND_TYPE_OPERATOR) ;

    op1->operand_.operator_.right_ = pop_operand(ctxt) ;
    op1->operand_.operator_.left_ = pop_operand(ctxt) ;
    push_operand(ctxt, op1);
}


static const char *parse_expr(expr_ctxt_t *ctxt, const char *line, basic_operand_t **ret, basic_err_t *err)
{
    basic_operand_t *operand1, *operand2, *op1, *op2 ;
    operator_table_t *o1, *o2 ;

    line = parse_operand(ctxt, line, &operand1, err) ;
    if (line == NULL) {
        return NULL ;
    }

    if (!push_operand(ctxt, operand1)) {
        *err = BASIC_ERR_TOO_COMPLEX;
        return NULL ;
    }

    line = parse_operator(line, &o1, err) ;
    if (*err != BASIC_ERR_NONE) {
        *err = BASIC_ERR_NONE ;
        *ret = pop_operand(ctxt) ;
        return line ;
    }

    op1 = createOperator(o1);
    if (op1 == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    if (!push_operator(ctxt, op1)) {
        *err = BASIC_ERR_TOO_COMPLEX;
        return NULL ;
    }

    while (true)
    {
        line = parse_operand(ctxt, line, &operand2, err) ;
        if (line == NULL) {
            return NULL ;
        }

        if (!push_operand(ctxt, operand2)) {
            *err = BASIC_ERR_TOO_COMPLEX;
            return NULL ;
        }

        line = parse_operator(line, &o2, err) ;
        if (*err != BASIC_ERR_NONE) {
            while (ctxt->operator_top_ > 0) {
                reduce(ctxt) ;
            }

            *err = BASIC_ERR_NONE ;
            *ret = pop_operand(ctxt);
            return line ;
        }

        op2 = createOperator(o2);
        if (op2 == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        while (ctxt->operator_top_ > 0) {
            op1 = peek_operator(ctxt) ;
            assert(op1->type_ == BASIC_OPERAND_TYPE_OPERATOR);
            if (op1->operand_.operator_.operator_->prec_ > op2->operand_.operator_.operator_->prec_)
                break; 

            reduce(ctxt) ;
        }

        push_operator(ctxt, op2);
    }

    return NULL;
}


static const char* parse_operand_top(const char* line, int argcnt, char **argnames, basic_operand_t** ret, basic_err_t* err)
{
    basic_operand_t* op;
    expr_ctxt_t* ctxt;

    ctxt = (expr_ctxt_t*)malloc(sizeof(expr_ctxt_t));
    if (ctxt == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    ctxt->operand_top_ = 0;
    ctxt->operator_top_ = 0;
    ctxt->argcnt_ = argcnt;
    ctxt->argnames_ = argnames;

    line = parse_expr(ctxt, line, &op, err);
    free(ctxt);

    *ret = op;
    return line;
}

const char *basic_expr_parse(const char *line, int argcnt, char **argnames, uint32_t *index, basic_err_t *err)
{
    basic_operand_t* op;
    
    line = parse_operand_top(line, argcnt, argnames, &op, err);
    if (op == NULL)
        return NULL ;

    //
    // The expr is a simple operand
    //
    if (!create_expr(op, index, err))
        return NULL ;

    return line ;
}

static basic_expr_t *get_expr_from_index(uint32_t index)
{
    for(basic_expr_t *expr = exprs ; expr != NULL ; expr = expr->next_)
    {
        if (expr->index_ == index)
            return expr ;
    }

    return NULL ;
}

bool basic_expr_destroy(uint32_t index)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    if (expr == NULL)
        return false ;

    basic_destroy_operand(expr->top_) ;

    if (exprs == expr) {
        exprs = expr->next_ ;
    }
    else {
        basic_expr_t *e = exprs ;
        while (e != NULL && e->next_ != expr)
            e = e->next_ ;

        assert(e != NULL) ;
        e->next_ = expr->next_ ;
    }

    free(expr) ;
    return true ;
}

static basic_value_t *clone_value(basic_value_t *v)
{
    basic_value_t *ret ;

    if (v->type_ == BASIC_VALUE_TYPE_NUMBER)
        ret = create_number_value(v->value.nvalue_) ;
    else
        ret = create_string_value(v->value.svalue_) ;

    return ret;
}

static basic_value_t *eval_plus(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ != right->type_) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    if (left->type_ == BASIC_VALUE_TYPE_NUMBER) {
        ret = create_number_value(left->value.nvalue_ + right->value.nvalue_);
    }
    else {
        char *combined  = (char *)malloc(strlen(left->value.svalue_) + strlen(right->value.svalue_) + 1) ;
        if (combined == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
        strcpy(combined, left->value.svalue_) ;
        strcat(combined, right->value.svalue_) ;        
        ret = create_string_value_keep(combined) ;
        if (ret == NULL) {
            free(combined) ;
        }
    }

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_minus(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    ret = create_number_value(left->value.nvalue_ - right->value.nvalue_);

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_times(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    ret = create_number_value(left->value.nvalue_ * right->value.nvalue_);

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_divide(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    if (right->value.nvalue_ == 0) {
        *err = BASIC_ERR_DIVIDE_ZERO ;
        return NULL ;
    }

    ret = create_number_value(left->value.nvalue_ / right->value.nvalue_);

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_power(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = pow(left->value.nvalue_, right->value.nvalue_);
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_not_equal(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ != right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_equal(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ == right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_greater(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ > right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_greater_eq(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ >= right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_less(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ < right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_less_eq(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    double p = (left->value.nvalue_ <= right->value.nvalue_) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_or(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    int l = fabs(left->value.nvalue_) > 1e-6 ;
    int r = fabs(right->value.nvalue_) > 1e-6 ;
    double p = (l || r) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_and(basic_value_t *left, basic_value_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    if (left->type_ == BASIC_VALUE_TYPE_STRING || right->type_ == BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    int l = fabs(left->value.nvalue_) > 1e-6 ;
    int r = fabs(right->value.nvalue_) > 1e-6 ;
    double p = (l && r) ;
    ret = create_number_value(p) ;

    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
    }

    return ret;
}

static basic_value_t *eval_operator(operator_table_t *oper, int vcnt, char **names, basic_value_t **values, basic_operand_t *left, basic_operand_t *right, basic_err_t *err)
{
    basic_value_t* ret = NULL;

    basic_value_t *leftval = eval_node(left,  vcnt, names, values, err) ;
    if (leftval == NULL)
        return NULL ;

    basic_value_t *rightval = eval_node(right,  vcnt, names, values, err) ;
    if (rightval == NULL) {
        basic_destroy_value(leftval) ;
        return NULL ;
    }

    switch(oper->oper_) {
        case BASIC_OPERATOR_PLUS:
            ret = eval_plus(leftval, rightval, err) ;
            break ;
        case BASIC_OPERATOR_MINUS:
            ret = eval_minus(leftval, rightval, err) ;
            break ;
        case BASIC_OPERATOR_TIMES:
            ret = eval_times(leftval, rightval, err) ;
            break ;
        case BASIC_OPERATOR_DIVIDE:
            ret = eval_divide(leftval, rightval, err) ;
            break ;      
        case BASIC_OPERATOR_POWER:
            ret = eval_power(leftval, rightval, err) ;
            break ;
        case BASIC_OPERATOR_NOT_EQUAL:
            ret = eval_not_equal(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_EQUAL:
            ret = eval_equal(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_GREATER:
            ret = eval_greater(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_GREATER_EQ:
            ret = eval_greater_eq(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_LESS:
            ret = eval_less(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_LESS_EQ:
            ret = eval_less_eq(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_OR:
            ret = eval_or(leftval, rightval, err) ;
            break ;        
        case BASIC_OPERATOR_AND:
            ret = eval_and(leftval, rightval, err) ;
            break ;        
    }

    basic_destroy_value(leftval) ;
    basic_destroy_value(rightval) ;
    return ret;
}

static basic_value_t *lookup_userfn_value(const char *name, uint32_t cnt, char **names, basic_value_t **values)
{
    for(int i = 0 ; i < cnt ; i++) {
        if (_stricmp(name, names[i]) == 0) {
            return values[i] ;
        }
    }

    return NULL ;
}

static basic_value_t *eval_node(basic_operand_t *op, int vcnt, char **names, basic_value_t **values, basic_err_t *err)
{
    basic_value_t *ret = NULL ;
    uint32_t dims[BASIC_MAX_DIMS];

    switch(op->type_)
    {
        case BASIC_OPERAND_TYPE_CONST:
            ret = clone_value(op->operand_.const_) ;
            break ;
        case BASIC_OPERAND_TYPE_OPERATOR:
            ret = eval_operator(op->operand_.operator_.operator_, 
                                vcnt, names, values,
                                op->operand_.operator_.left_,
                                op->operand_.operator_.right_, err) ;
            break; 
        case BASIC_OPERAND_TYPE_VAR:
            {
                uint32_t varidx ;
                const char *varname = basic_str_value(op->operand_.var_.varname_);
                assert(varname != NULL) ;

                if (!basic_var_get(varname, &varidx, err))
                    return NULL ;

                if (op->operand_.var_.dimcnt_ == 0) {
                    basic_value_t* val = basic_var_get_value(varidx);
                    if (val == NULL) {
                        *err = BASIC_ERR_NO_SUCH_VARIABLE;
                        ret = NULL;
                    }
                    else {
                        ret = clone_value(val);
                    }
                }
                else {
                    //
                    // We create a new value here since the storage is just the values and not a
                    // basic_value_t object.
                    //
                    for (int i = 0; i < op->operand_.var_.dimcnt_; i++) {
                        basic_operand_t *dimexpr = op->operand_.var_.dims_[i];
                        basic_value_t* dimval = eval_node(dimexpr,  0, NULL, NULL, err);
                        if (dimval == NULL)
                            return NULL ;

                        if (ret != NULL && dimval->type_ == BASIC_VALUE_TYPE_STRING) {
                            *err = BASIC_ERR_TYPE_MISMATCH;
                            return NULL ;
                        }

                        dims[i] = (int)dimval->value.nvalue_;
                    }

                    ret = basic_var_get_array_value(varidx, dims, err) ;
                }
            }
            break; 

        case BASIC_OPERAND_TYPE_FUNCTION:
            {
                int argcnt = op->operand_.function_.func_->num_args_;
                basic_value_t** argvals = (basic_value_t**)malloc(sizeof(basic_value_t*) * argcnt);
                if (argvals == NULL) {
                    *err = BASIC_ERR_OUT_OF_MEMORY;
                    return NULL;
                }

                for (int i = 0; i < argcnt ; i++) {
                    basic_operand_t* argexpr = op->operand_.function_.args_[i];
                    argvals[i] = eval_node(argexpr,  0, NULL, NULL, err);
                    if (argvals[i] == NULL)
                        return NULL;
                }

                ret = op->operand_.function_.func_->eval_(argcnt, argvals, err);
                for (int i = 0; i < argcnt; i++) {
                    basic_destroy_value(argvals[i]);
                }

                free(argvals);
            }
            break;

        case BASIC_OPERAND_TYPE_USERFN:
            {
                int argcnt = op->operand_.userfn_.func_->argcnt_;
                basic_value_t** argvals = (basic_value_t**)malloc(sizeof(basic_value_t*) * argcnt);
                if (argvals == NULL) {
                    *err = BASIC_ERR_OUT_OF_MEMORY;
                    return NULL;
                }

                for (int i = 0; i < argcnt ; i++) {
                    basic_operand_t* argexpr = op->operand_.userfn_.args_[i];
                    argvals[i] = eval_node(argexpr,  0, NULL, NULL, err);
                    if (argvals[i] == NULL)
                        return NULL;
                }

                ret = basic_expr_eval(op->operand_.userfn_.func_->expridx_,
                                      op->operand_.userfn_.func_->argcnt_,
                                      op->operand_.userfn_.func_->args_, 
                                      argvals, err);
                for (int i = 0; i < argcnt; i++) {
                    basic_destroy_value(argvals[i]);
                }
                free(argvals);
            }
            break ;

        case BASIC_OPERAND_TYPE_BOUNDV:
            {
                basic_value_t *v = lookup_userfn_value(op->operand_.boundv_, vcnt, names, values) ;
                if (v == NULL) {
                    *err = BASIC_ERR_UNBOUND_LOCAL_VAR ;
                    ret = NULL ;
                }
                else {
                    ret = clone_value(v);
                }
            }
            break ;        
    }

#ifdef _PRINT_EVALS_
    uint32_t strh = basic_str_create() ;
    basic_operand_to_string(NULL, op, strh);
    basic_str_add_str(strh, "=");
    value_to_string(strh, ret);
    const char *strval = basic_str_value(strh);
    printf("%s\n", strval);
#endif

    return ret ;
}

basic_value_t *basic_expr_eval(uint32_t index, uint32_t cntv, char **names, basic_value_t **values, basic_err_t *err)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    assert(expr != NULL) ;

    return eval_node(expr->top_, cntv, names, values, err) ;
}

uint32_t basic_expr_to_string(uint32_t index)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    assert(expr != NULL) ;

    uint32_t ret = basic_str_create() ;
    if (ret == BASIC_STR_INVALID)
        return BASIC_STR_INVALID;

    if (!basic_operand_to_string(NULL, expr->top_, ret)) {
        basic_str_destroy(ret);
        return BASIC_STR_INVALID ;
    }

    return ret ;
}

void basic_expr_clear_all()
{
    while (exprs) {
        basic_expr_destroy(exprs->index_);
    }
}

#ifdef _DUMP_EXPRS_
void basic_expr_dump()
{
    for (basic_expr_t* expr = exprs; expr != NULL; expr = expr->next_) {
        uint32_t exprhand = basic_expr_to_string(expr->index_);
        const char* exprstr = basic_str_value(exprhand);
        printf("%d: '%s'\n", expr->index_, exprstr);
        basic_str_destroy(exprhand);
    }
}
#endif

basic_expr_user_fn_t* get_user_fn_from_index(uint32_t index)
{
    for (basic_expr_user_fn_t* fn = userfns; fn != NULL; fn = fn->next_)
    {
        if (fn->index_ == index)
            return fn;
    }

    return NULL;
}

static basic_expr_user_fn_t* get_user_fn_from_name(const char *name)
{
    for (basic_expr_user_fn_t* fn = userfns; fn != NULL; fn = fn->next_)
    {
        if (_stricmp(name, fn->name_) == 0)
            return fn ;
    }

    return NULL;
}

bool basic_userfn_bind(char* fnname, uint32_t argcnt, char** argnames, uint32_t exprindex, uint32_t *fnindex, basic_err_t* err)
{
    basic_expr_user_fn_t* ufn = (basic_expr_user_fn_t*)malloc(sizeof(basic_expr_user_fn_t));
    if (ufn == NULL)
        return false;

    ufn->index_ = next_user_fn_index++;
    ufn->next_ = userfns;
    userfns = ufn;

    ufn->name_ = fnname;
    ufn->argcnt_ = argcnt;
    ufn->expridx_ = exprindex;

    ufn->args_ = (char **)malloc(sizeof(char *) * argcnt);
    if (ufn->args_ == NULL) {
        free(ufn) ;
        return NULL ;
    }

    for(int i = 0 ; i < argcnt ; i++) {
        ufn->args_[i] = argnames[i] ;
    }

    if (fnindex != NULL)
        *fnindex = ufn->index_;
        
    return true;
}

const char *basic_userfn_name(uint32_t index)
{
    basic_expr_user_fn_t* ufn = get_user_fn_from_index(index);
    if (ufn == NULL)
        return NULL ;

    return ufn->name_ ;
}

char **basic_userfn_args(uint32_t index, uint32_t *argcnt)
{
    basic_expr_user_fn_t* ufn = get_user_fn_from_index(index);
    if (ufn == NULL)
        return NULL ;

    *argcnt = ufn->argcnt_ ;
    return ufn->args_ ;    
}

bool basic_userfn_destroy(uint32_t index)
{
    basic_expr_user_fn_t* ufn = get_user_fn_from_index(index);
    if (ufn == NULL)
        return false;

    if (userfns == ufn) {
        userfns = userfns->next_;
    }
    else {
        basic_expr_user_fn_t* f = userfns;
        while (f != NULL && f->next_ != ufn)
            f = f->next_;

        assert(f != NULL);
        f->next_ = ufn->next_;
    }

    for (int i = 0; i < ufn->argcnt_; i++) {
        free(ufn->args_[i]);
    }

    basic_expr_destroy(ufn->expridx_);
    free(ufn->args_);
    free(ufn);

    return true;
}

void basic_userfn_clear_all()
{
    while (userfns) {
        basic_userfn_destroy(userfns->index_) ;
    }
}