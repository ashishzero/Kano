#pragma once
#include "Common.h"
#include "Token.h"

struct Lexer
{
	String      content;

	uint8_t *   cursor;
	size_t      row;
	size_t      column;

	uint8_t     buffer[256];
	bool        finished;

	Token       token;

	Token_Value value;
};

void          lexer_next(Lexer *lexer);
void          lexer_init(Lexer *lexer, String content);

inline Token *lexer_current_token(Lexer *lexer)
{
	return &lexer->token;
}
inline Token *lexer_next_token(Lexer *lexer)
{
	lexer_next(lexer);
	return &lexer->token;
}
