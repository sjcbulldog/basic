#include "basicexpr.h"
#include "basicexprint.h"
#include "basicerr.h"
#include "mystr.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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

bool basic_get_var(const char *name, uint32_t *index, basic_err_t *err)
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
    vars = newvar ;
    
    *index = newvar->index_ ;

    return true ;
}

bool basic_destroy_var(uint32_t index)
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

const char *basic_get_var_name(uint32_t index)
{
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            return var->name_ ;
        }
    }

    return NULL ;
}

bool basic_set_var_value(uint32_t index, basic_value_t *value, basic_err_t *err)
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

basic_value_t *basic_get_var_value(uint32_t index)
{
    for(basic_var_t *var = vars ; var != NULL ; var = var->next_) {
        if (var->index_ == index) {
            return var->value_ ;
        }
    }

    return NULL ;
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
            // Nothing to do here
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

static basic_operand_t *create_var_operand(uint32_t varindex)
{
    basic_operand_t *ret = (basic_operand_t *)malloc(sizeof(basic_operand_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->type_ = BASIC_OPERAND_TYPE_VAR ;
    ret->operand_.var_ = varindex ;
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

        uint32_t index ;
        parsebuffer[bind] = '\0' ;
        if (!basic_get_var(parsebuffer, &index, err)) {
            return NULL ;
        }

        *operand = create_var_operand(index);
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
            v = basic_get_var_name(oper->operand_.var_);
            if (!str_add_str(str, v))
                ret = false ;

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


static basic_value_t *eval_node(basic_operand_t *op, basic_err_t *err)
{
    basic_value_t *ret = NULL ;

    switch(op->type_)
    {
        case BASIC_OPERAND_TYPE_CONST:
            ret = op->operand_.const_value_ ;            
            break ;
        case BASIC_OPERAND_TYPE_OPERATOR:
            break; 
        case BASIC_OPERAND_TYPE_VAR:
            ret = basic_get_var_value(op->operand_.var_);
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