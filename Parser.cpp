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

static inline void parser_error(Parser *parser, Token *token, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String message = string_vprint(fmt, args);
	va_end(args);

	auto error      = new Error_Node;
	error->message  = message;
	error->next     = nullptr;

	error->location.start_row     = token->row;
	error->location.start_column  = token->column;
	error->location.finish_row    = token->row;
	error->location.finish_column = token->column;
	error->location.start         = token->offset;
	error->location.finish        = token->offset + token->content.length;

	parser->error.last->next = error;
	parser->error.last       = error;

	parser->error_count += 1;
}

//
//
//

static inline bool parser_should_continue(Parser *parser) {
	auto token      = lexer_current_token(&parser->lexer);
	parser->parsing = parser->parsing && token->kind != TOKEN_KIND_END;
	return parser->parsing;
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

		parser->location.start_row     = token->row;
		parser->location.start_column  = token->column;
		parser->location.start         = token->offset;

		parser->location.finish_row    = parser->location.start_row;
		parser->location.finish_column = parser->location.start_column;
		parser->location.finish        = parser->location.start;

		parser->value                  = parser->lexer.value;

		lexer_next(&parser->lexer);
		return true;
	}
	return false;
}

static bool parser_expect_token(Parser *parser, Token_Kind kind, bool terminate = true) {
	if (parser_accept_token(parser, kind)) {
		return true;
	}

	auto token = lexer_current_token(&parser->lexer);

	const char *expected = (char *)token_kind_string(kind).data;
	const char *got = (char *)token_kind_string(token->kind).data;

	parser_error(parser, token, "Expected: %s, Got: %s\n", expected, got);
	parser->parsing = false;

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
		auto node             = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind      = Literal::REAL;
		node->value.data.real = (float)parser->value.real;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_INTEGER)) {
		auto node                = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind         = Literal::INTEGER;
		node->value.data.integer = (int32_t)parser->value.integer;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_IDENTIFIER)) {
		auto node = parser_new_syntax_node<Syntax_Node_Identifier>(parser);
		String name;
		name.length = parser->value.string.length;
		name.data   = parser->value.string.data;
		node->name  = string_builder_copy(parser->builder, name);
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

	while (parser_should_continue(parser)) {
		// assignment
		if (parser_accept_token(parser, TOKEN_KIND_EQUALS)) {
			auto assignment = parser_new_syntax_node<Syntax_Node_Assignment>(parser);
			parser_finish_syntax_node(parser, assignment);
			assignment->left  = left;
			assignment->right = parse_root_expression(parser);
			return assignment;
		}

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
				parser_finish_syntax_node(parser, node);
				node->op = token;
				node->left = left;
				node->right = parse_expression(parser, op_prec);
				left = node;
				break;
			}
		}
	}

	return left;
}

Syntax_Node_Expression *parse_root_expression(Parser *parser) {
	auto expression = parser_new_syntax_node<Syntax_Node_Expression>(parser);
	expression->child = parse_expression(parser, 0);

	if (!expression->child) {
		expression->child = parser_new_syntax_node<Syntax_Node>(parser);
		parser_finish_syntax_node(parser, expression->child);
	}

	parser_finish_syntax_node(parser, expression);
	return expression;
}

Syntax_Node_Type *parse_type(Parser *parser) {
	auto type = parser_new_syntax_node<Syntax_Node_Type>(parser);

	if (parser_accept_token(parser, TOKEN_KIND_INT)) {
		type->token_type = TOKEN_KIND_INT;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_FLOAT)) {
		type->token_type = TOKEN_KIND_FLOAT;
	}
	else {
		auto token = lexer_current_token(&parser->lexer);
		parser_error(parser, token, "Expected type, got: %s\n", token_kind_string(token->kind).data);
	}

	parser_finish_syntax_node(parser, type);
	return type;
}

Syntax_Node_Declaration *parse_declaration(Parser *parser) {
	auto declaration = parser_new_syntax_node<Syntax_Node_Declaration>(parser);

	if (parser_accept_token(parser, TOKEN_KIND_CONST)) {
		declaration->flags |= DECLARATION_IS_CONSTANT;
	}
	else if (!parser_accept_token(parser, TOKEN_KIND_VAR)) {
		auto token = lexer_current_token(&parser->lexer);
		parser_error(parser, token, "Expected declaration 'var' or 'const'\n");
		parser->parsing = false;
	}

	if (parser_expect_token(parser, TOKEN_KIND_IDENTIFIER)) {
		String identifier;
		identifier.length       = parser->value.string.length;
		identifier.data         = parser->value.string.data;
		declaration->identifier = string_builder_copy(parser->builder, identifier);
	}

	parser_expect_token(parser, TOKEN_KIND_COLON);

	declaration->type = parse_type(parser);

	parser_finish_syntax_node(parser, declaration);
	return declaration;
}

Syntax_Node_Statement *parse_statement(Parser *parser) {
	auto statement = parser_new_syntax_node<Syntax_Node_Statement>(parser);

	// declaration
	if (parser_peek_token(parser, TOKEN_KIND_VAR) ||
		parser_peek_token(parser, TOKEN_KIND_CONST)) {
		auto declaration = parse_declaration(parser);
		statement->node  = declaration;

		parser_expect_token(parser, TOKEN_KIND_SEMICOLON);
	}

	// simple expressions
	else {
		auto expression = parse_root_expression(parser);
		statement->node = expression;

		if (!parser_accept_token(parser, TOKEN_KIND_SEMICOLON)) {
			auto token = lexer_current_token(&parser->lexer);
			parser_error(parser, token, "Unexpected token: %.*s\n", (int)token->content.length, token->content.data);
			parser->parsing = false;
		}

	}

	parser_finish_syntax_node(parser, statement);
	return statement;
}

Syntax_Node_Block *parse_block(Parser *parser) {
	auto block = parser_new_syntax_node<Syntax_Node_Block>(parser);

	Syntax_Node_Statement statement_stub_head;
	Syntax_Node_Statement *parent_statement = &statement_stub_head;
	uint64_t statement_count                = 0;

	while (parser_should_continue(parser)) {
		auto statement         = parse_statement(parser);
		parent_statement->next = statement;
		parent_statement       = statement;
		statement_count += 1;
	}

	block->statement_head  = statement_stub_head.next;
	block->statement_count = statement_count;

	parser_finish_syntax_node(parser, block);
	return block;
}

//
//
//

void parser_init(Parser *parser, String content, String_Builder *builder) {
	lexer_init(&parser->lexer, content);

	parser->error.first.message = "";
	parser->error.first.next    = nullptr;

	parser->error.last  = &parser->error.first;
	parser->error_count = 0;
	parser->parsing     = true;

	if (!ParseTableInitialize) {
		parser_init_precedence();
		ParseTableInitialize = true;
	}

	lexer_next(&parser->lexer);

	parser->builder = builder;
}

//
//
//
