#include "basicexec.h"
#include "basicerr.h"
#include "basicproc.h"
#include "basicexpr.h"
#include "basiccfg.h"
#include "mystr.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

extern void basic_save(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_load(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_flist(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;

basic_line_t *program = NULL ;
static const char *clearscreen = "\x1b[2J\x1b[;H";
static const char *spaces = "        " ;
static int space_count = 8 ;
static bool trace = false;

static void putSpaces(basic_out_fn_t outfn, int count)
{
    while (count > 0) {
        int howmany = count ;
        if (howmany > space_count) {
            howmany = space_count ;
        }

        (*outfn)(spaces, howmany) ;
        count -= howmany ;
    }
}

static bool dimsToString(uint32_t str, basic_line_t* line, int *i, int dimcnt, int* dims)
{
    int index = *i;

    if (!str_add_str(str, "("))
        return false;

    for (int i = 0; i < dimcnt; i++) {
        assert(line->tokens_[index] == BTOKEN_EXPR);
        index++;

        uint32_t expr = getU32(line, index);
        index += 4;

        uint32_t exprstr = basic_expr_to_string(expr);
        if (!str_add_handle(str, exprstr))
            return false;

        str_destroy(exprstr);
    }

    if (!str_add_str(str, ")"))
        return false;

    *i = index;
    return true;
}

static bool dimToString(basic_line_t* line, uint32_t str)
{
    int index = 1;
    uint32_t dimcnt;
    uint32_t dims[BASIC_MAX_DIMS];
    basic_err_t err;

    while (index < line->count_)
    {
        assert(line->tokens_[index] == BTOKEN_VAR);
        index++;

        uint32_t varidx = getU32(line, index);
        index += 4;

        const char* varname = basic_var_get_name(varidx);
        if (varname == NULL)
            return false;

        if (!basic_var_get_dims(varidx, &dimcnt, dims, &err))
            return false;

        if (!str_add_str(str, varname))
            return false;

        if (!str_add_str(str, "("))
            return false;

        for (int i = 0; i < dimcnt; i++) {
            if (i != 0) {
                if (!str_add_str(str, ","))
                    return false;
            }

            if (!str_add_int(str, dims[i]))
                return false;
        }

        if (!str_add_str(str, ")"))
            return false;
    }

    return true;
}

static bool letToString(basic_line_t *line, uint32_t str)
{
    int index = 6;
    uint32_t var = getU32(line, 2);
    const char* varname = basic_var_get_name(var);

    if (line->tokens_[1] == BTOKEN_ARRAY) 
    {
        basic_err_t err;

        int dims[BASIC_MAX_DIMS];
        int dimcnt = basic_var_get_dim_count(var, &err);

        if (dimcnt == -1)
            return false;

        if (!str_add_str(str, varname))
            return false;

        if (!dimsToString(str, line, &index, dimcnt, dims))
            return false;
    }
    else if (line->tokens_[1] == BTOKEN_VAR)
    {
        if (!str_add_str(str, varname))
            return false ;
    }
    else {
        assert(false);
    }

    if (!str_add_str(str, "=")) {
        return false ;
    }

    assert(line->tokens_[index] == BTOKEN_EXPR);
    index++;

    uint32_t strh = basic_expr_to_string(getU32(line, index)) ;
    if (strh == STR_INVALID) 
    {
        return false ;
    }

    if (!str_add_handle(str, strh)) {
        str_destroy(strh);
        return false ;
    }
    str_destroy(strh);

    return true ;
}

static bool printToString(basic_line_t *line, uint32_t str)
{
    int index = 1 ;
    while (index < line->count_) 
    {
        assert(line->tokens_[index] == BTOKEN_EXPR);
        index++ ;

        uint32_t exprindex = getU32(line, index);
        uint32_t strh = basic_expr_to_string(exprindex);
        if (!str_add_handle(str, strh)) {
            str_destroy(strh) ;
            str_destroy(str) ;
            return STR_INVALID ;
        }
        str_destroy(strh);
        index += 4 ;

        if (index == line->count_)
            break; 

        if (line->tokens_[index] == BTOKEN_COMMA) {
            if (!str_add_str(str, ",")) {
                str_destroy(str) ;
                return STR_INVALID ;
            }
            index++ ;
        }
        else if (line->tokens_[index] == BTOKEN_SEMICOLON) {
            if (!str_add_str(str, ";")) {
                str_destroy(str) ;
                return STR_INVALID ;
            }
            index++ ;
        }
        else {
            str_destroy(str) ;
            return STR_INVALID ;            
        }
    }

    return true ;
}

static bool remToString(basic_line_t *line, uint32_t str)
{
    return str_add_str(str, line->extra_) ;
}

static bool oneLineToString(basic_line_t *line, uint32_t str)
{
    if (!str_add_str(str, basic_token_to_str(line->tokens_[0]))) {
        return false ;
    }

    if (!str_add_str(str, " ")) {
        return false ;
    }

    switch(line->tokens_[0])
    {
        case BTOKEN_FOR:
            break ;

        case BTOKEN_GOSUB:
        case BTOKEN_GOTO:
            break ;

        case BTOKEN_IF:
            break ;

        case BTOKEN_REM:
            if (!remToString(line, str))
                return false ;
            break; 

        case BTOKEN_DIM:
            if (!dimToString(line, str))
                return false;
            break ;

        case BTOKEN_LET:
            if (!letToString(line, str))
                return false ;
            break ;

        case BTOKEN_PRINT:
            if (!printToString(line, str))
                return false ;
            break ;

        case BTOKEN_SAVE:
        case BTOKEN_LOAD:
            break ;

        default:    // No additional args
            break ;
    }

    return true ;
}

uint32_t lineToString(basic_line_t *line)
{
    uint32_t str ;

    str = str_create() ;
    if (!str_add_int(str, line->lineno_)) {
        str_destroy(str) ;
        return STR_INVALID ;
    }

    if (!str_add_str(str, " ")) {
        str_destroy(str) ;
        return STR_INVALID ;        
    }

    if (!oneLineToString(line, str)) {
        str_destroy(str) ;
        return STR_INVALID ;
    }

    basic_line_t *child = line->children_ ;

    while (child) {
        if (!str_add_str(str, ":")) {
            str_destroy(str) ;
            return STR_INVALID ;
        }

        if (!oneLineToString(child, str)) {
            str_destroy(str) ;
            return STR_INVALID ;
        }

        child = child->next_ ;
    }

    return str ;
}

static void replace_line(basic_line_t *stay, basic_line_t *newone)
{
    int tmpcnt ;
    uint8_t *tmptokens ;

    tmpcnt = stay->count_ ;
    stay->count_ = newone->count_ ;
    newone->count_ = tmpcnt ;

    tmptokens = stay->tokens_ ;
    stay->tokens_ = newone->tokens_ ;
    newone->tokens_ = tmptokens ;

    basic_destroy_line(newone) ;
}

void basic_store_line(basic_line_t *line)
{
    if (program == NULL) {
        program = line ;
    }
    else if (line->lineno_ < program->lineno_) {
        line->next_ = program ;
        program = line ;
    }
    else {
        basic_line_t *tmp = program ;
        while (tmp->next_ != NULL && line->lineno_ >= tmp->next_->lineno_) {
            tmp = tmp->next_ ;
        }

        if (tmp->next_ == NULL) {
            if (tmp->lineno_ == line->lineno_) {
                replace_line(tmp, line) ;
            }
            else {
                tmp->next_ = line ;
            }
        }
        else {
            if (tmp->next_->lineno_ == line->lineno_) {
                replace_line(tmp->next_, line) ;
            }
            else {
                line->next_ = tmp->next_ ;
                tmp->next_ = line ;
            }
        }
    }
}

void basic_cls(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    cy_rslt_t res ;

    *err = BASIC_ERR_NONE ;

    int len = (int)strlen(clearscreen) ;
    res = (*outfn)(clearscreen, len) ;
    if (res != CY_RSLT_SUCCESS) {
        *err = BASIC_ERR_NETWORK_ERROR ;
    }
}


void basic_run(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;    
    }

    exec_context_t context ;
    context.line_ = program ;
    context.child_ = NULL ;

    *err = basic_exec_line(&context, outfn) ;
}

void basic_list(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    static const char *empty_message = "No Program Stored\n" ;

    *err = BASIC_ERR_NONE ;

    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }

    if (program == NULL) {
        (*outfn)(empty_message,  (int)strlen(empty_message));
    }
    else {
        basic_line_t *pgm = program ;
        while (pgm) {
            uint32_t str = lineToString(pgm) ;

            if (str == STR_INVALID) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return ;
            }

            if (!str_add_str(str, "\n")) {
                str_destroy(str);
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return ;
            }

            const char *strval = str_value(str) ;
            (*outfn)(strval, (int)strlen(strval));
            str_destroy(str) ;
            
            pgm = pgm->next_ ;
        }
    }
}



void basic_clear(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_line_t *pgm = program ;

    *err = BASIC_ERR_NONE ;    

    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;
    }    

    while (pgm) {
        basic_line_t *next = pgm->next_ ;
        basic_destroy_line(pgm) ;
        pgm = next ;
    }

    program = NULL ;
}

void basic_print(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    int index = 1 ;
    int len = 0 ;
    static char fmtbuf[32];

    *err = BASIC_ERR_NONE ;    

    while (index < line->count_) {
        assert(line->tokens_[index] == BTOKEN_EXPR) ;
        index++ ;

        uint32_t expr = getU32(line, index) ;
        index += 4 ;
        basic_value_t *value = basic_eval_expr(expr, err);
        if (value == NULL)
            return ;

        const char* str;
        if (value->type_ == BASIC_VALUE_TYPE_STRING) {
            str = value->value.svalue_;
        }
        else {
            if ((value->value.nvalue_ - (int)value->value.nvalue_) < 1e-6)
                sprintf(fmtbuf, "%d", (int)value->value.nvalue_);
            else
                sprintf(fmtbuf, "%f", value->value.nvalue_);

            str = fmtbuf;
        }

        len += (int)strlen(str) ;
        (*outfn)(str, (int)strlen(str)) ;

        if(index == line->count_)
            break ;

        assert(line->tokens_[index] == BTOKEN_COMMA || line->tokens_[index] == BTOKEN_SEMICOLON) ;

        if (line->tokens_[index] == BTOKEN_COMMA) {
            int count = 8 - (len % 8) ;
            putSpaces(outfn, count) ;
            len += count;
        }

        index++ ;
    }

    if (line->tokens_[line->count_ - 1] != BTOKEN_SEMICOLON)
        (*outfn)("\n", 1) ;

    return ;
}

void basic_rem(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;
}

void basic_let_simple(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    // We expect a token pattern of BTOKEN_LET, VAR, EXPR
    assert(line->count_ == 11);
    assert(line->tokens_[0] == BTOKEN_LET) ;
    assert(line->tokens_[1] == BTOKEN_VAR) ;
    assert(line->tokens_[6] == BTOKEN_EXPR) ;

    *err = BASIC_ERR_NONE ;    

    uint32_t varindex = getU32(line, 2) ;
    uint32_t exprindex = getU32(line, 7) ;

    basic_value_t *value = basic_eval_expr(exprindex, err) ;
    if (value == NULL)
        return ;

    if (!basic_var_set_value(varindex, value, err))
        return ;

    return ;
}

void basic_let_array(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    int dimcnt ;
    int dims[BASIC_MAX_DIMS] ;
    int index = 6 ;

    // We expect a token pattern of BTOKEN_LET, VAR, EXPR
    assert(line->tokens_[0] == BTOKEN_LET) ;
    assert(line->tokens_[1] == BTOKEN_ARRAY) ;

    *err = BASIC_ERR_NONE ;    

    uint32_t varindex = getU32(line, 2) ;
    
    dimcnt = basic_var_get_dim_count(varindex, err) ;
    if (dimcnt == -1)
        return ;

    for(int i = 0 ; i < dimcnt ; i++) {
        assert(line->tokens_[index] == BTOKEN_EXPR);
        index++;

        uint32_t exprindex = getU32(line, index) ;
        index += 4 ;

        basic_value_t* value = basic_eval_expr(exprindex, err);
        if (value == NULL)
            return;

        if (value->type_ == BASIC_VALUE_TYPE_STRING) {
            *err = BASIC_ERR_TYPE_MISMATCH;
            return;
        }

        dims[i] = (int)value->value.nvalue_;
    }

    assert(line->tokens_[index] == BTOKEN_EXPR);
    index++;

    uint32_t exprindex = getU32(line, index) ;

    basic_value_t *value = basic_eval_expr(exprindex, err) ;
    if (value == NULL)
        return ;

    if (!basic_var_set_array_value(varindex, value, dims, err))
        return ;

    return ;
}

void basic_let(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    if (line->tokens_[1] == BTOKEN_VAR)
        basic_let_simple(line, err, outfn) ;
    else
        basic_let_array(line, err, outfn) ;
}

void basic_dim(basic_line_t* line, basic_err_t* err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE;
}

void basic_if(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_then(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_else(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_for(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_to(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return  ;
}

void basic_goto(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_gosub(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

static void exec_one_statement(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    switch(line->tokens_[0]) {
        case BTOKEN_CLS:
            basic_cls(line, err, outfn) ;
            break ;

        case BTOKEN_RUN:
            basic_run(line, err, outfn) ;
            break ;

        case BTOKEN_LIST:
            basic_list(line, err, outfn) ;
            break ;

        case BTOKEN_CLEAR:
            basic_clear(line, err, outfn) ;
            break ;

        case BTOKEN_FLIST:
            basic_flist(line, err, outfn) ;
            break ;            

        case BTOKEN_LET:
            basic_let(line, err, outfn) ;
            break ;

        case BTOKEN_DIM:
            basic_dim(line, err, outfn);
            break;

        case BTOKEN_REM:
            basic_rem(line, err, outfn) ;
            break ;

        case BTOKEN_PRINT:
            basic_print(line, err, outfn) ;
            break;

        case BTOKEN_IF:
            basic_if(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_THEN:
            basic_then(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_ELSE:
            basic_else(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_FOR:
            basic_for(line, err, outfn) ;
            break ;

        case BTOKEN_TO:
            basic_to(line, err, outfn) ;
            break ;

        case BTOKEN_GOTO:
            basic_goto(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_GOSUB:
            basic_gosub(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_SAVE:
            basic_save(line, err, outfn) ;
            break ;

        case BTOKEN_LOAD:
            basic_load(line, err, outfn) ;
            break ;        

        default:
            *err = BASIC_ERR_UNKNOWN_KEYWORD ;
            break ;
    }
}

int basic_exec_line(exec_context_t *context, basic_out_fn_t outfn)
{
    exec_context_t nextone ;
    basic_err_t code = BASIC_ERR_NONE ;
    basic_line_t *toexec ;
    static char tbuf[256];

    while (context->line_ != NULL) {
        nextone.line_ = NULL ;
        nextone.child_ = NULL ;

        if (context->child_ != NULL)
            toexec = context->child_ ;
        else
            toexec = context->line_ ;

        if (trace && context->line_->lineno_ != -1) {
            sprintf(tbuf, "Executing line %d: '", context->line_->lineno_);
            (*outfn)(tbuf, (int)strlen(tbuf));

            uint32_t str;
            str = str_create();
            if (oneLineToString(toexec, str)) {
                const char* strval = str_value(str);
                (*outfn)(strval, (int)strlen(strval));
            }
            str_destroy(str);
            (*outfn)("'\n", 2);
        }

        //
        // Execute one statement exactly.  If the statement wants to redirect
        // control flow, it must return a value in the nextone context.
        //
        exec_one_statement(toexec, &nextone, &code, outfn) ;

        // Error executing the last line
        if (code != BASIC_ERR_NONE)
            break ;

        if (nextone.line_ != NULL) {
            *context = nextone ;
        }
        else {
            if (context->child_ != NULL) {
                //
                // We are executing a child statement
                //
                if (context->child_->next_ == NULL) 
                {
                    //
                    // This is the last child in the list of children, move to the
                    // next parent.
                    //
                    context->line_ = context->line_->next_ ;
                    context->child_ = NULL ;
                }
                else
                {
                    //
                    // There are further children
                    //
                    context->child_ = context->child_->next_ ;
                }
            }
            else {
                //
                // We are executing a parent statement
                //
                if (context->line_->children_ == NULL) {
                    //
                    // There are not children, go to the next parent
                    //
                    context->line_ = context->line_->next_ ;
                    context->child_ = NULL ;
                }
                else 
                {
                    //
                    // There are children, run them
                    //
                    context->child_ = context->line_->children_ ;
                }
            }
        }
    }

    return code ;
}
