#pragma once
#include "Common.h"

enum Token_Kind
{
	TOKEN_KIND_ERROR = 0,

	TOKEN_KIND_SEMICOLON,
	TOKEN_KIND_COLON,
	TOKEN_KIND_COMMA,
	TOKEN_KIND_DASH_ARROW,
	TOKEN_KIND_DEREFERENCE,
	TOKEN_KIND_PERIOD,
	TOKEN_KIND_EQUALS,
	TOKEN_KIND_OPEN_BRACKET,
	TOKEN_KIND_CLOSE_BRACKET,
	TOKEN_KIND_OPEN_CURLY_BRACKET,
	TOKEN_KIND_CLOSE_CURLY_BRACKET,
	TOKEN_KIND_OPEN_SQUARE_BRACKET,
	TOKEN_KIND_CLOSE_SQUARE_BRACKET,

	TOKEN_KIND_PLUS,
	TOKEN_KIND_MINUS,
	TOKEN_KIND_ASTERISK,
	TOKEN_KIND_DIVISION,
	TOKEN_KIND_REMAINDER,

	TOKEN_KIND_COMPOUND_PLUS,
	TOKEN_KIND_COMPOUND_MINUS,
	TOKEN_KIND_COMPOUND_MULTIPLY,
	TOKEN_KIND_COMPOUND_DIVIDE,
	TOKEN_KIND_COMPOUND_REMAINDER,

	TOKEN_KIND_BITWISE_SHIFT_RIGHT,
	TOKEN_KIND_BITWISE_SHIFT_LEFT,

	TOKEN_KIND_COMPOUND_BITWISE_SHIFT_RIGHT,
	TOKEN_KIND_COMPOUND_BITWISE_SHIFT_LEFT,

	TOKEN_KIND_BITWISE_AND,
	TOKEN_KIND_BITWISE_XOR,
	TOKEN_KIND_BITWISE_OR,

	TOKEN_KIND_COMPOUND_BITWISE_AND,
	TOKEN_KIND_COMPOUND_BITWISE_XOR,
	TOKEN_KIND_COMPOUND_BITWISE_OR,

	TOKEN_KIND_BITWISE_NOT,

	TOKEN_KIND_LOGICAL_NOT,

	TOKEN_KIND_RELATIONAL_GREATER,
	TOKEN_KIND_RELATIONAL_LESS,
	TOKEN_KIND_RELATIONAL_GREATER_EQUAL,
	TOKEN_KIND_RELATIONAL_LESS_EQUAL,
	TOKEN_KIND_COMPARE_EQUAL,
	TOKEN_KIND_COMPARE_NOT_EQUAL,

	TOKEN_KIND_REAL,
	TOKEN_KIND_INTEGER,
	TOKEN_KIND_STRING,

	TOKEN_KIND_VAR,
	TOKEN_KIND_CONST,

	TOKEN_KIND_TRUE,
	TOKEN_KIND_FALSE,

	TOKEN_KIND_BYTE,
	TOKEN_KIND_INT,
	TOKEN_KIND_FLOAT,
	TOKEN_KIND_BOOL,
	TOKEN_KIND_DOUBLE_PERDIOD,

	TOKEN_KIND_IF,
	TOKEN_KIND_THEN,
	TOKEN_KIND_ELSE,

	TOKEN_KIND_FOR,
	TOKEN_KIND_WHILE,
	TOKEN_KIND_DO,

	TOKEN_KIND_SIZE_OF,
	TOKEN_KIND_TYPE_OF,

	TOKEN_KIND_PROC,
	TOKEN_KIND_STRUCT,
	TOKEN_KIND_RETURN,
	TOKEN_KIND_CAST,
	TOKEN_KIND_VOID,
	TOKEN_KIND_NULL,

	TOKEN_KIND_IDENTIFIER,

	TOKEN_KIND_END,

	_TOKEN_KIND_COUNT
};

struct Token
{
	Token_Kind kind;
	String     content;
	size_t     row;
	size_t     column;
	size_t     offset;
};

union Token_Value {
	double   real;
	uint64_t integer;
	struct
	{
		int64_t  length;
		uint8_t *data;
	} string;
};

//
//
//

static inline String token_kind_string(Token_Kind kind)
{
	static String strings[] = {"-unknown-",

	                           ";",           ":",
	                           ",",           "->",
	                           "?",           ".",
	                           "=",

	                           "(",           ")",
	                           "{",           "}",
	                           "[",           "]",

	                           "+",           "-",
	                           "*",           "/",
	                           "%",

	                           "+=",          "-=",
	                           "*=",          "/=",
	                           "%=",

	                           ">>",          "<<",

	                           ">>=",         "<<=",

	                           "&",           "^",
	                           "|",

	                           "&=",          "^=",
	                           "|=",

	                           "~",

	                           "!",

	                           ">",           "<",
	                           ">=",          "<=",
	                           "==",          "!=",

	                           "real number", "integer number",
	                           "string",

	                           "var",         "const",

	                           "true",        "false",

	                           "byte", "int",         "float",
	                           "bool",        "..",

	                           "if",          "then",
	                           "else",

	                           "for",         "while",
	                           "do",

	                           "size_of",     "type_of",

	                           "proc",        "struct",
	                           "return",      "cast",
	                           "void",        "null",

	                           "identifier",

	                           "-end-"};
	static_assert(ArrayCount(strings) == _TOKEN_KIND_COUNT);
	return strings[kind];
}
