#include "basicproc.h"
#include "basicerr.h"
#include "basicline.h"
#include "basicexec.h"
#include "basicexpr.h"
#include "networksvc.h"
#include <ff.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>

static const char *prompt = "Basic06> ";

static token_table_t tokens[] = 
{
    { BTOKEN_RUN, "run" },
    { BTOKEN_CLS, "cls" },
    { BTOKEN_LIST, "list"},
    { BTOKEN_CLEAR, "clear"},
    { BTOKEN_LET, "let"},
    { BTOKEN_REM, "rem"},
    { BTOKEN_IF, "if"},
    { BTOKEN_THEN, "then"},
    { BTOKEN_ELSE, "else"},
    { BTOKEN_ENDIF, "endif"},
    { BTOKEN_FOR, "for"},
    { BTOKEN_TO, "to"},
    { BTOKEN_GOTO, "goto"},
    { BTOKEN_GOSUB, "gosub"},
    { BTOKEN_SAVE, "save"},
    { BTOKEN_LOAD, "load"},
    { BTOKEN_STEP, "step"},
    { BTOKEN_LOAD, "load"},        
    { BTOKEN_PRINT, "print"},
    { BTOKEN_FLIST, "flist"}
} ;

static char linebuf[256] ;

const char *basic_token_to_str(uint8_t token)
{
    for(int i = 0 ; i < sizeof(tokens) / sizeof(tokens[0]) ; i++) {
        if (tokens[i].token_ == token)
            return tokens[i].str_ ;
    }

    return "!ERROR!" ;
}

static const char *parseInteger(const char *line, int *value)
{
    *value = 0 ;

    while (isdigit((uint8_t)*line)) {
        *value = *value * 10 + *line - '0' ;
        line++ ;
    }

    return line ;
}

static basic_line_t *create_line()
{
    basic_line_t *ret = (basic_line_t *)malloc(sizeof(basic_line_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->lineno_ = -1 ;
    ret->count_ = 0 ;
    ret->next_ = NULL ;
    ret->tokens_ = NULL ;

    return ret ;
}

void basic_destroy_line(basic_line_t *line)
{
    if (line->tokens_ != NULL) {
    	int index = 1 ;
    	while (index < line->count_) {
    		if (line->tokens_[index] == BTOKEN_VAR) {
    			uint32_t var = getU32(line + 1, index);
    			index += 5 ;
    			basic_destroy_var(var);
    		}
    		else if (line->tokens_[index] == BTOKEN_EXPR) {
    			uint32_t var = getU32(line, index + 1);
    			index += 5 ;
    			basic_destroy_expr(var);
    		}
    		else {
    			index++ ;
    		}
    	}
        free(line->tokens_) ;
    }

    free(line) ;
}

static bool add_token(basic_line_t *line, uint8_t token)
{
    if (line->count_ == 0) {
        // First token in a line
        line->tokens_ = (uint8_t *)malloc(sizeof(uint8_t)) ;
    }
    else {
        // Additional tokens in a line
        line->tokens_ = (uint8_t *)realloc(line->tokens_, sizeof(uint8_t) * (line->count_ + 1)) ;
    }

    if (line->tokens_ == NULL)
        return false ;

    line->tokens_[line->count_++] = token ;
    return true ;
}

static bool add_uint32(basic_line_t *line, uint32_t value)
{
    if (line->count_ == 0) {
        // First token in a line
        line->tokens_ = (uint8_t *)malloc(sizeof(uint32_t)) ;
    }
    else {
        // Additional tokens in a line
        line->tokens_ = (uint8_t *)realloc(line->tokens_, sizeof(uint8_t) * (line->count_ + sizeof(uint32_t))) ;
    }

    if (line->tokens_ == NULL)
        return false ;

    line->tokens_[line->count_++] = (value & 0xff) ;
    line->tokens_[line->count_++] = ((value >> 8) & 0xff) ;
    line->tokens_[line->count_++] = ((value >> 16) & 0xff) ;
    line->tokens_[line->count_++] = ((value >> 24) & 0xff) ;

    return true ;    
}

static const char *parse_keyword(const char *line, uint8_t *token, basic_err_t *err)
{
    char keyword[BASIC_MAX_KEYWORD_LENGTH + 1] ;
    int stored = 0 ;

    *err = BASIC_ERR_NONE ;

    while (isalpha((uint8_t)*line))
    {
        keyword[stored++] = *line++ ;
        if (stored == BASIC_MAX_KEYWORD_LENGTH + 1) {
            *err = BASIC_ERR_KEYWORD_TOO_LONG ;
            return NULL ;
        }
    }
    keyword[stored] = '\0' ;

    for(int i = 0 ; i < sizeof(tokens)/sizeof(tokens[0]) ; i++) {
        if (strcasecmp(tokens[i].str_, keyword) == 0) {
            *token = tokens[i].token_ ;
            return line ;
        }
    }

    *err = BASIC_ERR_UNKNOWN_KEYWORD ;
    return NULL ;
}

static const char *parse_varname(const char *line, uint32_t *varindex, basic_err_t *err)
{
    char keyword[BASIC_MAX_VARIABLE_LENGTH + 1] ;
    int stored = 0 ;

    *err = BASIC_ERR_NONE ;

    line = skipSpaces(line) ;

    while (isalpha((uint8_t)*line))
    {
        keyword[stored++] = *line++ ;
        if (stored == BASIC_MAX_VARIABLE_LENGTH + 1) {
            *err = BASIC_ERR_VARIABLE_TOO_LONG ;
            return NULL ;
        }
    }

    if (*line == '$') {
        keyword[stored++] = *line++ ;
        if (stored == BASIC_MAX_VARIABLE_LENGTH + 1) {
            *err = BASIC_ERR_VARIABLE_TOO_LONG ;
            return NULL ;
        }        
    }

    keyword[stored] = '\0' ;

    if (strlen(keyword) == 0) {
        *err = BASIC_ERR_EXPECTED_VAR ;
        return NULL ;
    }

    if (!basic_get_var(keyword, varindex, err)) {
        return NULL ;
    }

    return line ;
}

static bool parse_for(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint8_t token ;
    uint32_t varindex, startexpr, endexpr, stepexpr ;

    line = parse_varname(line, &varindex, err) ;
    if (line == NULL) {
        return false ;
    }

    line = skipSpaces(line) ;
    if (*line != '=') {
        *err = BASIC_ERR_EXPECTED_EQUAL ;
        return false ;
    }
    line++ ;

    line = basic_parse_expr(line, &startexpr, err) ;
    if (line == NULL) {
        return false ;
    }
    
    line = parse_keyword(line, &token, err) ;
    if (line == NULL) {
        return false ;
    }

    if (token != BTOKEN_TO) {
        *err = BASIC_ERR_EXPECTED_TO ;
        return false ;
    }

    line = basic_parse_expr(line, &endexpr, err) ;
    if (line == NULL) {
        return false ;
    }

    line = skipSpaces(line) ;
    if (*line != '\0') {
        line = parse_keyword(line, &token, err) ;
        if (line == NULL) {
            return false ;
        }

        if (token != BTOKEN_STEP) {
            *err = BASIC_ERR_EXPECTED_STEP ;
            return false ;
        }

        line = basic_parse_expr(line, &stepexpr, err) ;
        if (line == NULL) {
            return false ;
        }
    }
    else {
        line = basic_parse_expr("1", &stepexpr, err);
    }

    if (!add_token(bline, BTOKEN_VAR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, varindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, startexpr)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, endexpr)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, stepexpr)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }        

    return true ;
}

static bool parse_gotogosub(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    line = basic_parse_expr(line, &exprindex, err) ;
    if (line == NULL) {
        return false ;
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    return true ;
}

static bool parse_print(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    line = skipSpaces(line) ;
    if (*line == '\0') {
        *err = BASIC_ERR_NONE ;

        return true ;
    }

    while (1) {
        line = basic_parse_expr(line, &exprindex, err) ;
        if (line == NULL) {
            return false ;
        }

        if (!add_token(bline, BTOKEN_EXPR)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return false ;
        }

        if (!add_uint32(bline, exprindex)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return false ;
        }      

        line = skipSpaces(line) ;
        if (*line == '\0') {
            break ;
        }
        else if (*line == ',') {
            line++ ;
            if (!add_token(bline, BTOKEN_COMMA)) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return false ;        
            }
        }
        else {
            *err = BASIC_ERR_EXTRA_CHARS ;
            return false ;
        }
    }

    return true ;
}

static bool parse_let(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t varindex, exprindex ;

    line = parse_varname(line, &varindex, err) ;
    if (line == NULL) {
        return false ;
    }

    line = skipSpaces(line) ;
    if (*line != '=') {
        *err = BASIC_ERR_EXPECTED_EQUAL ;
        return false ;
    }

    line++ ;

    line = basic_parse_expr(line, &exprindex, err) ;
    if (line == NULL) {
        return false ;
    }

    line = skipSpaces(line) ;
    if (*line != '\0') {
        *err = BASIC_ERR_EXTRA_CHARS ;
        return false ;
    }

    if (!add_token(bline, BTOKEN_VAR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, varindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }

    return true ;
}

static bool parse_if(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;
    uint8_t token ;

    line = basic_parse_expr(line, &exprindex, err) ;
    if (line == NULL) {
        return false ;
    }

    line = parse_keyword(line, &token, err) ;
    if (line == NULL) {
        return false ;
    }

    if (token != BTOKEN_THEN) {
        *err = BASIC_ERR_EXPECTED_THEN ;
        return false ;
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }    

    return true ;
}

static bool parse_loadsave(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    line = basic_parse_expr(line, &exprindex, err) ;
    if (line == NULL) {
        return false ;
    }

    if (!add_token(bline, BTOKEN_EXPR)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return false ;        
    }    

    return true ;    
}

static basic_line_t *tokenize(const char *line, basic_err_t *err)
{
    basic_line_t *ret ;
    uint8_t token ;

    // Skip leading spaces
    line = skipSpaces(line) ;

    // If empty line, return EMPTY line
    if (*line == '\0') {
        *err = BASIC_ERR_NONE ;
        return NULL ;
    }

    ret = create_line() ;
    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    if (isdigit((uint8_t)(*line))) {
        // Starts with a line number
        line = parseInteger(line, &ret->lineno_);
        line = skipSpaces(line) ;
    }

    //
    // Now, look for a token to start the line
    //
    line = parse_keyword(line, &token, err) ;
    if (line == NULL) {
        basic_destroy_line(ret) ;
        return NULL ;
    }

    add_token(ret, token) ;

    if (token == BTOKEN_TO) {
        basic_destroy_line(ret);
        *err = BASIC_ERR_INVALID_TOKEN ;
        ret = NULL ;
    } else if ( token == BTOKEN_CLEAR || token == BTOKEN_CLS || token == BTOKEN_ELSE || token == BTOKEN_ENDIF ||
                token == BTOKEN_LIST || token == BTOKEN_RUN || token == BTOKEN_THEN || token == BTOKEN_FLIST)
    {
        line = skipSpaces(line) ;
        if (*line != '\0') {
            *err = BASIC_ERR_EXTRA_CHARS ;
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    } else if (token == BTOKEN_REM) {

    } else if (token == BTOKEN_LET) {
        if (!parse_let(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    } else if (token == BTOKEN_PRINT) {
        if (!parse_print(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    }
    else if (token == BTOKEN_FOR) {
        if (!parse_for(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    }
    else if (token == BTOKEN_GOSUB || token == BTOKEN_GOTO) {
        if (!parse_gotogosub(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    }
    else if (token == BTOKEN_IF) {
        if (!parse_if(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }
    }
    else if (token == BTOKEN_LOAD || token == BTOKEN_SAVE) {
        if (!parse_loadsave(ret, line, err)) {
            basic_destroy_line(ret) ;
            ret = NULL ;
        }        
    }

    return ret ;
}

static char msg[128] ;
bool basic_line_proc(const char *line, basic_out_fn_t outfn)
{
    bool rval = true ;
    basic_err_t code ;
    basic_line_t *ret ;

    ret = tokenize(line, &code) ;

    if (code != BASIC_ERR_NONE) {
        sprintf(msg, "Line: '%s'\n", line) ;
        (*outfn)(msg, strlen(msg)) ;

        sprintf(msg, "Error code %d\n", code) ;
        (*outfn)(msg, strlen(msg)) ;

        return false ;
    }

    if (ret->lineno_ == -1) {
        basic_exec_line(ret, outfn) ;
        basic_destroy_line(ret);
    }
    else {
        basic_store_line(ret) ;
    }

    return rval;
}

bool basic_proc_load(const char *filename, basic_err_t *err, basic_out_fn_t outfn)
{
    FIL fp ;
    FRESULT res ;
    UINT got ;
    int index = 0 ;

    // TODO - deal with parse error on load

    res = f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return false ;
    }

    while (true) {
        if (f_eof(&fp))
            break ;

        res = f_read(&fp, &linebuf[index], sizeof(linebuf) - index - 1, &got) ;
        if (res != FR_OK) {
            basic_clear(NULL, err, outfn);
            *err = BASIC_ERR_IO_ERROR ;
            return false ;
        }
        linebuf[index + got] = '\0' ;

        // Now process the buffer as a set of basic lines
        index = 0 ;
        while (index < got) {
            if (linebuf[index] == '\n') {
                linebuf[index] = '\0' ;
                printf("\nparsing: '%s'\n", linebuf);
                if (!basic_line_proc(linebuf, outfn)) {
                    basic_clear(NULL, err, outfn) ;
                    return false ;
                }
                int copy = got - index ;
                memcpy(linebuf, &linebuf[index + 1], got - index);
                got -= copy ;
                printf("remaining: '%s'\n", linebuf);
                index = 0 ;
            }
            else {
                index++ ;
            }
        }
    }

    //
    // Processing any remaining characters in the buffer
    //
    if (index != 0) {
        linebuf[index + 1] = '\0';
        basic_line_proc(linebuf, outfn);
    }

    f_close(&fp) ;

    return true;
}

void basic_prompt(basic_out_fn_t outfn)
{
    (*outfn)(prompt, strlen(prompt)) ;
}


