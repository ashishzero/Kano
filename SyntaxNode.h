#pragma once
#include "Common.h"
#include "Token.h"

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
	SYNTAX_NODE_UNARY_OPERATOR,
	SYNTAX_NODE_BINARY_OPERATOR,
	SYNTAX_NODE_EXPRESSION,
};

struct Syntax_Node {
	Syntax_Node_Kind kind = SYNTAX_NODE_NULL;
	Syntax_Location  location;
};

struct Syntax_Node_Literal : public Syntax_Node {
	Syntax_Node_Literal() { kind = SYNTAX_NODE_LITERAL; }
	double value = 0;
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

struct Syntax_Node_Expression : public Syntax_Node {
	Syntax_Node_Expression() { kind = SYNTAX_NODE_EXPRESSION; }

	Syntax_Node *child = nullptr;
};
