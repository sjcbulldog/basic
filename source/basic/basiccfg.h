#pragma once

// The maximum number of dimensions for an array
#define BASIC_MAX_DIMS							(3)

// The maximum index for a single dimension in an array
#define BASIC_MAX_SINGLE_DIM					(65536 * 4)

// The maximum length of a reserved keyword
#define BASIC_MAX_KEYWORD_LENGTH				(16)

// The maximum length of a variable name.
#define BASIC_MAX_VARIABLE_LENGTH				(32)

// The maximum length of a numeric constant
#define BASIC_MAX_NUMERIC_CONSTANT_LENGTH		(32)

// This should be bigger than BASIC_MAX_NUMERIC_CONSTANT_LENGTH 
// and BASIC_MAX_VARIABLE_LENGTH.  This also sets the maximum length
// of a string constant.
#define BASIC_PARSE_BUFFER_LENGTH				(128)

// The stack depth of the expression parser 
#define BASIC_MAX_EXPR_DEPTH					(48)

