#include "Parser.h"

#include <stdio.h>
#include <stdlib.h>

static bool     ParseTableInitialize = false;

static uint32_t UnaryOperatorPrecedence[_TOKEN_KIND_COUNT];
static uint32_t BinaryOperatorPrecedence[_TOKEN_KIND_COUNT];

static void     parser_init_precedence()
{
	UnaryOperatorPrecedence[TOKEN_KIND_PLUS]        = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_MINUS]       = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_BITWISE_NOT] = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_LOGICAL_NOT] = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_ASTERISK]    = 150;
	UnaryOperatorPrecedence[TOKEN_KIND_DEREFERENCE] = 150;

	//
	//
	//

	BinaryOperatorPrecedence[TOKEN_KIND_PERIOD]                       = 65;
	BinaryOperatorPrecedence[TOKEN_KIND_OPEN_BRACKET]                 = 65;
	BinaryOperatorPrecedence[TOKEN_KIND_OPEN_SQUARE_BRACKET]          = 65;

	BinaryOperatorPrecedence[TOKEN_KIND_ASTERISK]                     = 60;
	BinaryOperatorPrecedence[TOKEN_KIND_DIVISION]                     = 60;
	BinaryOperatorPrecedence[TOKEN_KIND_REMAINDER]                    = 60;

	BinaryOperatorPrecedence[TOKEN_KIND_PLUS]                         = 55;
	BinaryOperatorPrecedence[TOKEN_KIND_MINUS]                        = 55;

	BinaryOperatorPrecedence[TOKEN_KIND_BITWISE_SHIFT_RIGHT]          = 50;
	BinaryOperatorPrecedence[TOKEN_KIND_BITWISE_SHIFT_LEFT]           = 50;

	BinaryOperatorPrecedence[TOKEN_KIND_RELATIONAL_GREATER]           = 45;
	BinaryOperatorPrecedence[TOKEN_KIND_RELATIONAL_LESS]              = 45;
	BinaryOperatorPrecedence[TOKEN_KIND_RELATIONAL_GREATER_EQUAL]     = 45;
	BinaryOperatorPrecedence[TOKEN_KIND_RELATIONAL_LESS_EQUAL]        = 45;

	BinaryOperatorPrecedence[TOKEN_KIND_COMPARE_EQUAL]                = 40;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPARE_NOT_EQUAL]            = 40;

	BinaryOperatorPrecedence[TOKEN_KIND_BITWISE_AND]                  = 30;
	BinaryOperatorPrecedence[TOKEN_KIND_BITWISE_XOR]                  = 25;
	BinaryOperatorPrecedence[TOKEN_KIND_BITWISE_OR]                   = 20;

	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_PLUS]                = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_MINUS]               = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_MULTIPLY]            = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_DIVIDE]              = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_REMAINDER]           = 15;

	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_BITWISE_SHIFT_RIGHT] = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_BITWISE_SHIFT_LEFT]  = 15;

	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_BITWISE_AND]         = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_BITWISE_XOR]         = 15;
	BinaryOperatorPrecedence[TOKEN_KIND_COMPOUND_BITWISE_OR]          = 15;

	BinaryOperatorPrecedence[TOKEN_KIND_EQUALS]                       = 15;
}

//
//
//

static inline void parser_error(Parser *parser, Token *token, const char *fmt, ...)
{
	Syntax_Location location;
	location.start_row     = token->row;
	location.start_column  = token->column;
	location.finish_row    = token->row;
	location.finish_column = token->column;
	location.start         = token->offset;
	location.finish        = token->offset + token->content.length;

	va_list args;
	va_start(args, fmt);

	error_vfmt(&parser->error, location, fmt, args);

	va_end(args);

	parser->parsing = false;
}

//
//
//

static inline bool parser_should_continue(Parser *parser)
{
	auto token      = lexer_current_token(&parser->lexer);
	parser->parsing = parser->parsing && token->kind != TOKEN_KIND_END;
	return parser->parsing;
}

static bool parser_peek_token(Parser *parser, Token_Kind kind)
{
	auto token = lexer_current_token(&parser->lexer);
	if (token->kind == kind)
		return true;
	return false;
}

static bool parser_accept_token(Parser *parser, Token_Kind kind)
{
	if (parser_peek_token(parser, kind))
	{
		auto token                     = lexer_current_token(&parser->lexer);

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

static bool parser_expect_token(Parser *parser, Token_Kind kind, bool terminate = true)
{
	if (parser_accept_token(parser, kind))
	{
		return true;
	}

	auto        token    = lexer_current_token(&parser->lexer);

	const char *expected = (char *)token_kind_string(kind).data;
	const char *got      = (char *)token_kind_string(token->kind).data;

	parser_error(parser, token, "Expected: %s, Got: %s\n", expected, got);
	parser->parsing = false;

	return false;
}

//
//
//

template <typename T> static T *parser_new_syntax_node(Parser *parser)
{
	T *node                      = new T;
	node->location.start_row     = parser->location.start_row;
	node->location.start_column  = parser->location.start_column;
	node->location.start         = parser->location.start;

	node->location.finish_row    = node->location.start_row;
	node->location.finish_column = node->location.start_column;
	node->location.finish        = node->location.start;
	return node;
}

static void parser_finish_syntax_node(Parser *parser, Syntax_Node *ast)
{
	ast->location.finish_row    = parser->location.finish_row;
	ast->location.finish_column = parser->location.finish_column;
	ast->location.finish        = parser->location.finish;
}

//
//
//

Syntax_Node_Procedure *parse_procedure(Parser *parser);

Syntax_Node *parse_subexpression(Parser *parser, uint32_t prec)
{
	if (parser_accept_token(parser, TOKEN_KIND_OPEN_BRACKET))
	{
		auto node = parse_expression(parser, 0);
		parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_REAL))
	{
		auto node             = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind      = Literal::REAL;
		node->value.data.real = (float)parser->value.real;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_INTEGER))
	{
		auto node                = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		if (parser->value.integer >= 0 && parser->value.integer < 255)
		{
			node->value.kind = Literal::BYTE;
			node->value.data.integer = (int32_t)parser->value.integer;
		}
		else
		{
			node->value.kind = Literal::INTEGER;
			node->value.data.integer = (int32_t)parser->value.integer;
		}
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_STRING))
	{
		String string;
		string.length = parser->value.string.length;
		string.data   = new uint8_t[string.length + 1];
		memcpy(string.data, parser->value.string.data, string.length);
		string.data[string.length] = 0;

		auto node                  = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind           = Literal::STRING;
		node->value.data.string    = string;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_TRUE))
	{
		auto node                = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind         = Literal::BOOL;
		node->value.data.boolean = true;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_FALSE))
	{
		auto node                = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind         = Literal::BOOL;
		node->value.data.boolean = false;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_peek_token(parser, TOKEN_KIND_PROC))
	{
		return parse_procedure(parser);
	}

	if (parser_accept_token(parser, TOKEN_KIND_NULL))
	{
		auto node        = parser_new_syntax_node<Syntax_Node_Literal>(parser);
		node->value.kind = Literal::NULL_POINTER;
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_IDENTIFIER))
	{
		auto   node = parser_new_syntax_node<Syntax_Node_Identifier>(parser);
		String name;
		name.length = parser->value.string.length;
		name.data   = parser->value.string.data;
		node->name  = name;
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_SIZE_OF))
	{
		auto node = parser_new_syntax_node<Syntax_Node_Size_Of>(parser);
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{
			node->type = parse_type(parser);
			parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);
		}
		parser_finish_syntax_node(parser, node);
		return node;
	}

	if (parser_accept_token(parser, TOKEN_KIND_CAST))
	{
		auto node = parser_new_syntax_node<Syntax_Node_Type_Cast>(parser);
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{
			node->type = parse_type(parser);
			if (parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET))
			{
				auto expression   = parser_new_syntax_node<Syntax_Node_Expression>(parser);
				expression->child = parse_subexpression(parser, 0);
				parser_finish_syntax_node(parser, expression);
				node->expression = expression;
			}
		}
		parser_finish_syntax_node(parser, node);
		return node;
	}

	static const Token_Kind UnaryOpTokens[] = {TOKEN_KIND_PLUS,        TOKEN_KIND_MINUS,    TOKEN_KIND_BITWISE_NOT,
	                                           TOKEN_KIND_LOGICAL_NOT, TOKEN_KIND_ASTERISK, TOKEN_KIND_DEREFERENCE};

	auto                    token           = lexer_current_token(&parser->lexer);
	auto                    op_prec         = UnaryOperatorPrecedence[token->kind];

	if (op_prec < prec)
		return nullptr;

	for (uint32_t index = 0; index < ArrayCount(UnaryOpTokens); ++index)
	{
		Token_Kind token = UnaryOpTokens[index];
		if (parser_accept_token(parser, token))
		{
			auto node   = parser_new_syntax_node<Syntax_Node_Unary_Operator>(parser);
			node->op    = token;
			node->child = parse_expression(parser, op_prec);
			parser_finish_syntax_node(parser, node);
			return node;
		}
	}

	return nullptr;
}

struct Procedure_Call
{
	uint64_t                         count;
	Syntax_Node_Procedure_Parameter *head;
};

Procedure_Call parse_procedure_parameters(Parser *parser)
{
	uint64_t                         count = 0;

	Syntax_Node_Procedure_Parameter  stub_head;
	Syntax_Node_Procedure_Parameter *parent = &stub_head;

	while (parser_should_continue(parser))
	{
		if (parser_peek_token(parser, TOKEN_KIND_CLOSE_BRACKET))
			break;

		if (count)
		{
			parser_expect_token(parser, TOKEN_KIND_COMMA);
		}

		auto param        = parser_new_syntax_node<Syntax_Node_Procedure_Parameter>(parser);

		param->expression = parse_root_expression(parser);

		if (!param->expression->child)
		{
			auto site = lexer_current_token(&parser->lexer);
			parser_error(parser, site, "Expected expression");
			break;
		}

		parent->next = param;
		parent       = param;

		count += 1;
	}

	Procedure_Call call;
	call.count = count;
	call.head  = stub_head.next;

	return call;
}

Syntax_Node *parse_expression(Parser *parser, uint32_t prec)
{
	auto left = parse_subexpression(parser, prec);

	if (!left)
		return nullptr;

	while (parser_should_continue(parser))
	{
		auto token   = lexer_current_token(&parser->lexer);

		auto op_prec = BinaryOperatorPrecedence[token->kind];
		if (op_prec <= prec)
			break;

		// assignment
		if (parser_accept_token(parser, TOKEN_KIND_EQUALS))
		{
			auto assignment = parser_new_syntax_node<Syntax_Node_Assignment>(parser);
			parser_finish_syntax_node(parser, assignment);

			auto left_expression      = parser_new_syntax_node<Syntax_Node_Expression>(parser);
			left_expression->child    = left;
			left_expression->location = left->location;

			assignment->left          = left_expression;
			assignment->right         = parse_root_expression(parser);
			return assignment;
		}

		if (parser_peek_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{
			auto node = parser_new_syntax_node<Syntax_Node_Procedure_Call>(parser);
			parser_accept_token(parser, TOKEN_KIND_OPEN_BRACKET);

			auto parameters = parse_procedure_parameters(parser);
			parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);

			parser_finish_syntax_node(parser, node);

			auto procedure = parser_new_syntax_node<Syntax_Node_Expression>(parser);
			parser_finish_syntax_node(parser, procedure);
			procedure->location   = left->location;
			procedure->child      = left;

			node->procedure       = procedure;
			node->parameter_count = parameters.count;
			node->parameters      = parameters.head;
			left                  = node;
			continue;
		}

		if (parser_peek_token(parser, TOKEN_KIND_OPEN_SQUARE_BRACKET))
		{
			auto node = parser_new_syntax_node<Syntax_Node_Subscript>(parser);
			parser_accept_token(parser, TOKEN_KIND_OPEN_SQUARE_BRACKET);

			auto subscript = parse_root_expression(parser);
			parser_expect_token(parser, TOKEN_KIND_CLOSE_SQUARE_BRACKET);

			parser_finish_syntax_node(parser, node);

			auto expression = parser_new_syntax_node<Syntax_Node_Expression>(parser);
			parser_finish_syntax_node(parser, expression);
			expression->location = left->location;
			expression->child    = left;

			node->expression     = expression;
			node->subscript      = subscript;
			left                 = node;
			continue;
		}

		static const Token_Kind BinaryOpTokens[]      = {TOKEN_KIND_PLUS,
                                                    TOKEN_KIND_MINUS,
                                                    TOKEN_KIND_ASTERISK,
                                                    TOKEN_KIND_DIVISION,
                                                    TOKEN_KIND_REMAINDER,
                                                    TOKEN_KIND_BITWISE_SHIFT_RIGHT,
                                                    TOKEN_KIND_BITWISE_SHIFT_LEFT,
                                                    TOKEN_KIND_BITWISE_AND,
                                                    TOKEN_KIND_BITWISE_XOR,
                                                    TOKEN_KIND_BITWISE_OR,
                                                    TOKEN_KIND_RELATIONAL_GREATER,
                                                    TOKEN_KIND_RELATIONAL_LESS,
                                                    TOKEN_KIND_RELATIONAL_GREATER_EQUAL,
                                                    TOKEN_KIND_RELATIONAL_LESS_EQUAL,
                                                    TOKEN_KIND_COMPARE_EQUAL,
                                                    TOKEN_KIND_COMPARE_NOT_EQUAL,
                                                    TOKEN_KIND_COMPOUND_PLUS,
                                                    TOKEN_KIND_COMPOUND_MINUS,
                                                    TOKEN_KIND_COMPOUND_MULTIPLY,
                                                    TOKEN_KIND_COMPOUND_DIVIDE,
                                                    TOKEN_KIND_COMPOUND_REMAINDER,
                                                    TOKEN_KIND_COMPOUND_BITWISE_SHIFT_RIGHT,
                                                    TOKEN_KIND_COMPOUND_BITWISE_SHIFT_LEFT,
                                                    TOKEN_KIND_COMPOUND_BITWISE_AND,
                                                    TOKEN_KIND_COMPOUND_BITWISE_XOR,
                                                    TOKEN_KIND_COMPOUND_BITWISE_OR,
                                                    TOKEN_KIND_PERIOD};

		bool                    found_binary_operator = false;

		for (uint32_t index = 0; index < ArrayCount(BinaryOpTokens); ++index)
		{
			Token_Kind token = BinaryOpTokens[index];
			if (parser_accept_token(parser, token))
			{
				auto node = parser_new_syntax_node<Syntax_Node_Binary_Operator>(parser);
				parser_finish_syntax_node(parser, node);
				node->op              = token;
				node->left            = left;
				node->right           = parse_expression(parser, op_prec);
				left                  = node;
				found_binary_operator = true;
				break;
			}
		}

		if (!found_binary_operator)
			break;
	}

	return left;
}

Syntax_Node_Expression *parse_root_expression(Parser *parser)
{
	auto expression = parser_new_syntax_node<Syntax_Node_Expression>(parser);

	if (parser_accept_token(parser, TOKEN_KIND_RETURN))
	{
		auto return_node        = parser_new_syntax_node<Syntax_Node_Return>(parser);
		return_node->expression = parse_expression(parser, 0);
		parser_finish_syntax_node(parser, return_node);
		expression->child = return_node;
	}
	else
	{
		auto token = lexer_current_token(&parser->lexer);
		expression->child = parse_expression(parser, 0);
		
		if (!expression->child) 
		{
			parser_error(parser, token, "Expected expression");
			expression->child = parser_new_syntax_node<Syntax_Node>(parser);
			parser_finish_syntax_node(parser, expression->child);
		}
	}

	parser_finish_syntax_node(parser, expression);

	expression->location = expression->child->location;

	return expression;
}

Syntax_Node_Procedure_Prototype_Argument *parse_procedure_prototype_argument(Parser *parser)
{
	auto arg  = parser_new_syntax_node<Syntax_Node_Procedure_Prototype_Argument>(parser);
	arg->type = parse_type(parser);
	parser_finish_syntax_node(parser, arg);
	return arg;
}

Syntax_Node_Procedure_Prototype *parse_procedure_prototype(Parser *parser)
{
	auto proc_prototype = parser_new_syntax_node<Syntax_Node_Procedure_Prototype>(parser);

	if (parser_expect_token(parser, TOKEN_KIND_PROC))
	{
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{

			Syntax_Node_Procedure_Prototype_Argument  arg_stub;
			Syntax_Node_Procedure_Prototype_Argument *parent_arg = &arg_stub;

			uint64_t                                  arg_count  = 0;
			while (parser_should_continue(parser))
			{
				if (parser_peek_token(parser, TOKEN_KIND_CLOSE_BRACKET))
				{
					break;
				}

				if (arg_count)
				{
					parser_expect_token(parser, TOKEN_KIND_COMMA);
				}

				parent_arg->next = parse_procedure_prototype_argument(parser);
				parent_arg       = parent_arg->next;

				arg_count += 1;
			}

			parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);

			proc_prototype->arguments_type = arg_stub.next;
			proc_prototype->argument_count = arg_count;
		}

		if (parser_accept_token(parser, TOKEN_KIND_DASH_ARROW))
		{
			proc_prototype->return_type = parse_type(parser);
		}
	}

	parser_finish_syntax_node(parser, proc_prototype);

	return proc_prototype;
}

Syntax_Node_Procedure_Argument *parse_procedure_argument(Parser *parser)
{
	auto arg         = parser_new_syntax_node<Syntax_Node_Procedure_Argument>(parser);

	auto token       = lexer_current_token(&parser->lexer);
	arg->declaration = parse_declaration(parser);

	if (arg->declaration->flags & SYMBOL_BIT_CONSTANT)
	{
		parser_error(parser, token, "Procedure argument can not have constant declaration");
	}

	if (arg->declaration->initializer)
	{
		parser_error(parser, token, "Procedure argument can not be initialized");
	}

	parser_finish_syntax_node(parser, arg);

	return arg;
}

Syntax_Node_Procedure *parse_procedure(Parser *parser)
{
	auto proc = parser_new_syntax_node<Syntax_Node_Procedure>(parser);

	if (parser_expect_token(parser, TOKEN_KIND_PROC))
	{
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{

			Syntax_Node_Procedure_Argument  arg_stub;
			Syntax_Node_Procedure_Argument *parent_arg = &arg_stub;

			uint64_t                        arg_count  = 0;
			while (parser_should_continue(parser))
			{
				if (parser_peek_token(parser, TOKEN_KIND_CLOSE_BRACKET))
				{
					break;
				}

				if (arg_count)
				{
					parser_expect_token(parser, TOKEN_KIND_COMMA);
				}

				parent_arg->next = parse_procedure_argument(parser);
				parent_arg       = parent_arg->next;

				arg_count += 1;
			}

			parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);

			proc->arguments      = arg_stub.next;
			proc->argument_count = arg_count;
		}

		if (parser_accept_token(parser, TOKEN_KIND_DASH_ARROW))
		{
			proc->return_type = parse_type(parser);
		}

		if (!parser_peek_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
		{
			auto token = lexer_current_token(&parser->lexer);
			parser_error(parser, token, "Expected body in procedure call");
		}
	}

	proc->body = parse_block(parser);

	parser_finish_syntax_node(parser, proc);
	return proc;
}

Syntax_Node_Struct *parse_struct(Parser *parser)
{
	auto struct_node = parser_new_syntax_node<Syntax_Node_Struct>(parser);

	if (parser_expect_token(parser, TOKEN_KIND_STRUCT))
	{
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
		{
			uint64_t                     member_count = 0;
			Syntax_Node_Declaration_List stub_head;
			stub_head.next                       = nullptr;
			Syntax_Node_Declaration_List *parent = &stub_head;

			while (parser_should_continue(parser))
			{
				if (parser_peek_token(parser, TOKEN_KIND_CLOSE_CURLY_BRACKET))
					break;

				auto site = lexer_current_token(&parser->lexer);

				auto decl = parse_declaration(parser);

				if (!decl->initializer || decl->initializer->kind == SYNTAX_NODE_EXPRESSION)
				{
					parser_expect_token(parser, TOKEN_KIND_SEMICOLON);
				}

				if (decl->flags & SYMBOL_BIT_CONSTANT)
				{
					parser_error(parser, site, "Struct member declaration can not be a constant declaration");
					break;
				}

				auto member         = new Syntax_Node_Declaration_List;
				member->declaration = decl;
				member->next        = nullptr;

				parent->next        = member;
				parent              = member;

				member_count += 1;
			}

			parser_expect_token(parser, TOKEN_KIND_CLOSE_CURLY_BRACKET);

			struct_node->member_count = member_count;
			struct_node->members      = stub_head.next;
		}
	}

	parser_finish_syntax_node(parser, struct_node);
	return struct_node;
}

Syntax_Node_Type *parse_type(Parser *parser)
{
	auto type = parser_new_syntax_node<Syntax_Node_Type>(parser);

	if (parser_accept_token(parser, TOKEN_KIND_BYTE))
	{
		type->id       = Syntax_Node_Type::BYTE;
		type->location = parser->location;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_INT))
	{
		type->id       = Syntax_Node_Type::INT;
		type->location = parser->location;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_FLOAT))
	{
		type->id       = Syntax_Node_Type::FLOAT;
		type->location = parser->location;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_BOOL))
	{
		type->id       = Syntax_Node_Type::BOOL;
		type->location = parser->location;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_DOUBLE_PERDIOD))
	{
		type->id       = Syntax_Node_Type::VARIADIC_ARGUMENT;
		type->location = parser->location;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_ASTERISK))
	{
		type->id       = Syntax_Node_Type::POINTER;
		type->location = parser->location;

		if (parser_accept_token(parser, TOKEN_KIND_VOID))
		{
			auto void_ptr = parser_new_syntax_node<Syntax_Node_Type>(parser);
			void_ptr->id  = Syntax_Node_Type::VOID;
			parser_finish_syntax_node(parser, void_ptr);
			type->type = void_ptr;
		}
		else
		{
			type->type = parse_type(parser);
		}
	}
	else if (parser_peek_token(parser, TOKEN_KIND_PROC))
	{
		type->id       = Syntax_Node_Type::PROCEDURE;
		type->location = parser->location;
		type->type     = parse_procedure_prototype(parser);
	}
	else if (parser_accept_token(parser, TOKEN_KIND_IDENTIFIER))
	{
		auto identifier = parser_new_syntax_node<Syntax_Node_Identifier>(parser);
		parser_finish_syntax_node(parser, identifier);
		type->location = identifier->location;

		String name;
		name.length      = parser->value.string.length;
		name.data        = parser->value.string.data;
		identifier->name = name;

		type->id         = Syntax_Node_Type::IDENTIFIER;
		type->type       = identifier;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_TYPE_OF))
	{
		auto node = parser_new_syntax_node<Syntax_Node_Type_Of>(parser);
		if (parser_expect_token(parser, TOKEN_KIND_OPEN_BRACKET))
		{
			node->expression = parse_root_expression(parser);
			parser_expect_token(parser, TOKEN_KIND_CLOSE_BRACKET);
		}
		parser_finish_syntax_node(parser, node);
		type->location = node->location;
		type->id       = Syntax_Node_Type::TYPE_OF;
		type->type     = node;
	}
	else if (parser_accept_token(parser, TOKEN_KIND_OPEN_SQUARE_BRACKET))
	{
		if (parser_accept_token(parser, TOKEN_KIND_CLOSE_SQUARE_BRACKET))
		{
			type->id           = Syntax_Node_Type::ARRAY_VIEW;

			auto node          = parser_new_syntax_node<Syntax_Node_Array_View>(parser);
			node->location     = type->location;

			node->element_type = parse_type(parser);

			parser_finish_syntax_node(parser, node);

			type->type = node;
		}
		else
		{
			type->id         = Syntax_Node_Type::STATIC_ARRAY;

			auto node        = parser_new_syntax_node<Syntax_Node_Static_Array>(parser);
			node->location   = type->location;
			node->expression = parse_root_expression(parser);
			parser_expect_token(parser, TOKEN_KIND_CLOSE_SQUARE_BRACKET);

			node->element_type = parse_type(parser);

			parser_finish_syntax_node(parser, node);

			type->type = node;
		}
	}
	else
	{
		auto token = lexer_current_token(&parser->lexer);
		parser_error(parser, token, "Expected type, got: %s\n", token_kind_string(token->kind).data);
	}

	parser_finish_syntax_node(parser, type);
	return type;
}

Syntax_Node_Declaration *parse_declaration(Parser *parser)
{
	uint32_t flags = 0;
	if (parser_accept_token(parser, TOKEN_KIND_CONST))
	{
		flags |= SYMBOL_BIT_CONSTANT;
	}
	else if (!parser_accept_token(parser, TOKEN_KIND_VAR))
	{
		auto token = lexer_current_token(&parser->lexer);
		parser_error(parser, token, "Expected declaration 'var' or 'const'\n");
		parser->parsing = false;
	}

	auto declaration   = parser_new_syntax_node<Syntax_Node_Declaration>(parser);
	declaration->flags = flags;

	if (parser_expect_token(parser, TOKEN_KIND_IDENTIFIER))
	{
		String identifier;
		identifier.length       = parser->value.string.length;
		identifier.data         = parser->value.string.data;
		declaration->identifier = identifier;
	}

	parser_expect_token(parser, TOKEN_KIND_COLON);

	bool parse_initialization = false;
	if (parser_accept_token(parser, TOKEN_KIND_EQUALS))
	{
		parse_initialization = true;
	}
	else
	{
		declaration->type = parse_type(parser);

		if (parser_accept_token(parser, TOKEN_KIND_EQUALS))
		{
			parse_initialization = true;
		}
	}

	if (parse_initialization)
	{
		if (parser_peek_token(parser, TOKEN_KIND_PROC))
		{
			declaration->initializer = parse_procedure(parser);
		}
		else if (parser_peek_token(parser, TOKEN_KIND_STRUCT))
		{
			if (declaration->flags & SYMBOL_BIT_CONSTANT && !declaration->type)
			{
				declaration->initializer = parse_struct(parser);
			}
			else
			{
				if (declaration->type)
					parser_error(parser, lexer_current_token(&parser->lexer),
					             "Struct declaration can't be explicitely typed");
				else
					parser_error(parser, lexer_current_token(&parser->lexer), "Struct declaration must be constant");
			}
		}
		else
		{
			declaration->initializer = parse_root_expression(parser);
		}
	}

	if (declaration->flags & SYMBOL_BIT_CONSTANT && !declaration->initializer)
	{
		auto token = lexer_current_token(&parser->lexer);
		parser_error(parser, token, "Constant expression must be initialized during declaration");
	}

	parser_finish_syntax_node(parser, declaration);
	return declaration;
}

Syntax_Node_Statement *parse_statement(Parser *parser)
{
	auto statement = parser_new_syntax_node<Syntax_Node_Statement>(parser);

	// declaration
	if (parser_peek_token(parser, TOKEN_KIND_VAR) || parser_peek_token(parser, TOKEN_KIND_CONST))
	{
		auto declaration = parse_declaration(parser);
		statement->node  = declaration;

		if (!declaration->initializer || declaration->initializer->kind == SYNTAX_NODE_EXPRESSION)
		{
			parser_expect_token(parser, TOKEN_KIND_SEMICOLON);
		}
	}

	// if
	else if (parser_accept_token(parser, TOKEN_KIND_IF))
	{
		auto if_statement       = parser_new_syntax_node<Syntax_Node_If>(parser);

		if_statement->condition = parse_root_expression(parser);

		if (parser_accept_token(parser, TOKEN_KIND_THEN))
		{
			if_statement->true_statement = parse_statement(parser);
		}
		else if (parser_peek_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
		{
			if_statement->true_statement = parse_statement(parser);
		}
		else
		{
			auto token = lexer_current_token(&parser->lexer);
			parser_error(parser, token, "Expected then or a block\n");
		}

		if (parser_accept_token(parser, TOKEN_KIND_ELSE))
		{
			if_statement->false_statement = parse_statement(parser);
		}

		parser_finish_syntax_node(parser, if_statement);

		statement->node = if_statement;
	}

	// for
	else if (parser_accept_token(parser, TOKEN_KIND_FOR))
	{
		auto for_statement            = parser_new_syntax_node<Syntax_Node_For>(parser);

		for_statement->initialization = parse_statement(parser);

		for_statement->condition      = parse_root_expression(parser);
		parser_expect_token(parser, TOKEN_KIND_SEMICOLON);

		for_statement->increment = parse_root_expression(parser);

		for_statement->body      = parse_statement(parser);

		parser_finish_syntax_node(parser, for_statement);

		statement->node = for_statement;
	}

	// while
	else if (parser_accept_token(parser, TOKEN_KIND_WHILE))
	{
		auto while_statement       = parser_new_syntax_node<Syntax_Node_While>(parser);

		while_statement->condition = parse_root_expression(parser);

		if (parser_accept_token(parser, TOKEN_KIND_DO))
		{
			while_statement->body = parse_statement(parser);
		}
		else if (parser_peek_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
		{
			while_statement->body = parse_statement(parser);
		}
		else
		{
			auto token = lexer_current_token(&parser->lexer);
			parser_error(parser, token, "Expected do or a block\n");
		}

		parser_finish_syntax_node(parser, while_statement);

		statement->node = while_statement;
	}

	// do
	else if (parser_accept_token(parser, TOKEN_KIND_DO))
	{
		auto do_statement  = parser_new_syntax_node<Syntax_Node_Do>(parser);

		do_statement->body = parse_statement(parser);

		if (parser_expect_token(parser, TOKEN_KIND_WHILE))
		{
			do_statement->condition = parse_root_expression(parser);
			parser_expect_token(parser, TOKEN_KIND_SEMICOLON);
		}

		parser_finish_syntax_node(parser, do_statement);

		statement->node = do_statement;
	}

	// block
	else if (parser_peek_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
	{
		auto block      = parse_block(parser);
		statement->node = block;
	}

	// simple expressions
	else
	{
		auto expression = parse_root_expression(parser);
		statement->node = expression;

		if (!parser_accept_token(parser, TOKEN_KIND_SEMICOLON))
		{
			auto token = lexer_current_token(&parser->lexer);
			parser_error(parser, token, "Unexpected token: %.*s\n", (int)token->content.length, token->content.data);
			parser->parsing = false;
		}
	}

	parser_finish_syntax_node(parser, statement);

	statement->location = statement->node->location;

	return statement;
}

Syntax_Node_Block *parse_block(Parser *parser)
{
	if (parser_expect_token(parser, TOKEN_KIND_OPEN_CURLY_BRACKET))
	{
		auto                   block = parser_new_syntax_node<Syntax_Node_Block>(parser);

		Syntax_Node_Statement  statement_stub_head;
		Syntax_Node_Statement *parent_statement = &statement_stub_head;
		uint64_t               statement_count  = 0;

		while (parser_should_continue(parser))
		{
			if (parser_accept_token(parser, TOKEN_KIND_CLOSE_CURLY_BRACKET))
				break;

			auto statement         = parse_statement(parser);
			parent_statement->next = statement;
			parent_statement       = statement;
			statement_count += 1;
		}

		block->statements      = statement_stub_head.next;
		block->statement_count = statement_count;

		parser_finish_syntax_node(parser, block);
		return block;
	}

	return nullptr;
}

Syntax_Node_Global_Scope *parse_global_scope(Parser *parser)
{
	auto                         global     = parser_new_syntax_node<Syntax_Node_Global_Scope>(parser);

	uint64_t                     decl_count = 0;
	Syntax_Node_Declaration_List stub_head;
	stub_head.next                       = nullptr;
	Syntax_Node_Declaration_List *parent = &stub_head;

	while (parser_should_continue(parser))
	{
		auto decl_node = parse_declaration(parser);

		if (!decl_node->initializer || decl_node->initializer->kind == SYNTAX_NODE_EXPRESSION)
		{
			parser_expect_token(parser, TOKEN_KIND_SEMICOLON);
		}

		auto decl         = new Syntax_Node_Declaration_List;
		decl->declaration = decl_node;
		decl->next        = nullptr;

		parent->next      = decl;
		parent            = decl;

		decl_count += 1;
	}

	global->declarations      = stub_head.next;
	global->declaration_count = decl_count;

	parser_finish_syntax_node(parser, global);
	return global;
}

//
//
//

void parser_init(Parser *parser, String content)
{
	lexer_init(&parser->lexer, content);

	parser->error.first.message = "";
	parser->error.first.next    = nullptr;

	parser->error.last          = &parser->error.first;
	parser->parsing             = true;

	if (!ParseTableInitialize)
	{
		parser_init_precedence();
		ParseTableInitialize = true;
	}

	lexer_next(&parser->lexer);
}

//
//
//
