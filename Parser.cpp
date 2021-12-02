#include "Parser.h"

#include <stdio.h>
#include <stdlib.h>

static inline String string_vprint(const char *fmt, va_list list) {
	va_list args;
	va_copy(args, list);
	int   len = 1 + vsnprintf(NULL, 0, fmt, args);
	char *buf = (char *)malloc(len);
	vsnprintf(buf, len, fmt, list);
	va_end(args);
	return String((uint8_t *)buf, len - 1);
}

static inline String string_print(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String string = string_vprint(fmt, args);
	va_end(args);
	return string;
}

//
//
//

static bool ParseTableInitialize = false;

static uint32_t UnaryOperatorPrecedence[_TOKEN_KIND_COUNT];
static uint32_t BinaryOperatorPrecedence[_TOKEN_KIND_COUNT];

static void parser_init_precedence() {
	UnaryOperatorPrecedence[TOKEN_KIND_PLUS] = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_MINUS] = 150;

	//
	//
	//

	BinaryOperatorPrecedence[TOKEN_KIND_ASTRICK] = 60;
	BinaryOperatorPrecedence[TOKEN_KIND_DIVISION] = 60;

	BinaryOperatorPrecedence[TOKEN_KIND_PLUS] = 55;
	BinaryOperatorPrecedence[TOKEN_KIND_MINUS] = 55;
}

//
//
//

static inline void parser_error(Parser *parser, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String message = string_vprint(fmt, args);
	va_end(args);

	auto error = new Error_Node;
	error->message = message;
	error->location = parser->location;
	error->next = nullptr;

	parser->error.last->next = error;
	parser->error.last = error;

	parser->error_count += 1;
}

//
//
//

static inline bool parser_end(Parser *parser) {
	auto token = lexer_current_token(&parser->lexer);
	return token->kind == TOKEN_KIND_END;
}

static bool parser_peek_token(Parser *parser, Token_Kind kind) {
	auto token = lexer_current_token(&parser->lexer);
	if (token->kind == kind)
		return true;
	return false;
}

static bool parser_accept_token(Parser *parser, Token_Kind kind) {
	if (parser_peek_token(parser, kind)) {
		auto token = lexer_current_token(&parser->lexer);

		parser->location.start_row = token->row;
		parser->location.start_column = token->column;
		parser->location.start = token->offset;

		parser->location.finish_row = parser->location.start_row;
		parser->location.finish_column = parser->location.start_column;
		parser->location.finish = parser->location.start;

		parser->value = parser->lexer.value;

		lexer_next(&parser->lexer);
		return true;
	}
	return false;
}

static bool parser_expect_token(Parser *parser, Token_Kind kind) {
	if (parser_accept_token(parser, kind)) {
		return true;
	}

	auto token = lexer_current_token(&parser->lexer);

	const char *expected = (char *)token_kind_string(kind).data;
	const char *got = (char *)token_kind_string(token->kind).data;

	parser_error(parser, "Expected: %s, Got: %s\n", expected, got);

	return false;
}

//
//
//

template <typename T>
static T *parser_new_syntax_node(Parser *parser) {
	T *node = new T;
	node->location.start_row = parser->location.start_row;
	node->location.start_column = parser->location.start_column;
	node->location.start = parser->location.start;

	node->location.finish_row = node->location.start_row;
	node->location.finish_column = node->location.start_column;
	node->location.finish = node->location.start;
	return node;
}

static void parser_finish_syntax_node(Parser *parser, Syntax_Node *ast) {
	ast->location.finish_row = parser->location.finish_row;
	ast->location.finish_column = parser->location.finish_column;
	ast->location.finish = parser->location.finish;
}

//
//
//

Syntax_Node *parse_subexpression(Parser *parser, uint32_t prec) {
	if (parser_accept_token(parser, TOKEN_KIND_OPEN_BRACKET)) {
		auto node = parse_expression(parser, 0);
		parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_REAL)) {
		auto node = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value = parser->value.real;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_INTEGER)) {
		auto node = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value = (double)parser->value.integer;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	static const Token_Kind UnaryOpTokens[] = {
		TOKEN_KIND_PLUS, TOKEN_KIND_MINUS
	};

	auto token   = lexer_current_token(&parser->lexer);
	auto op_prec = UnaryOperatorPrecedence[token->kind];

	if (op_prec <= prec)
		return nullptr;

	for (uint32_t index = 0; index < ArrayCount(UnaryOpTokens); ++index) {
		Token_Kind token = UnaryOpTokens[index];
		if (parser_accept_token(parser, token)) {
			auto node = parser_new_syntax_node<Syntax_Node_Unary_Operator>(parser);
			node->op = token;
			node->child = parse_expression(parser, op_prec);
			parser_finish_syntax_node(parser, node);
			return node;
		}
	}

	return nullptr;
}

Syntax_Node *parse_expression(Parser *parser, uint32_t prec) {
	auto left = parse_subexpression(parser, prec);

	if (!left) return nullptr;

	while (!parser_end(parser)) {
		static const Token_Kind BinaryOpTokens[] = {
			TOKEN_KIND_PLUS, TOKEN_KIND_MINUS, TOKEN_KIND_ASTRICK, TOKEN_KIND_DIVISION
		};

		auto token = lexer_current_token(&parser->lexer);

		auto op_prec = BinaryOperatorPrecedence[token->kind];
		if (op_prec <= prec)
			break;

		for (uint32_t index = 0; index < ArrayCount(BinaryOpTokens); ++index) {
			Token_Kind token = BinaryOpTokens[index];
			if (parser_accept_token(parser, token)) {
				auto node = parser_new_syntax_node<Syntax_Node_Binary_Operator>(parser);
				node->op = token;
				node->left = left;
				node->right = parse_expression(parser, op_prec);
				parser_finish_syntax_node(parser, node);
				left = node;
				break;
			}
		}
	}

	return left;
}

//
//
//

void parser_init(Parser *parser, String content) {
	lexer_init(&parser->lexer, content);

	parser->error.first.message = "";
	parser->error.first.next = nullptr;

	parser->error.last = &parser->error.first;
	parser->error_count = 0;

	if (!ParseTableInitialize) {
		parser_init_precedence();
		ParseTableInitialize = true;
	}

	lexer_next(&parser->lexer);
}

//
//
//
