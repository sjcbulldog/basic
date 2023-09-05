#include "basicexec.h"
#include "basicerr.h"
#include "basicproc.h"
#include "basicexpr.h"
#include "basiccfg.h"
#include "basicstr.h"
#include "basictask.h"
#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

extern void basic_save(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_load(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_flist(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_del(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;
extern void basic_rename(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn) ;

basic_line_t *program = NULL ;
static const char *clearscreen = "\x1b[2J\x1b[;H";
static const char *spaces = "        " ;
static int space_count = 8 ;

static for_stack_entry_t *for_stack = NULL ;
static gosub_stack_entry_t *gosub_stack = NULL ;
static uint8_t end_program = BTOKEN_RUN ;
static bool trace = false ;

static basic_line_t *find_line_by_number(uint32_t lineno)
{
    for(basic_line_t *line = program ; line ; line = line->next_) {
        if (line->lineno_ == lineno)
            return line ;
    }

    return NULL ;
}

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

static void forwardOneStmt(exec_context_t *ctxt)
{
    if (ctxt->child_ == NULL) {
        if (ctxt->line_->children_) {
            ctxt->child_ = ctxt->line_->children_ ;
        }
        else {
            ctxt->line_ = ctxt->line_->next_ ;
        }
    }
    else {
        ctxt->child_ = ctxt->child_->next_ ;
        if (ctxt->child_ == NULL) {
            ctxt->line_ = ctxt->line_->next_ ;
        }
    }
}

static bool dimToString(basic_line_t* line, uint32_t str)
{
    int index = 1;
    uint32_t dimcnt, varnameidx ;

    while (index < line->count_)
    {
        if (index != 1) {
            if (!basic_str_add_str(str, ","))
                return false;
        }

        varnameidx = getU32(line, index);
        index += 4;

        const char *varname = basic_str_value(varnameidx) ;
        assert(varname != NULL) ;

        if (!basic_str_add_str(str, varname))
            return false;

        dimcnt = getU32(line, index);
        index += 4;

        if (!basic_str_add_str(str, "("))
            return false;

        for (uint32_t i = 0; i < dimcnt; i++) {
            if (i != 0) {
                if (!basic_str_add_str(str, ","))
                    return false;
            }

            uint32_t tmp = getU32(line, index);
            index += 4;

            if (!basic_str_add_int(str, tmp))
                return false;
        }

        if (!basic_str_add_str(str, ")"))
            return false;
    }

    return true;
}

static bool letSimpleToString(basic_line_t* line, uint32_t str)
{
    uint32_t varnameidx = getU32(line, 1);
    uint32_t expridx = getU32(line, 5);

    const char* varname = basic_str_value(varnameidx);
    assert(varname);
    if (!basic_str_add_str(str, varname))
        return false;

    if (!basic_str_add_str(str, "=")) {
        return false;
    }

    uint32_t strh = basic_expr_to_string(expridx);
    if (strh == BASIC_STR_INVALID)
    {
        return false;
    }

    if (!basic_str_add_handle(str, strh)) {
        basic_str_destroy(strh);
        return false;
    }
    basic_str_destroy(strh);
    return true;
}

static bool letArrayToString(basic_line_t *line, uint32_t str)
{
    int index = 1;

    uint32_t varnameidx = getU32(line, index);
    index += 4;

    const char* varname = basic_str_value(varnameidx) ;
    assert(varname != NULL) ;

    if (!basic_str_add_str(str, varname))
        return false;

    if (!basic_str_add_str(str, "("))
        return false;

    uint32_t dimcnt = getU32(line, index);
    index += 4;

    for (uint32_t i = 0; i < dimcnt; i++) {
        if (i != 0) {
            if (!basic_str_add_str(str, ","))
                return false;
        }

        uint32_t dimexpr = getU32(line, index);
        index += 4;

        uint32_t strh = basic_expr_to_string(dimexpr);
        if (strh == BASIC_STR_INVALID)
        {
            return false;
        }

        if (!basic_str_add_handle(str, strh)) {
            basic_str_destroy(strh);
            return false;
        }
    }

    if (!basic_str_add_str(str, ")="))
        return false;

    uint32_t exprh = getU32(line, index);
    uint32_t strh = basic_expr_to_string(exprh);
    if (strh == BASIC_STR_INVALID) 
    {
        return false ;
    }

    if (!basic_str_add_handle(str, strh)) {
        basic_str_destroy(strh);
        return false ;
    }
    basic_str_destroy(strh);

    return true ;
}

static bool exprToString(int cnt, basic_line_t* line, uint32_t str)
{
    while (cnt-- > 0) {
        uint32_t exprindex = getU32(line, 1);
        uint32_t strh = basic_expr_to_string(exprindex);
        if (!basic_str_add_handle(str, strh)) {
            basic_str_destroy(strh);
            return false;
        }
        basic_str_destroy(strh);

        if (!basic_str_add_str(str, " "))
            return false ;
    }

    return true;
}

static bool varsToString(basic_line_t *line, uint32_t str)
{
    if (line->count_ > 1) {
        uint32_t exprindex = getU32(line, 1);
        uint32_t strh = basic_expr_to_string(exprindex);
        if (!basic_str_add_handle(str, strh)) {
            basic_str_destroy(strh) ;
            return false;
        }
        basic_str_destroy(strh);        
    }
    return true ;
}

static bool printToString(basic_line_t *line, uint32_t str)
{
    int index = 1 ;
    while (index < line->count_) 
    {
        uint32_t exprindex = getU32(line, index);
        uint32_t strh = basic_expr_to_string(exprindex);
        if (!basic_str_add_handle(str, strh)) {
            basic_str_destroy(strh) ;
            return false;
        }
        basic_str_destroy(strh);
        index += 4 ;

        if (index == line->count_)
            break; 

        if (line->tokens_[index] == BTOKEN_COMMA) {
            if (!basic_str_add_str(str, ",")) {
                return false;
            }
            index++ ;
        }
        else if (line->tokens_[index] == BTOKEN_SEMICOLON) {
            if (!basic_str_add_str(str, ";")) {
                return false;
            }
            index++ ;
        }
        else {
            return false;
        }
    }

    return true ;
}

static bool remToString(basic_line_t *line, uint32_t str)
{
    return basic_str_add_str(str, line->extra_) ;
}

static bool nextToString(basic_line_t *line, uint32_t str)
{
    if (line->count_ == 5) {
        uint32_t idx = getU32(line, 1);
        const char* varname = basic_var_get_name(idx) ;
        if (!basic_str_add_str(str, varname)) {
            return false; 
        }
    }
    return true ;
}

static bool forToString(basic_line_t *line, uint32_t str)
{
    uint32_t idx, strh ;
    int index = 1 ;

    idx = getU32(line, index);
    const char* varname = basic_var_get_name(idx) ;
    if (!basic_str_add_str(str, varname))
        return false;    
    index += 4 ;

    if (!basic_str_add_str(str, " = "))
        return false ;

    idx = getU32(line, index);
    strh = basic_expr_to_string(idx);
    if (!basic_str_add_handle(str, strh)) {
        basic_str_destroy(strh) ;
        return false;
    }
    basic_str_destroy(strh);
    index += 4 ;

    if (!basic_str_add_str(str, " TO "))
        return false ;    

    idx = getU32(line, index);
    strh = basic_expr_to_string(idx);
    if (!basic_str_add_handle(str, strh)) {
        basic_str_destroy(strh) ;
        return false;
    }
    basic_str_destroy(strh);
    index += 4 ;

    if (index < line->count_) {
        if (!basic_str_add_str(str, "STEP"))
            return false ;   

        idx = getU32(line, index);
        strh = basic_expr_to_string(idx);
        if (!basic_str_add_handle(str, strh)) {
            basic_str_destroy(strh) ;
            return false;
        }
        basic_str_destroy(strh);
        index += 4 ;      
    }

    return true ;
}

static bool defToString(basic_line_t *line, uint32_t str)
{
    uint32_t fnindex = getU32(line, 1) ;
    uint32_t exprindex = getU32(line, 5) ;

    const char *fname = basic_userfn_name(fnindex);
    if (fname == NULL)
        return false ;

    uint32_t argcnt ;
    char **argnames = basic_userfn_args(fnindex, &argcnt);
    if (argnames == NULL)
        return false ;

    if (!basic_str_add_str(str, fname))
        return false ;

    if (!basic_str_add_str(str, "("))
        return false ;

    for(int i = 0 ; i < argcnt ; i++) 
    {
        if (i != 0) {
            if (!basic_str_add_str(str, ","))
                return false ;            
        }

        if (!basic_str_add_str(str, argnames[i]))
            return false ;
    }
    
    if (!basic_str_add_str(str, ")="))
        return false ;

    uint32_t strh = basic_expr_to_string(exprindex) ;
    if (!basic_str_add_handle(str, strh))
    {
        basic_str_destroy(strh) ;
        return false ;
    }

    return true ;
}

static bool ifToString(basic_line_t *line, uint32_t str)
{
    uint32_t exprindex = getU32(line, 1) ;

    uint32_t strh = basic_expr_to_string(exprindex) ;
    if (!basic_str_add_handle(str, strh))
    {
        basic_str_destroy(strh) ;
        return false ;
    }
    basic_str_destroy(strh);

    if (!basic_str_add_str(str, " THEN "))
        return false ;

    return true ;
}

static bool inputToString(basic_line_t *line, uint32_t str)
{
    uint32_t strh ;
    int index = 1 ;
    uint8_t token = getU32(line, index++) ;
    bool first = true ;

    if (token == BTOKEN_PROMPT) {
        strh = getU32(line, index) ;
        index += 4 ;

        if (!basic_str_add_str(str, "\""))
            return false ;

        if (!basic_str_add_handle(str, strh))
            return false;

        if (!basic_str_add_str(str, "\";"))
            return false ;            
    }

    while (index < line->count_) {
        if (!first) {
            if (!basic_str_add_str(str, ","))
                return false ;            
        }
        first = false ;
        strh = getU32(line, index) ;
        index += 4 ;    

        if (!basic_str_add_handle(str, strh))
            return false;        
    }

    return true ;
}

static bool onToString(basic_line_t *line, uint32_t str)
{
    int index = 1 ;
    uint32_t exprindex = getU32(line, index) ;
    index+= 4 ;

    uint32_t strh = basic_expr_to_string(exprindex) ;
    if (!basic_str_add_handle(str, strh))
    {
        basic_str_destroy(strh) ;
        return false ;
    }
    basic_str_destroy(strh);

    if (!basic_str_add_str(str, basic_token_to_str(line->tokens_[index++])))
        return false ;

    while (index < line->count_) {
        if (index != 6) {
            if (!basic_str_add_str(str, ","))
                return false ;              
        }
        uint32_t lineno = getU32(line, index) ;
        index += 4 ;
        if (!basic_str_add_int(str, (int)lineno))
            return false ;
    }

    return true;
}

static bool oneLineToString(basic_line_t *line, uint32_t str)
{
    if (!basic_str_add_str(str, basic_token_to_str(line->tokens_[0]))) {
        return false ;
    }

    if (!basic_str_add_str(str, " ")) {
        return false ;
    }

    switch(line->tokens_[0])
    {
        case BTOKEN_REM:
            if (!remToString(line, str))
                return false ;
            break; 

        case BTOKEN_DIM:
            if (!dimToString(line, str))
                return false;
            break ;

        case BTOKEN_LET_ARRAY:
            if (!letArrayToString(line, str))
                return false ;
            break ;

        case BTOKEN_LET_SIMPLE:
            if (!letSimpleToString(line, str))
                return false;
            break;

        case BTOKEN_LET:
            assert(false);
            break;

        case BTOKEN_PRINT:
            if (!printToString(line, str))
                return false ;
            break ;

        case BTOKEN_VARS:
            if (!varsToString(line, str))
                return false ;
            break ;

        case BTOKEN_SAVE:
        case BTOKEN_LOAD:
        case BTOKEN_DEL:
            if (!exprToString(1, line, str))
                return false;
            break ;

        case BTOKEN_RENAME:
            if (!exprToString(2, line, str))
                return false;
            break ;            

        case BTOKEN_DEF:
            if (!defToString(line, str))
                return false ;
            break ;

        case BTOKEN_FOR:
            if (!forToString(line, str))
                return false ;
            break ;

        case BTOKEN_GOTO:
        case BTOKEN_GOSUB:
            if (!basic_str_add_int(str, getU32(line, 1))) {
                return false ;
            }
            break ;

        case BTOKEN_NEXT:
            if (!nextToString(line, str))
                return false ;
            break ;

        case BTOKEN_RETURN:
        case BTOKEN_END:
        case BTOKEN_STOP:
        case BTOKEN_TRON:
        case BTOKEN_TROFF:
        case BTOKEN_CLS:
            break ;

        case BTOKEN_IF:
            if (!ifToString(line, str))
                return false ;
            break ;

        case BTOKEN_ON:
            if (!onToString(line, str))
                return false ;
            break ;

        case BTOKEN_INPUT:
            if (!inputToString(line, str))
                return false ;
            break;

        default:    // No additional args
            assert(false);
            break ;
    }

    return true ;
}

uint32_t lineToString(basic_line_t *line)
{
    uint32_t str ;
    basic_line_t *prev = NULL ;

    str = basic_str_create() ;
    if (!basic_str_add_int(str, line->lineno_)) {
        basic_str_destroy(str) ;
        return BASIC_STR_INVALID ;
    }

    if (!basic_str_add_str(str, " ")) {
        basic_str_destroy(str) ;
        return BASIC_STR_INVALID ;        
    }

    if (!oneLineToString(line, str)) {
        basic_str_destroy(str) ;
        return BASIC_STR_INVALID ;
    }

    basic_line_t *child = line->children_ ;
    prev = line ;

    while (child) {
        if (prev->tokens_[0] != BTOKEN_IF) {
            if (!basic_str_add_str(str, ":")) {
                basic_str_destroy(str) ;
                return BASIC_STR_INVALID ;
            }                
        }

        if (!oneLineToString(child, str)) {
            basic_str_destroy(str) ;
            return BASIC_STR_INVALID ;
        }

        prev = child ;
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
    else if (line->lineno_ == program->lineno_) {
        replace_line(program, line) ;
    }
    else if (line->lineno_ < program->lineno_) {
        line->next_ = program ;
        program = line ;
    }
    else {
        basic_line_t *tmp = program ;
        while (tmp->next_ != NULL && line->lineno_ > tmp->next_->lineno_) {
            tmp = tmp->next_ ;
        }

        if (tmp->next_ == NULL) {
            tmp->next_ = line ;
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
    *err = BASIC_ERR_NONE ;

    int len = (int)strlen(clearscreen) ;
    (*outfn)(clearscreen, len) ;
}

void basic_run(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    static const char *StoppedMessage = "Program Stopped" ;

    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return ;    
    }

    exec_context_t context ;
    context.line_ = program ;
    context.child_ = NULL ;
    context.next_ = NULL ;

    *err = basic_exec_line(&context, outfn) ;
    if (end_program == BTOKEN_STOP) {
        // TODO: Add line number to this message
        (outfn)(StoppedMessage, strlen(StoppedMessage)) ;
    }

    end_program = BTOKEN_RUN ;
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
        uint32_t start = getU32(line, 1) ;
        uint32_t end = getU32(line, 5);

        basic_line_t *pgm = program ;
        while (pgm && pgm->lineno_ <= end) {
            if (pgm->lineno_ >= start && pgm->lineno_ <= end) {
                uint32_t str = lineToString(pgm) ;

                if (str == BASIC_STR_INVALID) {
                    *err = BASIC_ERR_OUT_OF_MEMORY ;
                    return ;
                }

                if (!basic_str_add_str(str, "\n")) {
                    basic_str_destroy(str);
                    *err = BASIC_ERR_OUT_OF_MEMORY ;
                    return ;
                }

                const char *strval = basic_str_value(str) ;
                (*outfn)(strval, (int)strlen(strval));
                basic_str_destroy(str) ;
            }
            
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

    basic_str_clear_all() ;
    basic_var_clear_all() ;
    basic_expr_clear_all() ;
    basic_userfn_clear_all() ;
}

static char fmtbuf[32];
void basic_vars(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    const char *prefix = NULL ;

    if (line->count_ > 1) {
        uint32_t expr = getU32(line, 1) ;

        basic_value_t *value = basic_expr_eval(expr, 0, NULL, NULL, err);
        if (value == NULL)
            return ;        

        if (value->type_ != BASIC_VALUE_TYPE_STRING) 
        {
            *err = BASIC_ERR_TYPE_MISMATCH ;
            return ;
        }

        prefix = value->value.svalue_ ;
    }

    int cnt = basic_var_count() ;
    if (cnt > 0) {
        uint32_t *all = (uint32_t *)malloc(sizeof(uint32_t) * cnt) ;
        basic_var_get_all(all)  ;

        for(int i = 0 ; i < cnt ; i++) {
            const char *varname = basic_var_get_name(all[i]) ;
            if (varname == NULL)
                continue ;

            if (prefix == NULL || strncmp(prefix, varname, strlen(prefix)) == 0) 
            {
                basic_value_t *value = basic_var_get_value(all[i]) ;
                if (value->type_ == BASIC_VALUE_TYPE_NUMBER) {
                    if ((value->value.nvalue_ - (int)value->value.nvalue_) < 1e-6)
                        sprintf(fmtbuf, " %d ", (int)value->value.nvalue_);
                    else
                        sprintf(fmtbuf, " %f ", value->value.nvalue_);
                }

                (*outfn)(varname, strlen(varname)) ;
                (*outfn)(" = ", 3) ;
                if (value->type_ == BASIC_VALUE_TYPE_NUMBER)
                {
                    (*outfn)(fmtbuf, strlen(fmtbuf)) ;
                }
                else
                {
                    (*outfn)(value->value.svalue_, strlen(value->value.svalue_)) ;
                }
                (*outfn)("\n", 1) ;
            }
        }
        free(all) ;
    }
}

void basic_print(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    int index = 1 ;
    int len = 0 ;


    *err = BASIC_ERR_NONE ;    

    while (index < line->count_) {
        uint32_t expr = getU32(line, index) ;
        index += 4 ;

        basic_value_t *value = basic_expr_eval(expr, 0, NULL, NULL, err);
        if (value == NULL)
            return ;

        const char* str;
        if (value->type_ == BASIC_VALUE_TYPE_STRING) {
            str = value->value.svalue_;
        }
        else {
            if ((value->value.nvalue_ - (int)value->value.nvalue_) < 1e-6)
                sprintf(fmtbuf, " %d ", (int)value->value.nvalue_);
            else
                sprintf(fmtbuf, " %f ", value->value.nvalue_);

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
 
void basic_on(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    uint8_t token ;
    int index = 1 ;

    uint32_t exprindex = getU32(line, index) ;
    index += 4 ;

    token = line->tokens_[index++] ;
    assert(token == BTOKEN_GOTO || token == BTOKEN_GOSUB);    

    basic_value_t *value = basic_expr_eval(exprindex, 0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (value->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    int v = (int)(value->value.nvalue_) - 1;
    int lineidx = index + v * 4 ;
    if (lineidx + 4 <= line->count_) {
        int lineno = getU32(line, lineidx);
        basic_line_t *nline = find_line_by_number(lineno) ;
        if (nline == NULL) {
            *err = BASIC_ERR_INVALID_LINE_NUMBER ;
            return ;
        }

        if (token == BTOKEN_GOTO) {
            nextline->line_ = nline ;
            nextline->child_ = NULL ;
        }
        else {
            gosub_stack_entry_t *entry = (gosub_stack_entry_t *)malloc(sizeof(gosub_stack_entry_t)) ;
            if (entry == NULL) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return ;
            }

            entry->context_.line_ = current->line_ ;
            entry->context_.child_ = current->child_ ;
            forwardOneStmt(&entry->context_);

            entry->next_ = gosub_stack ;
            gosub_stack = entry ;

            nextline->line_ = nline ;    
            nextline->child_ = NULL ;
            *err = BASIC_ERR_NONE ;
        }
    }
}

void basic_input(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    static const char *InvalidNumber = "invalid number entered, try again" ;
    static const char *NotEnoughInput = "not enough input values supplied" ;

    const char *prompt = NULL ;
    int index = 1 ;
    uint32_t v, varidx ;
    char *stripped = NULL ;

    uint8_t token = line->tokens_[index++];
    if (token == BTOKEN_PROMPT) {
        v = getU32(line, index) ;
        index += 4 ;
        prompt = basic_str_value(v) ;
    }

    int saveindex = index ;
    bool isvalid = false ;

    //
    // Redirect input from the basic parser to here
    //
    basic_task_store_input(true) ;

    while (!isvalid) {
        //
        // Return index to the beginning of the variables
        //
        index = saveindex ;

        //
        // For this pass, assume we are valid until we see a reason to be invalid
        //
        isvalid = true ;

        //
        // Send the prompt to the user
        //
        if (prompt) {
            (*outfn)(prompt, strlen(prompt)) ;
            (*outfn)(" ", 1);            
        }    

        //
        // Get a line of text from the user
        //
        const char *text = basic_task_get_line() ;

        stripped = (char *)malloc(strlen(text) + 1) ;
        strcpy(stripped, text) ;
        int len = strlen(stripped) ;
        if (stripped[len - 1] == '\n')
            stripped[len - 1] = '\0' ;

        //
        // Parse the line of text
        //
        while (isvalid && index < line->count_) {
            //
            // Get the variable name, handle, and type
            //
            v = getU32(line, index) ;
            index += 4 ;

            const char *varname = basic_str_value(v) ;
            if (!basic_var_get(varname, &varidx, err))
                return ;
            bool str = basic_var_is_string(varidx) ;

            if (str) 
            {
                //
                // If the input variable is a string, all input is valid, store the input
                // and return.
                //
                basic_var_set_value_string(varidx, stripped, err) ;
            }
            else
            {
                double num ;
                
                text = skipSpaces(text) ;
                if (*text == '\0') {
                    //
                    // Not enough input
                    //
                    (outfn)(NotEnoughInput, strlen(NotEnoughInput)) ;
                    (outfn)("\n", 1) ;
                    isvalid = false ;
                    continue ;
                }

                text = basic_expr_parse_number(text, &num, err) ;
                if (text == NULL) 
                {
                    //
                    // Tell the user the output is not valid and stay in the
                    // input loop
                    //
                    (outfn)(InvalidNumber, strlen(InvalidNumber)) ;
                    (outfn)("\n", 1) ;
                    isvalid = false ;
                    continue ;
                }

                //
                // Valid number set the variables value and move to the next one
                //
                basic_var_set_value_number(varidx, num, err) ;

                if (index == line->count_)
                    break; 

                text = skipSpaces(text) ;
                if (*text != ',') {
                    *err = BASIC_ERR_EXPECTED_COMMA ;
                    isvalid = false ;
                    continue ;
                }
            }
        }
    }

    if (stripped)
        free(stripped) ;
    basic_task_store_input(false) ;
}

void basic_rem(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;
}

void basic_let_simple(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    assert(line->count_ == 9);
    *err = BASIC_ERR_NONE ;    

    uint32_t varindex ;
    uint32_t varnameidx = getU32(line, 1) ;
    uint32_t exprindex = getU32(line, 5) ;
    const char *varname = basic_str_value(varnameidx) ;
    assert(varname != NULL) ;

    if (!basic_var_get(varname, &varindex, err))
        return ;

    basic_value_t *value = basic_expr_eval(exprindex, 0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (!basic_var_set_value(varindex, value, err))
        return ;

    return ;
}

void basic_let_array(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    uint32_t dims[BASIC_MAX_DIMS] ;
    int index = 1;

    *err = BASIC_ERR_NONE ;    

    uint32_t varindex ;
    uint32_t varnameidx = getU32(line, index) ;
    const char *varname = basic_str_value(varnameidx) ;
    assert(varname != NULL) ;
    if (!basic_var_get(varname, &varindex, err))
        return ;
    index += 4 ;

    uint32_t dimcnt = getU32(line, index);
    index += 4;

    for(uint32_t i = 0 ; i < dimcnt ; i++) {
        uint32_t exprindex = getU32(line, index) ;
        index += 4 ;

        basic_value_t* value = basic_expr_eval(exprindex, 0, NULL, NULL, err);
        if (value == NULL)
            return;

        if (value->type_ == BASIC_VALUE_TYPE_STRING) {
            *err = BASIC_ERR_TYPE_MISMATCH;
            return;
        }

        if (value->value.nvalue_ < 0) {
            *err = BASIC_ERR_INVALID_DIMENSION;
            return;
        }

        dims[i] = (uint32_t)value->value.nvalue_;
    }

    uint32_t exprindex = getU32(line, index) ;

    basic_value_t *value = basic_expr_eval(exprindex,  0, NULL, NULL, err) ;
    if (value == NULL)
        return ;

    if (!basic_var_set_array_value(varindex, value, dims, err))
        return ;

    return ;
}

void basic_dim(basic_line_t* line, basic_err_t* err, basic_out_fn_t outfn)
{
    uint32_t dims[BASIC_MAX_DIMS];
    uint32_t varidx, dimcnt, varnameidx ;
    const char *varname ;
    int index = 1;

    while (index < line->count_) {
        varnameidx = getU32(line, index) ;
        index += 4 ;
        varname = basic_str_value(varnameidx) ;
        assert(varname != NULL) ;
        if (!basic_var_get(varname, &varidx, err))
            return ;

        dimcnt = getU32(line, index);
        index += 4;

        for (uint32_t i = 0; i < dimcnt; i++) {
            dims[i] = getU32(line, index);
            index += 4;
        }

        if (!basic_var_add_dims(varidx, dimcnt, dims, err))
            return;
    }

    *err = BASIC_ERR_NONE;
}

void basic_if(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_value_t *cond = basic_expr_eval(getU32(line, 1), 0, NULL, NULL, err);
    if (cond == NULL)
        return ;

    if (cond->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    if (cond->value.nvalue_ < 1.0e-6) {
        //
        // Conditional is false, jump to the next numbered line
        //
        nextline->line_ = current->line_->next_ ;
        nextline->child_ = NULL ;
    }

    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_then(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return ;
}

void basic_for(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    uint32_t varindex, expridx ;

    uint32_t varnameidx = getU32(line, 1) ;
    const char *varname = basic_str_value(varnameidx) ;
    assert(varname != NULL) ;
    if (!basic_var_get(varname, &varindex, err))
        return ;

    expridx = getU32(line, 5);

    basic_value_t *start = basic_expr_eval(expridx, 0, NULL, NULL, err);
    if (start == NULL)
        return ;

    if (!basic_var_set_value(varindex, start, err))
        return ;

    for_stack_entry_t *c = (for_stack_entry_t *)malloc(sizeof(for_stack_entry_t)) ;
    if (c == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return ;
    }

    c->varidx_ = varindex ;
    c->endidx_ = getU32(line, 9) ;
    if (line->count_ > 13) {
        c->stepidx_ = getU32(line, 13);
    }
    else {
        c->stepidx_ = 0xffffffff ;
    }

    c->context_.line_ = current->line_ ;
    c->context_.child_ = current->child_ ;
    forwardOneStmt(&c->context_);

    c->next_ = for_stack ;
    for_stack = c ;

    *err = BASIC_ERR_NONE ;
    return ;
}

void basic_next(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    double step = 1.0 ;

    if (for_stack == NULL) {
        *err = BASIC_ERR_UNMATCHED_NEXT ;
        return ;
    }

    //
    // Get the current loop variable value
    //
    basic_value_t *loopval = basic_var_get_value(for_stack->varidx_) ;
    if (loopval->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }

    //
    // Evaluate the end of for loop condition
    //
    basic_value_t *endval = basic_expr_eval(for_stack->endidx_, 0, NULL, NULL, err) ;
    if (endval == NULL)
        return ;
    if (endval->type_ != BASIC_VALUE_TYPE_NUMBER) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return ;
    }        

    //
    // Evaluate the step value as it is needed in the determination of the end of loop
    // condition
    //
    if (for_stack->stepidx_ != 0xffffffff) {
        basic_value_t *stepval = basic_expr_eval(for_stack->stepidx_, 0, NULL, NULL, err) ;
        if (stepval == NULL)
            return ;

        if (stepval->type_ != BASIC_VALUE_TYPE_NUMBER) {
            *err = BASIC_ERR_TYPE_MISMATCH ;
            return ;
        }

        step = stepval->value.nvalue_ ;
    }    

    if ((step < 0.0 && loopval->value.nvalue_ + step < endval->value.nvalue_) ||
        (step > 0.0 && loopval->value.nvalue_ + step > endval->value.nvalue_)) {
        //
        // The loop is done, remove the top entry from the for stack
        //
        basic_var_destroy(for_stack->varidx_) ;
        for_stack_entry_t *todel = for_stack ;
        for_stack = for_stack->next_ ;
        free(todel) ;
    }
    else {
        //
        // The loop is still running, get the step value
        //
        if (!basic_var_set_value_number(for_stack->varidx_, loopval->value.nvalue_ + step, err))
            return ;

        nextline->line_ = for_stack->context_.line_ ;
        nextline->child_ = for_stack->context_.child_ ;
    }
}

void basic_to(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    *err = BASIC_ERR_NONE ;    
    return  ;
}

void basic_goto(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_line_t *nline = find_line_by_number(getU32(line, 1)) ;
    if (nline == NULL) {
        *err = BASIC_ERR_INVALID_LINE_NUMBER ;
        return ;
    }
    nextline->line_ = nline ;
    nextline->child_ = NULL ;
    *err = BASIC_ERR_NONE ;
    return ;
}

void basic_gosub(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_line_t *nline = find_line_by_number(getU32(line, 1)) ;
    if (nline == NULL) {
        *err = BASIC_ERR_INVALID_LINE_NUMBER ;
        return ;
    }

    gosub_stack_entry_t *entry = (gosub_stack_entry_t *)malloc(sizeof(gosub_stack_entry_t)) ;
    if (entry == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return ;
    }

    entry->context_.line_ = current->line_ ;
    entry->context_.child_ = current->child_ ;
    forwardOneStmt(&entry->context_);

    entry->next_ = gosub_stack ;
    gosub_stack = entry ;


    nextline->line_ = nline ;    
    nextline->child_ = NULL ;
    *err = BASIC_ERR_NONE ;
    return ;
}

void basic_return(basic_line_t *line, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    if (gosub_stack == NULL) {
        *err = BASIC_ERR_NO_GOSUB ;
        return ;
    }

    nextline->line_ = gosub_stack->context_.line_ ;
    nextline->child_ = gosub_stack->context_.child_ ;

    gosub_stack_entry_t *todel = gosub_stack ;
    gosub_stack = gosub_stack->next_ ;
    free(todel) ;
    return ;
}

static int stmtNumber(basic_line_t *parent, basic_line_t *child)
{
    int cnt = 0 ;

    if (parent == child) 
    {
        cnt = 0 ;
    }
    else 
    {
        cnt = 1 ;
        basic_line_t *line = parent->children_ ;
        while (line && line != child) {
            cnt++ ;
            line = line->next_ ;
        }

        if (line == NULL)
            cnt = -1 ;
    }

    return cnt ;
}

static char trbuf[64] ;
static void exec_one_statement(basic_line_t *line, exec_context_t *current, exec_context_t *nextline, basic_err_t *err, basic_out_fn_t outfn)
{
    if (trace && current->line_->lineno_ != -1) {
        int stmt = stmtNumber(current->line_, line) ;
        if (stmt == 0) 
        {
            sprintf(trbuf, "[%d]", current->line_->lineno_);
        }
        else
        {
            sprintf(trbuf, "[%d:%d]", current->line_->lineno_, stmt) ;
        }

        (outfn)(trbuf, strlen(trbuf)) ;
    }

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

        case BTOKEN_DEL:
            basic_del(line, err,outfn) ;
            break ;

        case BTOKEN_RENAME:
            basic_rename(line, err,outfn) ;
            break ;            

        case BTOKEN_LET_SIMPLE:
            basic_let_simple(line, err, outfn) ;
            break ;

        case BTOKEN_LET_ARRAY:
            basic_let_array(line, err, outfn);
            break;

        case BTOKEN_LET:
            assert(false);
            break;

        case BTOKEN_DIM:
            basic_dim(line, err, outfn);
            break;

        case BTOKEN_REM:
            basic_rem(line, err, outfn) ;
            break ;

        case BTOKEN_PRINT:
            basic_print(line, err, outfn) ;
            break;

        case BTOKEN_VARS:
            basic_vars(line, err, outfn) ;
            break ;

        case BTOKEN_INPUT:
            basic_input(line, err, outfn) ;
            break ;

        case BTOKEN_THEN:
            basic_then(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_FOR:
            basic_for(line, current, nextline, err, outfn) ;
            break ;

        case BTOKEN_GOTO:
            basic_goto(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_GOSUB:
            basic_gosub(line, current, nextline, err, outfn) ;
            break ;            

        case BTOKEN_RETURN:
            basic_return(line, nextline, err, outfn) ;
            break ;

        case BTOKEN_NEXT:
            basic_next(line, current, nextline, err, outfn) ;
            break ;

        case BTOKEN_IF:
            basic_if(line, current, nextline, err, outfn);
            break ;

        case BTOKEN_TO:
            basic_to(line, err, outfn) ;
            break ;

        case BTOKEN_SAVE:
            basic_save(line, err, outfn) ;
            break ;

        case BTOKEN_LOAD:
            basic_load(line, err, outfn) ;
            break ;     

        case BTOKEN_ON:
            basic_on(line, current, nextline, err, outfn) ;
            break ;

        case BTOKEN_END:
            end_program = BTOKEN_END ;
            break; 

        case BTOKEN_TRON:
            trace = true ;
            break ;

        case BTOKEN_TROFF:
            trace = false ;
            break ;

        case BTOKEN_STOP:
            end_program = BTOKEN_STOP ;
            break ;            

        case BTOKEN_DEF:
            *err = BASIC_ERR_NONE ;
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

        //
        // Execute one statement exactly.  If the statement wants to redirect
        // control flow, it must return a value in the nextone context.
        //
        exec_one_statement(toexec, context, &nextone, &code, outfn) ;

        // Error executing the last line
        if (code != BASIC_ERR_NONE) {
            if (toexec->lineno_ != -1) {
                sprintf(tbuf, "Program failed: line %d: error code %d\n", toexec->lineno_, code) ;
                (*outfn)(tbuf, strlen(tbuf)) ;
            }
            break ;
        }

        if (end_program != BTOKEN_RUN)
            return BASIC_ERR_NONE ;

        if (nextone.line_ != NULL) 
        {
            *context = nextone ;
        }
        else {
            forwardOneStmt(context) ;
        }
    }

    return code ;
}
