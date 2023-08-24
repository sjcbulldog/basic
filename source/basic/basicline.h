#pragma once
#include <stdint.h>

#define BASIC_MAX_KEYWORD_LENGTH            (16)
#define BASIC_MAX_VARIABLE_LENGTH           (16)

//
// Token types
//
typedef enum btokens {
    // Char tokens that are important
    BTOKEN_COMMA = 1,
    BTOKEN_SEMICOLON = 2,

    // Pseudo tokens 
    BTOKEN_VAR = 80,
	BTOKEN_EXPR = 81,

    // These below here are in the token table in basicproc.c
    BTOKEN_RUN = 100,
    BTOKEN_CLS = 101,
    BTOKEN_LIST = 102,
    BTOKEN_CLEAR = 103,
    BTOKEN_LET = 104,
    BTOKEN_IF = 105,
    BTOKEN_THEN = 106,
    BTOKEN_ELSE = 107,
    BTOKEN_ENDIF = 108,
    BTOKEN_FOR = 109,
    BTOKEN_TO = 110,
    BTOKEN_GOTO = 111,
    BTOKEN_GOSUB = 112,
    BTOKEN_SAVE = 113,
    BTOKEN_LOAD = 114,
    BTOKEN_STEP = 115,
    BTOKEN_PRINT = 116,
    BTOKEN_FLIST = 117,
    BTOKEN_REM = 120,
    BTOKEN_DIM = 121,
    BTOKEN_DEF = 122,
    BTOKEN_INPUT = 123,
    BTOKEN_ON = 124

} btoken_t ;

typedef struct basic_line
{
    int lineno_ ;
    int count_ ;
    uint8_t *tokens_ ;
    char *extra_ ;
    struct basic_line *children_ ;
    struct basic_line *next_ ;
} basic_line_t ;

typedef struct exec_context
{
    basic_line_t *line_ ;
    basic_line_t *child_ ;
} exec_context_t ;

typedef struct token_table
{
    btoken_t token_ ;
    const char *str_ ;
} token_table_t ;
