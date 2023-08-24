#include "basicexec.h"
#include "networksvc.h"
#include "basicerr.h"
#include "basicproc.h"
#include "basicexpr.h"
#include "mystr.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ff.h>

static basic_line_t *program = NULL ;
static const char *clearscreen = "\x1b[2J\x1b[;H";
static char filename[64] ;

static bool letToString(basic_line_t *line, uint32_t str)
{
    assert(line->tokens_[1] == BTOKEN_VAR);
    const char *varname = basic_get_var_name(getU32(line, 2)) ;
    if (!str_add_str(str, varname)) {
        return false ;
    }

    if (!str_add_str(str, "=")) {
        return false ;
    }

    assert(line->tokens_[6] == BTOKEN_EXPR);
    uint32_t strh = basic_expr_to_string(getU32(line, 7)) ;
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
        if (index != 1) {
            if (!str_add_str(str, ",")) {
                str_destroy(str) ;                            
                return STR_INVALID ;
            }
        }
        assert(line->tokens_[index] == BTOKEN_EXPR);
        index++ ;
        uint32_t strh = basic_expr_to_string(getU32(line, index));
        if (!str_add_handle(str, strh)) {
            str_destroy(strh) ;
            str_destroy(str) ;
            return STR_INVALID ;
        }
        str_destroy(strh);
        index += 4 ;

        if (index == line->count_)
            break; 

        if (!str_add_str(str, ",")) {
            str_destroy(str) ;
            return STR_INVALID ;
        }
    }

    return true ;
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
            break; 

        case BTOKEN_DIM:
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

    return str ;
}

static uint32_t lineToString(basic_line_t *line)
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

basic_line_t *basic_cls(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    cy_rslt_t res ;

    *err = BASIC_ERR_NONE ;

    int len = strlen(clearscreen) ;
    res = (*outfn)(clearscreen, len) ;
    if (res != CY_RSLT_SUCCESS) {
        *err = BASIC_ERR_NETWORK_ERROR ;
        return NULL ;
    }

    if (line == NULL) {
        return NULL ;
    }

    return line->next_ ;
}

basic_line_t *basic_run(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    if (line->lineno_ != -1) {
        *err = BASIC_ERR_NOT_ALLOWED ;
        return NULL ;
    }

    *err = basic_exec_line(program, outfn) ;
    return NULL ;
}

basic_line_t *basic_list(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    static const char *empty_message = "No Program Stored\n" ;

    if (program == NULL) {
        (*outfn)(empty_message, strlen(empty_message));
    }
    else {
        basic_line_t *pgm = program ;
        while (pgm) {
            uint32_t str = lineToString(pgm) ;

            if (str == STR_INVALID) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return NULL ;
            }

            if (!str_add_str(str, "\n")) {
                str_destroy(str);
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return NULL ;                
            }

            const char *strval = str_value(str) ;
            (*outfn)(strval, strlen(strval));
            str_destroy(str) ;
            
            pgm = pgm->next_ ;
        }
    }

    if (line == NULL)
        return NULL ;

    return line->next_ ;
}

basic_line_t *basic_flist(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    DIR dp ;
    FILINFO info ;
    const int tabno = 16 ;
    char outline[64], num[12] ;

    FRESULT res = f_opendir (&dp, "/") ;
    if (res != FR_OK) {
        *err = BASIC_ERR_SDCARD_ERROR ;
        return NULL ;
    }

    strcpy(outline, "FileName        Size\n");
    (*outfn)(outline, strlen(outline));
    strcpy(outline, "============================\n") ;
    (*outfn)(outline, strlen(outline));

    const char *name ;
    while (true) {
        res = f_readdir (&dp, &info);
        if (res != FR_OK || info.fname[0] == '\0')
            break;

        const char *ext = strrchr(info.fname, '.') ;
        if (strcasecmp(ext, ".bas") != 0)
            continue ;

        name = info.fname ;
        if (name[0] == '\0')
            name = info.altname ;

        int index = 0 ;
        int src = 0 ;
        while (name[src] != '\0')
            outline[index++] = name[src++] ;
        
        while(index < tabno)
            outline[index++] = ' ' ;

        sprintf(num, "%ld", info.fsize);
        src = 0 ;
        while (num[src] != '\0')
            outline[index++] = num[src++] ;
       
        outline[index] = '\n' ;
        outline[index] = '\0' ;

        (*outfn)(outline, strlen(outline));
    }

    (*outfn)("\n", 1);
    f_closedir(&dp);
    return line->next_ ;
}

basic_line_t *basic_clear(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_line_t *pgm = program ;

    while (pgm) {
        basic_line_t *next = pgm->next_ ;
        basic_destroy_line(pgm) ;
        pgm = next ;
    }

    program = NULL ;
    return NULL ;
}

basic_line_t *basic_print(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    int index = 1 ;

    while (index < line->count_) {
        assert(line->tokens_[index] == BTOKEN_EXPR) ;
        index++ ;

        uint32_t expr = getU32(line, index) ;
        index += 4 ;
        basic_value_t *value = basic_eval_expr(expr, err);
        if (value == NULL)
            return NULL ;

        const char *str = basic_value_to_string(value) ;
        (*outfn)(str, strlen(str)) ;

        if(index == line->count_)
            break ;

        assert(line->tokens_[index] == BTOKEN_COMMA);
        index++ ;
    }

    (*outfn)("\n", 1) ;
    return line->next_ ;
}

basic_line_t *basic_rem(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return line->next_ ;
}

basic_line_t *basic_let(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    // We expect a token pattern of BTOKEN_LET, VAR, EXPR
    assert(line->count_ == 11);
    assert(line->tokens_[0] == BTOKEN_LET) ;
    assert(line->tokens_[1] == BTOKEN_VAR) ;
    assert(line->tokens_[6] == BTOKEN_EXPR) ;

    uint32_t varindex = getU32(line, 2) ;
    uint32_t exprindex = getU32(line, 7) ;

    basic_value_t *value = basic_eval_expr(exprindex, err) ;
    if (value == NULL)
        return NULL ;

    if (!basic_set_var_value(varindex, value, err))
        return NULL ;

    return line->next_ ;
}

basic_line_t *basic_if(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_then(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_else(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_for(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_to(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_goto(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_gosub(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    return NULL ;
}

basic_line_t *basic_save(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    FIL fp ;
    FRESULT res ;
    UINT written ;

    uint32_t expr = getU32(line, 1);
    basic_value_t *value = basic_eval_expr(expr, err) ;
    if (value == NULL)
        return NULL ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    res = f_open(&fp, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return false ;
    }

    basic_line_t *pgm = program ;
    while (pgm)
    {
        uint32_t str = lineToString(pgm);
        if (str == STR_INVALID) {
            f_close(&fp) ;
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        if (!str_add_str(str, "\n")) {
            f_close(&fp) ;
            str_destroy(str);
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;                
        }        

        const char *strval = str_value(str) ;
        UINT towrite = strlen(strval) ;
        res = f_write(&fp, strval, towrite, &written) ;
        str_destroy(str) ;
        if (res != FR_OK || towrite != written) {
            f_close(&fp) ;
            *err = BASIC_ERR_IO_ERROR ;
            return NULL ;
        }
    }
    f_close(&fp) ;

    return line->next_ ;
}

basic_line_t *basic_load(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    assert(line->tokens_[1] == BTOKEN_EXPR) ;

    uint32_t expr = getU32(line, 2);
    basic_value_t *value = basic_eval_expr(expr, err) ;
    if (value == NULL)
        return NULL ;

    if (value->type_ != BASIC_VALUE_TYPE_STRING) {
        *err = BASIC_ERR_TYPE_MISMATCH ;
        return NULL ;
    }

    strcpy(filename, "/") ;
    strcat(filename, value->value.svalue_);

    if (!basic_proc_load(filename, err, outfn))
        return NULL ;
    
    
    return NULL ;
}

static basic_line_t *exec_one_line(basic_line_t *line, basic_err_t *err, basic_out_fn_t outfn)
{
    basic_line_t *next = line->next_ ;
    assert(line != NULL) ;
    assert(line->tokens_ != NULL) ;

    *err = BASIC_ERR_NONE;

    switch(line->tokens_[0]) {
        case BTOKEN_CLS:
            next = basic_cls(line, err, outfn) ;
            break ;

        case BTOKEN_RUN:
            next = basic_run(line, err, outfn) ;
            break ;

        case BTOKEN_LIST:
            next = basic_list(line, err, outfn) ;
            break ;

        case BTOKEN_CLEAR:
            next = basic_clear(line, err, outfn) ;
            break ;

        case BTOKEN_FLIST:
            next = basic_flist(line, err, outfn) ;
            break ;            

        case BTOKEN_LET:
            next = basic_let(line, err, outfn) ;
            break ;

        case BTOKEN_REM:
            next = basic_rem(line, err, outfn) ;
            break ;

        case BTOKEN_PRINT:
            next = basic_print(line, err, outfn) ;
            break;

        case BTOKEN_IF:
            next = basic_if(line, err, outfn) ;
            break ;

        case BTOKEN_THEN:
            next = basic_then(line, err, outfn) ;
            break ;

        case BTOKEN_ELSE:
            next = basic_else(line, err, outfn) ;
            break ;

        case BTOKEN_FOR:
            next = basic_for(line, err, outfn) ;
            break ;

        case BTOKEN_TO:
            next = basic_to(line, err, outfn) ;
            break ;

        case BTOKEN_GOTO:
            next = basic_goto(line, err, outfn) ;
            break ;

        case BTOKEN_GOSUB:
            next = basic_gosub(line, err, outfn) ;
            break ;

        case BTOKEN_SAVE:
            next = basic_save(line, err, outfn) ;
            break ;

        case BTOKEN_LOAD:
            next = basic_load(line, err, outfn) ;
            break ;        

        default:
            *err = BASIC_ERR_UNKNOWN_KEYWORD ;
            next = NULL ;
    }

    return next ;
}

int basic_exec_line(basic_line_t *line, basic_out_fn_t outfn)
{
    basic_err_t code = BASIC_ERR_NONE ;

    while (1) {
        line = exec_one_line(line, &code, outfn) ;

        // Error executing the last line
        if (code != BASIC_ERR_NONE)
            break ;

        // We are done with the lines
        if (line == NULL)
            break ;
    }

    return code ;
}
