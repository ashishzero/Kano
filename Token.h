#pragma once
#include "Common.h"

enum Token_Kind {
	TOKEN_KIND_ERROR = 0,

	TOKEN_KIND_SEMICOLON,
	TOKEN_KIND_COLON,
	TOKEN_KIND_DEREFERENCE,
	TOKEN_KIND_EQUALS,
	TOKEN_KIND_OPEN_BRACKET,
	TOKEN_KIND_CLOSE_BRACKET,

	TOKEN_KIND_PLUS,
	TOKEN_KIND_MINUS,
	TOKEN_KIND_ASTERISK,
	TOKEN_KIND_DIVISION,
	TOKEN_KIND_REMAINDER,

	TOKEN_KIND_BITWISE_SHIFT_RIGHT,
	TOKEN_KIND_BITWISE_SHIFT_LEFT,

	TOKEN_KIND_BITWISE_AND,
	TOKEN_KIND_BITWISE_XOR,
	TOKEN_KIND_BITWISE_OR,
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

	TOKEN_KIND_VAR,
	TOKEN_KIND_CONST,

	TOKEN_KIND_TRUE,
	TOKEN_KIND_FALSE,

	TOKEN_KIND_INT,
	TOKEN_KIND_FLOAT,
	TOKEN_KIND_BOOL,

	TOKEN_KIND_IDENTIFIER,

	TOKEN_KIND_END,

	_TOKEN_KIND_COUNT
};

struct Token {
	Token_Kind kind;
	String     content;
	size_t     row;
	size_t     column;
	size_t     offset;
};

union Token_Value {
	double   real;
	uint64_t integer;
	struct {
		int64_t length;
		uint8_t *data;
	} string;
};

//
//
//

static inline String token_kind_string(Token_Kind kind) {
	static String strings[] = {
		"-unknown-",

		";", ":", "?", "=",

		"(", ")",

		"+", "-", "*", "/", "%",

		">>", "<<",

		"&", "^", "|", "~",

		"!",

		">", "<", ">=", "<=",
		"==", "!=",

		"real number",
		"integer number",

		"var", "const",

		"true", "false",

		"int", "float", "bool",

		"identifier",

		"-end-"
	};
	static_assert(ArrayCount(strings) == _TOKEN_KIND_COUNT);
	return strings[kind];
}
