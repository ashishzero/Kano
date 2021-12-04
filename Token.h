#pragma once
#include "Common.h"

enum Token_Kind {
	TOKEN_KIND_ERROR = 0,

	TOKEN_KIND_SEMICOLON,
	TOKEN_KIND_OPEN_BRACKET,
	TOKEN_KIND_CLOSE_BRACKET,

	TOKEN_KIND_PLUS,
	TOKEN_KIND_MINUS,
	TOKEN_KIND_ASTRICK,
	TOKEN_KIND_DIVISION,

	TOKEN_KIND_REAL,
	TOKEN_KIND_INTEGER,

	TOKEN_KIND_END,

	_TOKEN_KIND_COUNT
};

struct Token {
	Token_Kind kind;
	String content;
	size_t row;
	size_t column;
	size_t offset;
};

union Token_Value {
	double real;
	uint64_t integer;
};

//
//
//

static inline String token_kind_string(Token_Kind kind) {
	static String strings[] = {
		"-unknown-",

		";",

		"(", ")",

		"+", "-", "*", "/",

		"real number",
		"integer number",

		"-end-"
	};
	static_assert(ArrayCount(strings) == _TOKEN_KIND_COUNT);
	return strings[kind];
}
