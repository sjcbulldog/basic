#pragma once
#include <stdint.h>

#define BASIC_MAX_KEYWORD_LENGTH            (16)
#define BASIC_MAX_VARIABLE_LENGTH           (16)

//
// Token types
//
typedef enum btokens {
    BTOKEN_COMMA = 1,
    BTOKEN_RUN = 2,
    BTOKEN_CLS = 3,
    BTOKEN_LIST = 4,
    BTOKEN_CLEAR = 5,
    BTOKEN_LET = 6,
    BTOKEN_IF = 7,
    BTOKEN_THEN = 8,
    BTOKEN_ELSE = 9,
    BTOKEN_ENDIF = 10,
    BTOKEN_FOR = 11,
    BTOKEN_TO = 12,
    BTOKEN_GOTO = 13,
    BTOKEN_GOSUB = 14,
    BTOKEN_SAVE = 15,
    BTOKEN_LOAD = 16,
    BTOKEN_STEP = 17,
    BTOKEN_PRINT = 18,
    BTOKEN_FLIST = 19,
	BTOKEN_EXPR = 20,
	BTOKEN_VAR = 21

} btoken_t ;

typedef struct basic_line
{
    int lineno_ ;
    int count_ ;
    uint8_t *tokens_ ;
    struct basic_line *next_ ;
} basic_line_t ;

typedef struct token_table
{
    btoken_t token_ ;
    const char *str_ ;
} token_table_t ;
