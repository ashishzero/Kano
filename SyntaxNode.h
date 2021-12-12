#pragma once
#include "Common.h"
#include "Token.h"
#include "Flags.h"

struct Syntax_Location {
	size_t start_row = 0;
	size_t start_column = 0;

	size_t finish_row = 0;
	size_t finish_column = 0;

	size_t start = 0;
	size_t finish = 0;
};

//
//
//

enum Syntax_Node_Kind {
	SYNTAX_NODE_NULL,
	SYNTAX_NODE_LITERAL,
	SYNTAX_NODE_IDENTIFIER,
	SYNTAX_NODE_UNARY_OPERATOR,
	SYNTAX_NODE_BINARY_OPERATOR,
	SYNTAX_NODE_TYPE,
	SYNTAX_NODE_ASSIGNMENT,
	SYNTAX_NODE_EXPRESSION,
	SYNTAX_NODE_DECLARATION,
	SYNTAX_NODE_STATEMENT,
	SYNTAX_NODE_BLOCK,
};

struct Syntax_Node;
struct Syntax_Node_Literal;
struct Syntax_Node_Identifier;
struct Syntax_Node_Unary_Operator;
struct Syntax_Node_Binary_Operator;
struct Syntax_Node_Type;
struct Syntax_Node_Assignment;
struct Syntax_Node_Expression;
struct Syntax_Node_Declaration;
struct Syntax_Node_Statement;
struct Syntax_Node_Block;

struct Syntax_Node {
	Syntax_Node_Kind kind = SYNTAX_NODE_NULL;
	Syntax_Location  location;
};

struct Literal {
	enum Kind {
		INTEGER, REAL, BOOL
	};

	union Value {
		int32_t integer = 0;
		float   real;
		bool    boolean;
	};

	Kind  kind = INTEGER;
	Value data;
};

struct Syntax_Node_Literal : public Syntax_Node {
	Syntax_Node_Literal() { kind = SYNTAX_NODE_LITERAL; }

	Literal value;
};

struct Syntax_Node_Identifier : public Syntax_Node {
	Syntax_Node_Identifier() { kind = SYNTAX_NODE_IDENTIFIER; }

	String name = "";
};

struct Syntax_Node_Unary_Operator : public Syntax_Node {
	Syntax_Node_Unary_Operator() { kind = SYNTAX_NODE_UNARY_OPERATOR; }

	Token_Kind  op;
	Syntax_Node *child = nullptr;
};

struct Syntax_Node_Binary_Operator : public Syntax_Node {
	Syntax_Node_Binary_Operator() { kind = SYNTAX_NODE_BINARY_OPERATOR; }

	Token_Kind  op;
	Syntax_Node *left = nullptr;
	Syntax_Node *right = nullptr;
};

struct Syntax_Node_Type : public Syntax_Node {
	Syntax_Node_Type() { kind = SYNTAX_NODE_TYPE; }

	Token_Kind token_type  = TOKEN_KIND_ERROR;
	Syntax_Node_Type *next = nullptr;
};

struct Syntax_Node_Assignment : public Syntax_Node {
	Syntax_Node_Assignment() { kind = SYNTAX_NODE_ASSIGNMENT; }

	Syntax_Node_Expression *left = nullptr;
	Syntax_Node_Expression *right = nullptr;
};

struct Syntax_Node_Expression : public Syntax_Node {
	Syntax_Node_Expression() { kind = SYNTAX_NODE_EXPRESSION; }

	Syntax_Node *child = nullptr;
};

struct Syntax_Node_Declaration : public Syntax_Node {
	Syntax_Node_Declaration() { kind = SYNTAX_NODE_DECLARATION; }

	uint32_t          flags = 0;
	String            identifier;
	Syntax_Node_Type *type  = nullptr;
};

struct Syntax_Node_Statement : public Syntax_Node {
	Syntax_Node_Statement() { kind = SYNTAX_NODE_STATEMENT; }

	Syntax_Node *node           = nullptr;
	Syntax_Node_Statement *next = nullptr;
};

struct Syntax_Node_Block : public Syntax_Node {
	Syntax_Node_Block() { kind = SYNTAX_NODE_BLOCK; }

	Syntax_Node_Statement *statement_head = nullptr;
	uint64_t statement_count = 0;
};
