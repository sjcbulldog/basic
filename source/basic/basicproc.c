#include "basicproc.h"
#include "basicerr.h"
#include "basicline.h"
#include "basicexec.h"
#include "basicexpr.h"
#include "basiccfg.h"
#include "basicstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#ifndef DESKTOP
#include <ff.h>
#define _stricmp strcasecmp
#define _strdup strdup
#endif

static const char *prompt = "Basic06> ";

token_table_t tokens[] = 
{
    { BTOKEN_RUN, "RUN" },
    { BTOKEN_CLS, "CLS" },
    { BTOKEN_LIST, "LIST"},
    { BTOKEN_CLEAR, "CLEAR"},
    { BTOKEN_LET, "LET"},
    { BTOKEN_IF, "IF"},
    { BTOKEN_THEN, "THEN"},
    { BTOKEN_FOR, "FOR"},
    { BTOKEN_NEXT, "NEXT"},
    { BTOKEN_TO, "TO"},
    { BTOKEN_GOTO, "GOTO"},
    { BTOKEN_GOSUB, "GOSUB"},
    { BTOKEN_RETURN, "RETURN"},
    { BTOKEN_SAVE, "SAVE"},
    { BTOKEN_LOAD, "LOAD"},
    { BTOKEN_STEP, "STEP"},
    { BTOKEN_PRINT, "PRINT"},
    { BTOKEN_FLIST, "FLIST"},
    { BTOKEN_DEL, "DEL"},
    { BTOKEN_RENAME, "RENAME"},        
    { BTOKEN_REM, "REM"},    
    { BTOKEN_DIM, "DIM"},
    { BTOKEN_DEF, "DEF"},
    { BTOKEN_INPUT, "INPUT"},
    { BTOKEN_ON, "ON"},
    { BTOKEN_END, "END"},
    { BTOKEN_STOP, "STOP"},
    { BTOKEN_TRON, "TRON"},
    { BTOKEN_TROFF, "TROFF"},

    { BTOKEN_LET_SIMPLE, "LET"},
    { BTOKEN_LET_ARRAY, "LET"},
} ;

static char linebuf[256] ;
static char other[256] ;
static char msg[128];

bool basic_is_keyword(const char *line)
{
    for(int i = 0 ; i < sizeof(tokens)/sizeof(tokens[0]) ; i++) {
        int len = strlen(tokens[i].str_) ;
        if (strncasecmp(line, tokens[i].str_, len) == 0) {
            return true ;
        }
    }
    return false ;
}

const char *basic_token_to_str(uint8_t token)
{
    for(int i = 0 ; i < sizeof(tokens) / sizeof(tokens[0]) ; i++) {
        if (tokens[i].token_ == token) {
            return tokens[i].str_;
        }
    }

    return "!ERROR!" ;
}

static basic_line_t *create_line()
{
    basic_line_t *ret = (basic_line_t *)malloc(sizeof(basic_line_t)) ;
    if (ret == NULL)
        return NULL ;

    ret->lineno_ = -1 ;
    ret->count_ = 0 ;
    ret->next_ = NULL ;
    ret->children_ = NULL ;
    ret->tokens_ = NULL ;
    ret->extra_ = NULL ;

    return ret ;
}

void basic_destroy_line(basic_line_t *line)
{
    if (line->tokens_ != NULL) {
        switch (line->tokens_[0])
        {
            case BTOKEN_DIM:
            {
                uint32_t index = 1 ;

                while (index < line->count_) {
                    basic_str_destroy(getU32(line, index)) ;
                    index += 4 ;

                    uint32_t dimcnt = getU32(line, index) ;
                    index += 4 ;
                    index += dimcnt * 4 ;
                }
            }
            break;

            case BTOKEN_LOAD:
            case BTOKEN_SAVE:
                basic_expr_destroy(getU32(line, 1));
                break;

            case BTOKEN_LET_SIMPLE:
                basic_str_destroy(getU32(line, 1)) ;
                basic_expr_destroy(getU32(line, 5));
                break;

            case BTOKEN_LET_ARRAY:
            {
                int index = 1;

                basic_str_destroy(getU32(line, index)) ;
                index += 4 ;

                uint32_t dimcnt = getU32(line, index);
                index += 4;

                // Note, the +1 covers the assigned expression
                for (uint32_t i = 0; i < dimcnt + 1; i++) {
                    uint32_t expridx = getU32(line, index);
                    index += 4;
                    basic_expr_destroy(expridx);
                }
            }
            break;

            case BTOKEN_PRINT:
            {
                int index = 1;
                while (index < line->count_) {
                    uint32_t expridx = getU32(line, index);
                    index += 4;
                    basic_expr_destroy(expridx);

                    if (index == line->count_)
                        break;

                    assert(line->tokens_[index] == BTOKEN_COMMA || line->tokens_[index] == BTOKEN_SEMICOLON);
                    index++;
                }
            }
            break; 

            case BTOKEN_INPUT:
            {
                uint32_t strh ;
                int index = 2 ;

                while (index < line->count_) {
                    strh = getU32(line, index) ;
                    index += 4 ;

                    basic_str_destroy(strh) ;
                }
            }
            break ;

            case BTOKEN_ON:
                basic_expr_destroy(getU32(line, 2));
                break ;

            case BTOKEN_REM:
            case BTOKEN_LIST:
            case BTOKEN_RUN: 
            case BTOKEN_CLEAR:
            case BTOKEN_FLIST:
            case BTOKEN_CLS:
            case BTOKEN_GOTO:
            case BTOKEN_GOSUB:
            case BTOKEN_RETURN:
            case BTOKEN_END:
            case BTOKEN_STOP:
            case BTOKEN_TRON:
            case BTOKEN_TROFF:
                break;

            case BTOKEN_DEL:
                basic_expr_destroy(getU32(line, 1));
                break;    

            case BTOKEN_RENAME:
                basic_expr_destroy(getU32(line, 1));
                break;                         

            case BTOKEN_DEF:
                basic_userfn_destroy(getU32(line, 1));
                break;

            case BTOKEN_IF:
                basic_expr_destroy(getU32(line, 1));
                break ;

            case BTOKEN_FOR:
                basic_str_destroy(getU32(line, 1)) ;
                basic_expr_destroy(getU32(line, 5)) ;
                basic_expr_destroy(getU32(line, 9)) ;
                basic_expr_destroy(getU32(line, 13)) ;
                break ;

            case BTOKEN_NEXT:
                break;

            default:
                assert(false);
                break;
        }

        free(line->tokens_);
    }

    if (line->extra_) {
        free(line->extra_);
    }

    basic_line_t *child = line->children_ ;
    while (child) {
        basic_line_t *next = child->next_ ;
        basic_destroy_line(child) ;
        child = next ;
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
    *err = BASIC_ERR_NONE ;
    line = skipSpaces(line) ;

    for(int i = 0 ; i < sizeof(tokens)/sizeof(tokens[0]) ; i++) {
        int len = strlen(tokens[i].str_) ;
        if (strncasecmp(line, tokens[i].str_, len) == 0) {
            *token = tokens[i].token_ ;
            return line + len ;
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

    if (!isalpha((uint8_t)*line)) {
        *err = BASIC_ERR_INVALID_VARNAME ;
        return NULL ;
    }

    while (isalpha((uint8_t)*line) || isdigit((uint8_t)*line))
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

    *varindex = basic_str_create_str(keyword) ;
    if (*varindex == BASIC_STR_INVALID) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    return line ;
}

static const char* parse_def(basic_line_t* bline, const char* line, basic_err_t* err)
{
    char keyword[BASIC_MAX_VARIABLE_LENGTH + 1];
    int stored = 0;
    uint32_t exprindex;
    char* argnames[BASIC_MAX_DEFFN_ARGS];

    line = skipSpaces(line);
    while (isalpha((uint8_t)*line))
    {
        keyword[stored++] = *line++;
        if (stored == BASIC_MAX_VARIABLE_LENGTH + 1) {
            *err = BASIC_ERR_VARIABLE_TOO_LONG;
            return NULL;
        }
    }
    keyword[stored] = '\0';

    char* fnname = _strdup(keyword);
    if (fnname == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    if (*line != '(') {
        *err = BASIC_ERR_EXPECTED_OPENPAREN;
        return NULL;
    }
    line++;

    int index = 0;
    while (true) {
        line = skipSpaces(line);
        stored = 0;
        while (isalpha((int)*line)) {
            keyword[stored++] = *line++;
            if (stored == BASIC_MAX_VARIABLE_LENGTH + 1) {
                *err = BASIC_ERR_VARIABLE_TOO_LONG;
                return NULL;
            }
        }
        keyword[stored] = '\0';
        argnames[index] = _strdup(keyword);
        if (argnames[index] == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        index++;

        line = skipSpaces(line);
        if (*line == ')') {
            break;
        }
        else if (*line != ',') {
            *err = BASIC_ERR_EXPECTED_CLOSEPAREN;
            return NULL;
        }
    }

    line = skipSpaces(line + 1);

    if (*line != '=') {
        *err = BASIC_ERR_EXPECTED_EQUAL;
        return NULL;
    }

    line = skipSpaces(line + 1);

    line = basic_expr_parse(line, index, argnames, &exprindex, err);
    if (line == NULL) {
        return NULL;
    }

    //
    // Bind the expression to the function name
    //
    uint32_t fnindex ;
    if (!basic_userfn_bind(fnname, index, argnames, exprindex, &fnindex, err)) {
        line = NULL;
    }

    if (!add_uint32(bline, fnindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    return line;
}

static const char *parse_print(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    line = skipSpaces(line) ;
    if (*line == '\0' || *line == ':') {
        *err = BASIC_ERR_NONE ;
        return line ;
    }

    while (1) {
        line = skipSpaces(line) ;
        if (*line == '\0' || *line == ':')
            break ;
            
        line = basic_expr_parse(line, 0, NULL, &exprindex, err) ;
        if (line == NULL) {
            return NULL ;
        }

        if (!add_uint32(bline, exprindex)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }      

        line = skipSpaces(line) ;
        if (*line == '\0' || *line == ':') {
            break ;
        }
        else if (*line == ',') {
            line++ ;
            if (!add_token(bline, BTOKEN_COMMA)) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return NULL ;        
            }
        }
        else if (*line == ';') {
            line++ ;
            if (!add_token(bline, BTOKEN_SEMICOLON)) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return NULL ;        
            }
        }
        else {
            *err = BASIC_ERR_EXTRA_CHARS ;
            return NULL ;
        }
    }

    return line ;
}

static const char *parse_input(basic_line_t *bline, const char *line, basic_err_t *err)
{
    int index = 1 ;
    int cntstr = 0 ;
    uint32_t v ;

    line = skipSpaces(line) ;
    if (*line == '"') {

        // This input has a prompt
        line = basic_expr_parse_str(line, &v, err) ;
        if (line == NULL)
            return NULL ;

        if (!add_token(bline, BTOKEN_PROMPT)){
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }            

        if (!add_uint32(bline, v)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
        index += 4 ;

        line = skipSpaces(line) ;

        if (*line != ';') {
            *err = BASIC_ERR_EXPECTED_SEMICOLON ;
            return NULL ;
        }
        line++ ;
    }
    else {
        if (!add_token(bline, BTOKEN_NO_PROMPT)){
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
    }

    while (true) {
        line = parse_varname(line, &v, err) ;
        if (line == NULL)
            return NULL ;

        const char *varname = basic_str_value(v) ;
        if (varname[strlen(varname) - 1] == '$')
            cntstr++ ;

        if (!add_uint32(bline, v)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }   

        line = skipSpaces(line) ;

        if (*line == '\0' || *line == ':') {
            break ;
        }

        if (*line != ',') {
            *err = BASIC_ERR_EXPECTED_COMMA ;
            return NULL ;
        }
        line++ ;
    }

    if (cntstr > 1) {
        *err = BASIC_ERR_TOO_MANY_STRING_VARS ;
        return NULL ;
    }

    return line ;     
}

static const char *parse_on(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;
    uint8_t token ;

    line = basic_expr_parse(line, 0, NULL, &exprindex, err) ;
    if (line == NULL)
        return NULL ;

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    line = parse_keyword(line, &token, err) ;
    if (line == NULL)
        return NULL ;

    if (token != BTOKEN_GOTO && token != BTOKEN_GOSUB) {
        *err = BASIC_ERR_EXPECTED_GOTO_GOSUB ;
        return NULL ;
    }

    if (!add_token(bline, token)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    int lineno ;
    while (true) {
        line = basic_expr_parse_int(line, &lineno, err) ;
        if (line == NULL)
            return NULL ;

        if (!add_uint32(bline, lineno)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        line = skipSpaces(line) ;
        if (*line == '\0' || *line == ':')
            break ;
        else if (*line == ',') {
            line = skipSpaces(line + 1) ;
        }
        else {
            *err = BASIC_ERR_EXPECTED_COMMA ;
            return NULL ;
        }
    }

    return line ;
}

static const char *parse_let(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t varindex, exprindex ;
    int dimcnt = 0 ;
    uint32_t dims[BASIC_MAX_DIMS] ;

    line = parse_varname(line, &varindex, err) ;
    if (line == NULL) {
        return NULL ;
    }

    line = skipSpaces(line) ;
    if (*line == '(') {
        //
        // This is an array
        //
        line = basic_expr_parse_dims_expr(line, &dimcnt, dims, err);
        if (line == NULL)
            return NULL;

        line = skipSpaces(line);
    }

    if (*line != '=') {
        *err = BASIC_ERR_EXPECTED_EQUAL ;
        return NULL ;
    }

    line++ ;

    line = basic_expr_parse(line, 0, NULL, &exprindex, err) ;
    if (line == NULL) {
        return NULL ;
    }

    line = skipSpaces(line) ;
    if (*line != '\0' && *line != ':') {
        *err = BASIC_ERR_EXTRA_CHARS ;
        return NULL ;
    }

    bline->count_ = 0;
    if (dimcnt > 0) {
        if (!add_token(bline, BTOKEN_LET_ARRAY)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        if (!add_uint32(bline, varindex)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        if (!add_uint32(bline, dimcnt)) {
            *err = BASIC_ERR_OUT_OF_MEMORY;
            return NULL;
        }

        for(int i = 0 ; i < dimcnt ; i++) {
            if (!add_uint32(bline, dims[i])) {
                *err = BASIC_ERR_OUT_OF_MEMORY ;
                return NULL ;
            }            
        }
    }
    else {
        if (!add_token(bline, BTOKEN_LET_SIMPLE)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }

        if (!add_uint32(bline, varindex)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;        
    }

    return line ;
}

static const char *parse_strings(int cnt, basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    while (cnt-- > 0) {
        line = basic_expr_parse(line, 0, NULL, &exprindex, err) ;
        if (line == NULL) {
            return NULL ;
        }

        if (!add_uint32(bline, exprindex)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;        
        }    
    }

    return line ;    
}

static const char *parse_dim(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t index ;
    uint32_t dimcnt = 0 ;
    uint32_t dims[BASIC_MAX_DIMS] ;

    while (true) {
        line = parse_varname(line, &index, err) ;
        if (line == NULL) {
            return NULL ;
        }

        line = basic_expr_parse_dims_const(line, &dimcnt, dims, err);
        if (line == NULL)
            return NULL;

        if (!add_uint32(bline, index)) {
            *err = BASIC_ERR_OUT_OF_MEMORY;
            return NULL;
        }

        if (!add_uint32(bline, dimcnt)) {
            *err = BASIC_ERR_OUT_OF_MEMORY;
            return NULL;
        }

        for (uint32_t i = 0; i < dimcnt; i++) {
            if (!add_uint32(bline, dims[i])) {
                *err = BASIC_ERR_OUT_OF_MEMORY;
                return NULL;
            }
        }

        line = skipSpaces(line) ;
        if (*line == ',') {
            // More variables
            line++ ;
        }
        else if (*line == '\0' || *line == ':') {
            break ;
        }
        else {
            *err = BASIC_ERR_INVALID_CHAR ;
            return NULL ;
        }
    }

    return line ;
}

static const char *parse_next(basic_line_t* bline, const char* line, basic_err_t* err)
{
    uint32_t varidx ;

    line = skipSpaces(line);
    if (*line != '\0' && *line != ':') {
        line = parse_varname(line, &varidx, err) ;
        if (line == NULL)
            return NULL ;

        if (!add_uint32(bline, varidx)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }
    }

    return line ;
}

static const char* parse_for(basic_line_t* bline, const char* line, basic_err_t* err)
{
    uint32_t varidx, initexpr, endexpr, stepexpr ;
    uint8_t token ;

    line = skipSpaces(line);
    line = parse_varname(line, &varidx, err) ;
    if (line == NULL)
        return NULL ;

    line = skipSpaces(line) ;
    if (*line != '=') {
        *err = BASIC_ERR_EXPECTED_EQUAL ;
        return NULL ;
    }
    line = skipSpaces(line + 1);

    line = basic_expr_parse(line, 0, NULL, &initexpr, err) ;
    if (line == NULL)
        return NULL ;

    line = parse_keyword(line, &token, err) ;
    if (line == NULL || token != BTOKEN_TO) {
        *err = BASIC_ERR_EXPECTED_TO ;
        return NULL ;
    }

    line = basic_expr_parse(line, 0, NULL, &endexpr, err) ;
    if (line == NULL)
        return NULL ;

    if (!add_uint32(bline, varidx)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    if (!add_uint32(bline, initexpr)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }    

    if (!add_uint32(bline, endexpr)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }      

    line = skipSpaces(line) ;

    if (*line != '\0' && *line != ':') {
        line = parse_keyword(line, &token, err) ;
        if (line == NULL || token != BTOKEN_STEP) {
            *err = BASIC_ERR_EXPECTED_STEP ;
            return NULL ;
        }

        line = basic_expr_parse(line, 0, NULL, &stepexpr, err) ;
        if (line == NULL)
            return NULL ;

        if (!add_uint32(bline, stepexpr)) {
            *err = BASIC_ERR_OUT_OF_MEMORY ;
            return NULL ;
        }             
    }

    return line ;
}

static const char* parse_list(basic_line_t* bline, const char* line, basic_err_t* err)
{
    int start = 0 ;
    int end = 0x7fffffff ;

    line = skipSpaces(line) ;
    if (*line != '\0' && *line != ':') {
        //
        // Starting line
        //
        if (*line != '-') {
            line = basic_expr_parse_int(line, &start, err) ;
            if (line == NULL)
                return NULL ;

            line = skipSpaces(line) ;
        }

        //
        // We have parsed the starting line number, should be either at
        // a dash or at the end of the statement
        //
        if (*line == '\0' || *line == ':') {
            //
            // Just a single line number, set start and end to the same line
            end = start ;
        }
        else {
            //
            // Ok there should be a dash next
            //
            if (*line != '-') {
                *err = BASIC_ERR_EXPECTED_DASH ;
                return NULL ;
            }

            line = skipSpaces(line + 1) ;

            if (*line != '\0' && *line != ':') {
                //
                // This is the case of START - END
                //
                line = basic_expr_parse_int(line, &start, err) ;
                if (line == NULL)
                    return NULL ;
            } else {
                //
                // This is the case of START -
                //
            }
        }
    }

    if (!add_uint32(bline, (uint32_t)start)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    } 

    if (!add_uint32(bline, (uint32_t)end)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }     

    return line ;
}

static const char *parse_goto_gosub(basic_line_t* bline, const char* line, basic_err_t* err)
{
    int value ;

    line = skipSpaces(line) ;
    line = basic_expr_parse_int(line, &value, err) ;
    if (line == NULL)
        return NULL ;

    if (!add_uint32(bline, (uint32_t)value)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    return line ;
}

static const char *parse_if(basic_line_t* bline, const char* line, basic_err_t* err)
{
    uint32_t expridx ;
    uint8_t token ;

    line = skipSpaces(line) ;
    line = basic_expr_parse(line, 0, NULL, &expridx, err) ;
    if (line == NULL)
        return NULL ;

    if (!add_uint32(bline, expridx)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;
    }

    line = skipSpaces(line) ;
    line = parse_keyword(line, &token, err) ;
    if (line == NULL || token != BTOKEN_THEN) {
        *err = BASIC_ERR_EXPECTED_THEN ;
        return NULL ;
    }

    return line ;
}

static const char* tokenize_one(const char* line, basic_line_t** bline, basic_err_t* err)
{
    basic_line_t* ret;
    uint8_t token;

    line = skipSpaces(line);

    ret = create_line();
    if (ret == NULL) {
        *err = BASIC_ERR_OUT_OF_MEMORY;
        *bline = NULL;
        return NULL;
    }
    *bline = ret;

    //
    // Now, look for a token to start the line
    //
    const char* save = line;
    line = parse_keyword(line, &token, err);
    if (line == NULL) {
        line = save;
        int errsave = *err;

        //
        // Ok, it is not a basic token, but it might be a variables
        //
        line = parse_let(ret, line, err);
        if (line == NULL) {
            *err = errsave;
            basic_destroy_line(ret);
            *bline = NULL;
            return NULL;
        }
        else {
            return line;
        }
    }

    add_token(ret, token);

    if (token == BTOKEN_TO || token == BTOKEN_STEP) {
        basic_destroy_line(ret);
        *err = BASIC_ERR_INVALID_TOKEN;
        ret = NULL;
    }
    else if (token == BTOKEN_CLEAR || token == BTOKEN_CLS || token == BTOKEN_RUN || token == BTOKEN_END ||
             token == BTOKEN_THEN || token == BTOKEN_FLIST || token == BTOKEN_RETURN || 
             token == BTOKEN_STOP || token == BTOKEN_TRON || token == BTOKEN_TROFF)
    {
        line = skipSpaces(line);
        if (*line != '\0' && *line != ':') {
            *err = BASIC_ERR_EXTRA_CHARS;
            line = NULL ;
        }
    }
    else if (token == BTOKEN_LIST) {
        line = parse_list(ret, line, err) ;
    }
    else if (token == BTOKEN_REM) {
        ret->extra_ = _strdup(line);
        if (ret->extra_ == NULL) {
            *err = BASIC_ERR_OUT_OF_MEMORY;
            basic_destroy_line(ret);
            ret = NULL;
        }
        int len = (int)strlen(ret->extra_);
        if (ret->extra_[len - 1] == '\n') {
            ret->extra_[len - 1] = '\0';
        }
        while (*line != '\0')
            line++;
    }
    else if (token == BTOKEN_LET) {
        line = parse_let(ret, line, err);
    }
    else if (token == BTOKEN_PRINT) {
        line = parse_print(ret, line, err);
    }
    else if (token == BTOKEN_INPUT) {
        line = parse_input(ret, line, err) ;
    }
    else if (token == BTOKEN_ON) {
        line = parse_on(ret, line, err) ;        
    }
    else if (token == BTOKEN_DEF) {
        line = parse_def(ret, line, err);
    }
    else if (token == BTOKEN_FOR) {
        line = parse_for(ret, line, err) ;
    }
    else if (token == BTOKEN_GOTO || token == BTOKEN_GOSUB) {
        line = parse_goto_gosub(ret, line, err) ;
    }
    else if (token == BTOKEN_IF) {
        line = parse_if(ret, line, err);
    }
    else if (token == BTOKEN_NEXT) {
        line = parse_next(ret, line, err) ;
    }
    else if (token == BTOKEN_LOAD || token == BTOKEN_SAVE || token == BTOKEN_DEL) {
        line = parse_strings(1, ret, line, err) ;
    }
    else if (token == BTOKEN_RENAME) {
        line = parse_strings(2, ret, line, err) ;        
    }
    else if (token == BTOKEN_DIM) {
        line = parse_dim(ret, line, err) ;
    }

    if (line == NULL) {
        basic_destroy_line(ret) ;
        *bline = NULL ;
        line = NULL ;
    }
    return line ;
}

static basic_line_t *tokenize(const char *line, basic_err_t *err)
{
    basic_line_t *ret = NULL, *child = NULL, *last = NULL, *parsed = NULL ;
    bool first = true ;
    int lineno = -1 ;

    // Skip leading spaces
    line = skipSpaces(line) ;

    // If empty line, return EMPTY line
    if (*line == '\0') {
        *err = BASIC_ERR_NONE ;
        return NULL ;
    }

    if (isdigit((uint8_t)(*line))) {
        // Starts with a line number
        line = basic_expr_parse_int(line, &lineno, err);
        line = skipSpaces(line) ;
    }

    while (true) {
        if (first) {
            line = tokenize_one(line, &ret, err) ;
            if (line == NULL)
                return NULL ;

            ret->lineno_ = lineno ;
            first = false ;
            parsed = ret ;
        }
        else {
            line = tokenize_one(line, &child, err) ;
            if (line == NULL)
                return NULL ;

            if (last == NULL) {
                ret->children_ = child ;
            }
            else {
                last->next_ = child ;
            }

            last = child ;
            parsed = child ;
        }

        line = skipSpaces(line) ;
        if (*line == '\0')
            break ;

        //
        // The form if an IF statement is IF expr THEN stmt1:stmt2:stmt3 ...
        // We want stmt1 to be the first child
        //
        if (parsed->tokens_[0] != BTOKEN_IF) {
            if (*line != ':') {
                *err = BASIC_ERR_EXTRA_CHARS ;
                return NULL ;
            }

            line++ ;
        }
    }

    return ret ;
}

bool basic_line_proc(const char *line, basic_out_fn_t outfn)
{
    bool rval = true ;
    basic_err_t code ;
    basic_line_t *ret ;

    ret = tokenize(line, &code) ;

    if (code != BASIC_ERR_NONE) {
        int len = (int)strlen(line) ;

        (*outfn)("Line: '", 7) ;
        (*outfn)(line, len - 1);
        (*outfn)("'\n", 2);

        sprintf(msg, "Error code %d\n", code) ;
        (*outfn)(msg, (int)strlen(msg)) ;

        return false ;
    }

    if (ret == NULL)
        return true ;

    if (ret->lineno_ == -1) {
        exec_context_t ctx ;

        ctx.line_ = ret ;
        ctx.child_ = NULL ;

        basic_exec_line(&ctx, outfn) ;
        basic_destroy_line(ret);
    }
    else {
        basic_store_line(ret) ;
    }

    return rval;
}

#ifndef DESKTOP
static UINT got = 0 ;               // The number of chars read into line buf
static UINT oindex = 0 ;            // Index into the line buffer (other)
static UINT rdindex = 0 ;            // Index into the read buffer (linebuf)

static int read_line(FIL *fp)
{
    FRESULT res ;

    oindex = 0 ;

    while (true) {
        if (got == 0) {
            if (f_eof(fp))
                return 0 ;
                
            //
            // We need more characters, but buffer is empty
            //
            res = f_read(fp, linebuf, sizeof(linebuf), &got) ;
            if (res != FR_OK) {
                return -1 ;
            }
            rdindex = 0 ;
        }

        //
        // We now have 'got' characters starting at 'rindex'
        //
        while (got > 0) {
            other[oindex++] = linebuf[rdindex++] ;
            got-- ;

            if (other[oindex - 1] == '\n') {
                other[oindex] = '\0';
                return 1 ;
            }
        }
    }
}

bool basic_proc_load(const char *filename, basic_err_t *err, basic_out_fn_t outfn)
{
    FIL fp ;
    FRESULT res ;

    res = f_open(&fp, filename, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK) {
        *err = BASIC_ERR_COUNT_NOT_OPEN ;
        return false ;
    }

    while (true) {
        int st = read_line(&fp) ;
        if (st == -1) {
            basic_clear(NULL, err, outfn);
            *err = BASIC_ERR_IO_ERROR ;
            return false ;            
        }
        else if (st == 0) {
            break ;
        }
        else {
            if (!basic_line_proc(other, outfn)) {
                basic_clear(NULL, err, outfn) ;
                return false ;
            }
        }
    }

    f_close(&fp) ;

    return true;
}
#endif

void basic_prompt(basic_out_fn_t outfn)
{
    (*outfn)(prompt, (int)strlen(prompt)) ;
}


