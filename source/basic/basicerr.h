#pragma once


// 
// Error codes
//
typedef enum basic_err {
    BASIC_ERR_NONE = 0,
    BASIC_ERR_OUT_OF_MEMORY = 1,
    BASIC_ERR_NETWORK_ERROR = 2,
    BASIC_ERR_KEYWORD_TOO_LONG = 3,
    BASIC_ERR_UNKNOWN_KEYWORD = 4,
    BASIC_ERR_INVALID_TOKEN = 5,
    BASIC_ERR_EXTRA_CHARS = 6,
    BASIC_ERR_EXPECTED_EQUAL = 7,
    BASIC_ERR_EXPECTED_TO = 8,
    BASIC_ERR_EXPECTED_STEP = 9,
    BASIC_ERR_EXPECTED_THEN = 0,
    BASIC_ERR_VARIABLE_TOO_LONG = 1,
    BASIC_ERR_NUMBER_TOO_LONG = 2,
    BASIC_ERR_STRING_TOO_LONG = 3,
    BASIC_ERR_UNTERMINATED_STRING = 4,
    BASIC_ERR_NO_SUCH_VARIABLE = 5,
    BASIC_ERR_TYPE_MISMATCH = 6,
    BASIC_ERR_SDCARD_ERROR = 7,
    BASIC_ERR_COUNT_NOT_OPEN = 8,
    BASIC_ERR_IO_ERROR = 19,
    BASIC_ERR_INVALID_OPERATOR = 20,
    BASIC_ERR_TOO_COMPLEX = 21,
    BASIC_ERR_NOT_ALLOWED = 22,
    BASIC_ERR_EXPECTED_VAR = 23,
    BASIC_ERR_EXPECTED_OPENPAREN = 24,
    BASIC_ERR_EXPECTED_CLOSEPAREN = 25,
    BASIC_ERR_BAD_INTEGER_VALUE = 26,
    BASIC_ERR_INVALID_DIMENSION = 27, 
    BASIC_ERR_INVALID_CHAR = 28,
    BASIC_ERR_TOO_MANY_DIMS = 29,
    BASIC_ERR_DIVIDE_ZERO = 30,
    BASIC_ERR_NOT_ARRAY = 31,
    BASIC_ERR_DIM_MISMATCH = 32,
    BASIC_ERR_BAD_ARG_COUNT = 33,
    BASIC_ERR_TRNG_FAILED = 34
} basic_err_t ;