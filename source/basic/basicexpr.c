#include "basicexpr.h"
#include "basicexprint.h"
#include "basicerr.h"
#include "basiccfg.h"
#include "mystr.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static basic_value_t *eval_node(basic_operand_t *op, basic_err_t *err) ;

static uint32_t next_var_index = 1 ;
static basic_var_t *vars = NULL ;

static uint32_t next_expr_index = 1 ;
static basic_expr_t *exprs = NULL ;

operator_table_t operators[] = 
{
    { BASIC_OPERATOR_PLUS, "+" , 4},
    { BASIC_OPERATOR_MINUS, "-", 4 },
    { BASIC_OPERATOR_TIMES, "*", 3 },
    { BASIC_OPERATOR_DIVIDE, "/", 3 },            
} ;

// This must be at least BASIC_MAX_VARIABLE_LENGTH + 1 long.
// The length of this determines the length of a string constant
// or a numeric constant that can be parsed
static char parsebuffer[265] ;

const char *basic_parse_int(const char *line, int *value, basic_err_t *err)
{
    *err = BASIC_ERR_NONE ;

    line = skipSpaces(line) ;
    if (!isdigit(*line) && *line != '+' && *line != '-') {
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

    while (isdigit(*line)) {
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

const char *basic_value_to_string(basic_value_t *value)
{
    const char *ret ;

    if (value->type_ == BASIC_VALUE_TYPE_STRING) {
        sprintf(parsebuffer, "\"%s\"", value->value.svalue_);
        ret = parsebuffer ;
    }
    else {
        if ((value->value.nvalue_ - (int)value->value.nvalue_) < 1e-6) {
            sprintf(parsebuffer, "%d", (int)value->value.nvalue_) ;
        }
        else {
            sprintf(parsebuffer, "%f", value->value.nvalue_);
        }
        ret = parsebuffer ;
    }

    return ret;
}

bool basic_var_get(const char *name, uint32_t *index, basic_err_t *err)
{    
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (strcmp(var->name_, name) == 0) {
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
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            return var->name_ ;
        }
    }

    return NULL ;
}

bool basic_var_set_value(uint32_t index, basic_value_t *value, basic_err_t *err)
{
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            if (var->name_[strlen(var->name_) - 1] == '$') {
                if (value->type_ != BASIC_VALUE_TYPE_STRING) {
                    *err = BASIC_ERR_TYPE_MISMATCH ;
                    return false ;
                }
            }
            else {
                if (value->type_ != BASIC_VALUE_TYPE_NUMBER) {
                    *err = BASIC_ERR_TYPE_MISMATCH ;
                    return false ;
                }
            }

            if (var->value_ != NULL) {
                basic_destroy_value(var->value_) ;
            }
            var->value_ = value ;
            return true ;
        }
    }

    *err = BASIC_ERR_NO_SUCH_VARIABLE ;
    return false ;
}

basic_value_t *basic_var_get_value(uint32_t index)
{
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            return var->value_ ;
        }
    }

    return NULL ;
}

basic_var_t *get_var_from_index(uint32_t index)
{
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            return var ;
        }
    }

    return NULL ;
}

bool basic_var_validate_array(uint32_t index, int dimcnt, int *dims, basic_err_t *err)
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

    if (var->dimcnt_ != dimcnt) {
        *err = BASIC_ERR_DIM_MISMATCH ;
        return false ;
    }

    for(int i = 0 ; i < dimcnt ; i++) {
        if (dims[i] > var->dims_[i]) {
            *err = BASIC_ERR_INVALID_DIMENSION ;
            return false;
        }
    }

    return true ;
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

    return var->dimcnt_ ;
}

static int compute_index(int dimcnt, int *maxdims, int *dims)
{
    int ret = 0 ;
    int mult = 1 ;
    for(int i = 0 ; i < dimcnt ; i++) {
        ret += dims[i] * mult ;
        mult *= maxdims[i] ;
    }

    return ret ;
}

bool basic_var_set_array_value(uint32_t index, basic_value_t *value, int *dims, basic_err_t *err)
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

        int index = compute_index(var->dimcnt_, var->dims_, dims) ;
        if (index == -1) {
            *err = BASIC_ERR_INVALID_DIMENSION ;
            return false ;
        }
        var->sarray_[index] = strdup(value->value.svalue_) ;
        if (var->sarray_[index] == NULL) {
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

        int index = compute_index(var->dimcnt_, var->dims_, dims) ;
        if (index == -1) {
            *err = BASIC_ERR_INVALID_DIMENSION ;
            return false ;
        }
        var->darray_[index] = value->value.nvalue_ ;
    }

    return true ;
}

static bool isString(basic_var_t *var)
{
    return var->name_[strlen(var->name_) - 1] == '$' ;
}

bool basic_var_add_dims(uint32_t index, uint32_t dimcnt, int *dims, basic_err_t *err)
{
    basic_var_t *var = get_var_from_index(index) ;
    if (var == NULL) {
        *err = BASIC_ERR_NO_SUCH_VARIABLE ;
        return false ;
    }

    var->dimcnt_ = dimcnt ;
    var->dims_ = (int *)malloc(sizeof(int) * dimcnt) ;
    if (var->dims_ == NULL)
        return false ;

    memcpy(var->dims_, dims, sizeof(int) * dimcnt) ;

    int total = 1 ;
    for(int i = 0 ; i < dimcnt ; i++) {
        total *= dims[i] ;
    }

    if (isString(var)) {
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
        var->darray_ = (double *)malloc(sizeof(double) * total) ;
        if (var->sarray_ == NULL) {
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

static void basic_destroy_operator(basic_operand_t *operand)
{
    switch(operand->type_) {
        case BASIC_OPERAND_TYPE_CONST:
            basic_destroy_value(operand->operand_.const_value_);
            break ;

        case BASIC_OPERAND_TYPE_OPERATOR:
            basic_destroy_operator(operand->operand_.operator_args_.left_) ;
            basic_destroy_operator(operand->operand_.operator_args_.right_) ;
            break ;

        case BASIC_OPERAND_TYPE_VAR:
            if (operand->operand_.var_.dimcnt_ > 0) {
                free(operand->operand_.var_.dims_) ;
            }
            break; 
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
    ret->operand_.const_value_ = value ;
    return ret ;
}

static basic_operand_t *create_var_operand(uint32_t varindex, int dimcnt, int *dims)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_VAR ;
    ret->operand_.var_.varindex_ = varindex ;
    ret->operand_.var_.dimcnt_ = dimcnt ;
    if (dimcnt == 0) {
        ret->operand_.var_.dims_ = NULL ;
    }
    else {
        ret->operand_.var_.dims_ = (int *)malloc(sizeof(int) * dimcnt) ;
        if (ret->operand_.var_.dims_ == NULL) {
            free(ret) ;
            return NULL ;
        }
    }
    return ret ;    
}

static const char *parse_operand(const char *line, basic_operand_t **operand, basic_err_t *err)
{
    int bind = 0 ;

    line = skipSpaces(line);
    if (isdigit((uint8_t)*line)) {
        //
        // Parse a constant number value
        //

        while (isdigit((uint8_t)*line) && bind < sizeof(parsebuffer)) {
            parsebuffer[bind++] = *line++ ;
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_NUMBER_TOO_LONG ;
            return NULL ;
        }

        if (*line == '.') {
            parsebuffer[bind++] = *line++ ;            
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_NUMBER_TOO_LONG ;
            return NULL ;
        }

        while (isdigit((uint8_t)*line) && bind < sizeof(parsebuffer)) {
            parsebuffer[bind++] = *line++ ;
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_NUMBER_TOO_LONG ;
            return NULL ;
        }

        if (*line == 'e' || *line == 'E') {
            parsebuffer[bind++] = *line++ ;   
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_NUMBER_TOO_LONG ;
            return NULL ;
        }

        while (isdigit((uint8_t)*line) && bind < sizeof(parsebuffer)) {
            parsebuffer[bind++] = *line++ ;
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_NUMBER_TOO_LONG ;
            return NULL ;
        }

        parsebuffer[bind] = '\0' ;

        double fv = atof(parsebuffer) ;
        basic_value_t *value = create_number_value(fv);
        if (value == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        *operand = create_const_operand(value) ;
    }
    else if (*line == '"') {
        line++ ;
        while (*line && *line != '"' && bind < sizeof(parsebuffer)) {
            parsebuffer[bind++] = *line++ ;
        }

        if (bind == sizeof(parsebuffer)) {
            *err = BASIC_ERR_STRING_TOO_LONG ;
            return NULL ;
        }

        if (*line != '"') {
            *err = BASIC_ERR_UNTERMINATED_STRING ;
            return NULL ;
        }

        line++ ;
        parsebuffer[bind] = '\0' ;

        basic_value_t *value = create_string_value(parsebuffer) ;
        if (value == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
        
        *operand = create_const_operand(value) ;
    }
    else if (isalpha((uint8_t)*line)) {
        while (*line && isalpha((uint8_t)*line) && bind < sizeof(parsebuffer)) {
            parsebuffer[bind++] = *line++ ;
        }

        if (bind > BASIC_MAX_VARIABLE_LENGTH) {
            *err = BASIC_ERR_VARIABLE_TOO_LONG ;
            return NULL ;
        }

        line = skipSpaces(line) ;
        int dimcnt = 0 ;
        int dims[BASIC_MAX_DIMS]  ;

        if (*line == '(') {
            line++ ;
            while (true) {
                if (dimcnt == BASIC_MAX_DIMS) {
                    *err = BASIC_ERR_TOO_MANY_DIMS ;
                    return NULL ;
                }

                if (!basic_parse_int(line, &dims[dimcnt], err)) {
                    return NULL ;
                }

                line = skipSpaces(line) ;
                if (*line == ')') {
                    line++ ;
                    break ;
                }
                else if (*line == ',') {
                    line++ ;
                }
                else {
                    *err = BASIC_ERR_INVALID_CHAR ;
                    return NULL ;
                }
            }
        }

        uint32_t index ;
        parsebuffer[bind] = '\0' ;
        if (!basic_var_get(parsebuffer, &index, err)) {
            return NULL ;
        }

        *operand = create_var_operand(index, dimcnt, dims);
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

static uint32_t dimvar_to_str(basic_var_args_t *var)
{
    uint32_t str = str_create() ;
    if (str == STR_INVALID)
        return STR_INVALID  ;

    const char *v = basic_var_get_name(var->varindex_);
    if (!str_add_str(str, v)) {
        str_destroy(str) ;
        return STR_INVALID ;
    }

    if (!str_add_str(str, "(")) {
                str_destroy(str) ;
        return STR_INVALID ;
    }

    for(int i = 0 ; i < var->dimcnt_ ; i++) {
        if (i != 0) {
            if (!str_add_str(str, ",")) {
                str_destroy(str) ;
                return STR_INVALID ;
            }            
        }
    }

    if (!str_add_str(str, ")")) {
        str_destroy(str) ;
        return STR_INVALID ;
    }
}

static bool basic_oper_to_string(basic_operand_t *oper, uint32_t str)
{
    bool ret = true ;
    const char *v ;

    switch(oper->type_)
    {
        case BASIC_OPERAND_TYPE_CONST:
            v = basic_value_to_string(oper->operand_.const_value_);
            if (!str_add_str(str, v))
                ret = false ;
            break;

        case BASIC_OPERAND_TYPE_OPERATOR:
            {
                if (!basic_oper_to_string(oper->operand_.operator_args_.left_, str))
                    ret = false ;

                if (ret) {
                    if (!str_add_str(str, oper->operand_.operator_args_.operator_->string_))
                        ret = false ;
                }

                if (ret) {
                    if (!basic_oper_to_string(oper->operand_.operator_args_.right_, str))
                        ret = false ;
                }
            }
            break;

        case BASIC_OPERAND_TYPE_VAR:
            // TODO: deal with arrays
            if (oper->operand_.var_.dimcnt_ > 0) {
                int strh = dimvar_to_str(&oper->operand_.var_) ;
                if (strh == STR_INVALID)
                    ret = false;

                if (!str_add_handle(str, strh)) {
                    str_destroy(strh) ;
                    ret = false ;
                }
            }
            else {
                v = basic_var_get_name(oper->operand_.var_.varindex_);
                if (!str_add_str(str, v))
                    ret = false ;
            }

            break ;
    }
    
    return ret;
}

static basic_operand_t *operand_stack[64] ;
static int operand_top = 0 ;

static basic_operand_t *operator_stack[64] ;
static int operator_top = 0 ;

static bool push_operand(basic_operand_t *o)
{
    assert(o != NULL) ;

    if (operand_top == sizeof(operand_stack)/sizeof(operand_stack[0])) {
        return false;
    }
    operand_stack[operand_top++]  = o ;
    return true ;
}

static basic_operand_t *pop_operand()
{
    assert(operand_top != 0);
    return operand_stack[--operand_top] ;
}

static bool push_operator(basic_operand_t *o)
{
    assert(o != NULL) ;

    assert(o->type_ == BASIC_OPERAND_TYPE_OPERATOR);
    if (operator_top == sizeof(operator_stack)/sizeof(operator_stack[0])) {
        return false;
    }
    operator_stack[operator_top++] = o ;
    return true ;
}

static basic_operand_t *pop_operator()
{
    assert(operator_top != 0) ;
    return operator_stack[--operator_top] ;
}

static basic_operand_t *peek_operator()
{
    assert(operator_top != 0) ;
    return operator_stack[operator_top - 1] ;    
}

static basic_operand_t *createOperator(operator_table_t *t)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t));
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_OPERATOR ;
    ret->operand_.operator_args_.operator_ = t ;
    return ret;
}

static void reduce()
{
    basic_operand_t *op1 = pop_operator() ;
    assert(op1->type_ == BASIC_OPERAND_TYPE_OPERATOR) ;

    op1->operand_.operator_args_.left_ = pop_operand() ;
    op1->operand_.operator_args_.right_ = pop_operand() ;
    push_operand(op1);
}

static const char *parse_expr(const char *line, basic_operand_t **ret, basic_err_t *err)
{
    basic_operand_t *operand1, *operand2, *op1, *op2 ;
    operator_table_t *o1, *o2 ;

    line = parse_operand(line, &operand1, err) ;
    if (line == NULL) {
        return NULL ;
    }

    if (!push_operand(operand1)) {
        *err = BASIC_ERR_TOO_COMPLEX;
        return NULL ;
    }

    line = parse_operator(line, &o1, err) ;
    if (*err != BASIC_ERR_NONE) {
        *err = BASIC_ERR_NONE ;
        *ret = pop_operand() ;
        return line ;
    }

    op1 = createOperator(o1);
    if (op1 == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    if (!push_operator(op1)) {
        *err = BASIC_ERR_TOO_COMPLEX;
        return NULL ;
    }

    while (true)
    {
        line = parse_operand(line, &operand2, err) ;
        if (line == NULL) {
            return NULL ;
        }

        if (!push_operand(operand2)) {
            *err = BASIC_ERR_TOO_COMPLEX;
            return NULL ;
        }

        line = parse_operator(line, &o2, err) ;
        if (*err != BASIC_ERR_NONE) {
            while (operator_top > 0) {
                reduce() ;
            }

            *err = BASIC_ERR_NONE ;
            *ret = pop_operand();
            return line ;
        }

        op2 = createOperator(o2);
        if (op2 == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        while (operator_top > 0) {
            op1 = peek_operator() ;
            assert(op1->type_ == BASIC_OPERAND_TYPE_OPERATOR);
            if (op1->operand_.operator_args_.operator_->prec_ > op2->operand_.operator_args_.operator_->prec_)
                break; 

            reduce() ;
        }

        push_operator(op2);
    }

    return NULL;
}

const char *basic_parse_expr(const char *line, uint32_t *index, basic_err_t *err)
{
    basic_operand_t *op ;

    operand_top = 0 ;
    operator_top = 0 ;

    line = parse_expr(line, &op, err);
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

bool basic_destroy_expr(uint32_t index)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    if (expr == NULL)
        return false ;

    basic_destroy_operator(expr->top_) ;

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

static basic_value_t *eval_operator(operator_table_t *oper, basic_operand_t *left, basic_operand_t *right, basic_err_t *err)
{
    basic_value_t *ret ;

    basic_value_t *leftval = eval_node(left, err) ;
    if (leftval == NULL)
        return NULL ;

    basic_value_t *rightval = eval_node(right, err) ;
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
    }

    basic_destroy_value(leftval) ;
    basic_destroy_value(rightval) ;
    return ret;
}

static basic_value_t *eval_node(basic_operand_t *op, basic_err_t *err)
{
    basic_value_t *ret = NULL ;

    switch(op->type_)
    {
        case BASIC_OPERAND_TYPE_CONST:
            ret = clone_value(op->operand_.const_value_) ;
            break ;
        case BASIC_OPERAND_TYPE_OPERATOR:
            ret = eval_operator(op->operand_.operator_args_.operator_, 
                                op->operand_.operator_args_.left_,
                                op->operand_.operator_args_.right_, err) ;
            break; 
        case BASIC_OPERAND_TYPE_VAR:
            {
                if (op->operand_.var_.dimcnt_ == 0) {
                    ret = clone_value(basic_var_get_value(op->operand_.var_.varindex_));
                }
                else {
                    //
                    // We create a new value here since the storage is just the values and not a
                    // basic_value_t object.
                    //
                    ret = basic_var_get_array_value(op->operand_.var_.varindex_, op->operand_.var_.dims_) ;
                }
            }
            break; 
    }

    return ret ;
}

basic_value_t *basic_eval_expr(uint32_t index, basic_err_t *err)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    assert(expr != NULL) ;

    return eval_node(expr->top_, err) ;
}

uint32_t basic_expr_to_string(uint32_t index)
{
    basic_expr_t *expr = get_expr_from_index(index) ;
    assert(expr != NULL) ;

    uint32_t ret = str_create() ;
    if (!basic_oper_to_string(expr->top_, ret)) {
        str_destroy(ret);
        return STR_INVALID ;
    }

    return ret ;
}