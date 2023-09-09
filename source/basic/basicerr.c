#ifdef DESKTOP
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "basicerr.h"
#include <stdint.h>

static const char *errMessages[] = {
    "NO ERROR",
    "OUT OF MEMORY",
    "NETWORK ERROR",
    "KEYWORD TOO LONG",
    "UNKNOWN KEYWORD",
    "INVALID TOKEN",
    "EXTRA CHARS",
    "EXPECTED EQUAL",
    "EXPECTED TO",
    "EXPECTED STEP",
    "EXPECTED THEN", // 10
    "EXPECTED SEMICOLON",
    "EXPECTED VAR",
    "EXPECTED OPEN PAREN",
    "EXPECTED CLOSE PAREN",
    "EXPECTED QUOTE",
    "EXPECTED IDENTIFIER",
    "EXPECTED COMMA",
    "EXPECTED GOTO GOSUB",
    "VARIABLE TOO LONG",
    "NUMBER TOO LONG", // 20
    "STRING TOO LONG",
    "UNTERMINATED STRING",
    "NO SUCH VARIABLE",
    "TYPE MISMATCH",
    "SDCARD ERROR",
    "COUNT NOT OPEN",
    "IO ERROR",
    "INVALID OPERATOR",
    "TOO COMPLEX",
    "NOT ALLOWED", // 30
    "BAD INTEGER VALUE",
    "BAD NUMBER VALUE",
    "INVALID DIMENSION",
    "INVALID CHAR",
    "TOO MANY DIMS",
    "DIVIDE ZERO",
    "NOT ARRAY",
    "DIM MISMATCH",
    "BAD ARG COUNT",
    "TRNG FAILED", // 40
    "UNBOUND LOCAL VAR",
    "ARG COUNT MISMATCH",
    "INVALID ARG VALUE",
    "UNMATCHED NEXT",
    "EXPECTED DASH",
    "NO GOSUB",
    "INVALID LINE NUMBER",
    "INVALID VARNAME",
    "TOO MANY STRING VARS",
    "NO DATA/DATA EXHAUSTED"
};

const char *basic_err_to_string(basic_err_t err)
{
    uint32_t code = (uint32_t)err;
    const char *msg = "UNKNOWN ERROR MESSAGE";

    if (code < sizeof(errMessages) / sizeof(errMessages[0]))
    {
        msg = errMessages[code];
    }

    return msg;
}