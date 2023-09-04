#pragma once
#include <stdint.h>

//
// Token types
//
typedef enum btokens {
    // Internal tokens that are important
    BTOKEN_COMMA,
    BTOKEN_SEMICOLON,
    BTOKEN_LET_ARRAY,
    BTOKEN_LET_SIMPLE,
    BTOKEN_PROMPT,
    BTOKEN_NO_PROMPT,

    // These below here are in the token table in basicproc.c
    BTOKEN_RUN,
    BTOKEN_CLS,     
    BTOKEN_LIST,
    BTOKEN_CLEAR,
    BTOKEN_LET,
    BTOKEN_IF,
    BTOKEN_THEN,
    BTOKEN_FOR,
    BTOKEN_NEXT,
    BTOKEN_TO,
    BTOKEN_GOTO,
    BTOKEN_GOSUB,
    BTOKEN_RETURN,
    BTOKEN_SAVE,
    BTOKEN_LOAD,
    BTOKEN_STEP,
    BTOKEN_PRINT,
    BTOKEN_FLIST,
    BTOKEN_DEL,
    BTOKEN_RENAME,
    BTOKEN_REM,
    BTOKEN_DIM,
    BTOKEN_DEF,
    BTOKEN_INPUT,
    BTOKEN_ON,
    BTOKEN_END,
    BTOKEN_STOP,
    BTOKEN_TRON,
    BTOKEN_TROFF
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
    struct exec_context *next_ ;         // Not always used, but these can be linked to create a stack
} exec_context_t ;

typedef struct for_stack_entry
{
    exec_context_t context_ ;
    uint32_t varidx_ ;
    uint32_t endidx_ ;
    uint32_t stepidx_ ;
    struct for_stack_entry *next_ ;
} for_stack_entry_t ;

typedef struct gosub_stack_entry
{
    exec_context_t context_ ;
    struct gosub_stack_entry *next_ ;
} gosub_stack_entry_t ;

typedef struct token_table
{
    btoken_t token_ ;
    const char *str_ ;
} token_table_t ;
