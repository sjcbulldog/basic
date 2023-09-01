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

static token_table_t tokens[] = 
{
    { BTOKEN_RUN, "RUN" },
    { BTOKEN_CLS, "CLS" },
    { BTOKEN_LIST, "LIST"},
    { BTOKEN_CLEAR, "CLEAR"},
    { BTOKEN_LET, "LET"},
    { BTOKEN_IF, "IF"},
    { BTOKEN_THEN, "THEN"},
    { BTOKEN_ELSE, "ELSE"},
    { BTOKEN_ENDIF, "ENDIF"},
    { BTOKEN_FOR, "FOR"},
    { BTOKEN_NEXT, "NEXT"},
    { BTOKEN_TO, "TO"},
    { BTOKEN_GOTO, "GOTO"},
    { BTOKEN_GOSUB, "GOSUB"},
    { BTOKEN_SAVE, "SAVE"},
    { BTOKEN_LOAD, "LOAD"},
    { BTOKEN_STEP, "STEP"},
    { BTOKEN_PRINT, "PRINT"},
    { BTOKEN_FLIST, "FLIST"},
    { BTOKEN_REM, "REM"},    
    { BTOKEN_DIM, "DIM"},
    { BTOKEN_DEF, "DEF"},
    { BTOKEN_INPUT, "INPUT"},
    { BTOKEN_ON, "ON"},

    { BTOKEN_LET_SIMPLE, "LET"},
    { BTOKEN_LET_ARRAY, "LET"},
} ;

static char linebuf[256] ;
static char other[256] ;
static char msg[128];

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
                break;

            case BTOKEN_LOAD:
            case BTOKEN_SAVE:
                basic_expr_destroy(getU32(line, 1));
                break;

            case BTOKEN_LET_SIMPLE:
                basic_expr_destroy(getU32(line, 5));
                break;

            case BTOKEN_LET_ARRAY:
            {
                int index = 5;
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

            case BTOKEN_REM:
            case BTOKEN_LIST:
            case BTOKEN_RUN: 
            case BTOKEN_CLEAR:
            case BTOKEN_FLIST:
            case BTOKEN_CLS:
                break;

            case BTOKEN_DEF:
                basic_userfn_destroy(getU32(line, 1));
                break;

            case BTOKEN_FOR:
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
    char keyword[BASIC_MAX_KEYWORD_LENGTH + 1] ;
    int stored = 0 ;

    *err = BASIC_ERR_NONE ;

    line = skipSpaces(line) ;

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
        if (_stricmp(tokens[i].str_, keyword) == 0) {
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

    if (!basic_var_get(keyword, varindex, err)) {
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

static const char *parse_loadsave(basic_line_t *bline, const char *line, basic_err_t *err)
{
    uint32_t exprindex ;

    line = basic_expr_parse(line, 0, NULL, &exprindex, err) ;
    if (line == NULL) {
        return NULL ;
    }

    if (!add_uint32(bline, exprindex)) {
        *err = BASIC_ERR_OUT_OF_MEMORY ;
        return NULL ;        
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
    else if (token == BTOKEN_CLEAR || token == BTOKEN_CLS || token == BTOKEN_ELSE || token == BTOKEN_ENDIF ||
        token == BTOKEN_LIST || token == BTOKEN_RUN || token == BTOKEN_THEN || token == BTOKEN_FLIST)
    {
        line = skipSpaces(line);
        if (*line != '\0' && *line != ':') {
            *err = BASIC_ERR_EXTRA_CHARS;
            line = NULL ;
        }
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
    else if (token == BTOKEN_DEF) {
        line = parse_def(ret, line, err);
    }
    else if (token == BTOKEN_FOR) {
        line = parse_for(ret, line, err) ;
    }
    else if (token == BTOKEN_NEXT) {
        line = parse_next(ret, line, err) ;
    }
    else if (token == BTOKEN_LOAD || token == BTOKEN_SAVE) {
        line = parse_loadsave(ret, line, err) ;
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

#ifdef _DUMP_TOKENS
static void dump_tokens(basic_line_t* line, const char* text, int lineno)
{
    printf("%d:'", lineno);
    while (*text && *text != '\n' && *text != ':') {
        putchar(*text++);
    }
    printf("'\n");
    printf("  %d:", line->count_);
    for (int i = 0; i < line->count_; i++) {
        printf(" %02x ", line->tokens_[i], line->tokens_[i]);
    }
    printf("\n");
}
#endif

static basic_line_t *tokenize(const char *line, basic_err_t *err)
{
    basic_line_t *ret = NULL, *child, *last = NULL ;
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
#ifdef _DUMP_TOKENS
        const char* save = line;
#endif
        if (first) {
            line = tokenize_one(line, &ret, err) ;
            if (line == NULL)
                return NULL ;

            ret->lineno_ = lineno ;
            first = false ;

#ifdef _DUMP_TOKENS
            dump_tokens(ret, save, lineno);
#endif
        }
        else {
            line = tokenize_one(line, &child, err) ;
            if (line == NULL)
                return NULL ;

#ifdef _DUMP_TOKENS
            dump_tokens(child, save, lineno);
#endif

            if (last == NULL) {
                ret->children_ = child ;
            }
            else {
                last->next_ = child ;
            }

            last = child ;
        }

        line = skipSpaces(line) ;
        if (*line == '\0')
            break ;

        if (*line != ':') {
            *err = BASIC_ERR_EXTRA_CHARS ;
            return NULL ;
        }

        line++ ;
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


