#pragma once
#include "SyntaxNode.h"

enum Code_Node_Kind {
	CODE_NODE_NULL,
	CODE_NODE_LITERAL,
	CODE_NODE_UNARY_OPERATOR,
	CODE_NODE_BINARY_OPERATOR,
	CODE_NODE_EXPRESSION,

	_CODE_NODE_COUNT,
};

enum Code_Type_Kind {
	CODE_TYPE_NULL,
	CODE_TYPE_REAL,

	_CODE_TYPE_COUNT
};

struct Code_Type {
	Code_Type_Kind kind = CODE_TYPE_NULL;
};

struct Code_Node {
	Code_Node_Kind  kind = CODE_NODE_NULL;
	Code_Type       type;
	Syntax_Location location;
};

struct Code_Node_Literal : public Code_Node {
	Code_Node_Literal() { kind = CODE_NODE_LITERAL; }

	double value = 0;
};

enum Unary_Operator_Kind {
	UNARY_OPERATOR_PLUS,
	UNARY_OPERATOR_MINUS,

	_UNARY_OPERATOR_COUNT
};

struct Unary_Operator {
	Code_Type parameter;
	Code_Type output;
};

struct Code_Node_Unary_Operator : public Code_Node {
	Code_Node_Unary_Operator() { kind = CODE_NODE_UNARY_OPERATOR; }

	Unary_Operator_Kind op_kind;
	Unary_Operator *op = nullptr;

	Code_Node *child = nullptr;
};

enum Binary_Operator_Kind {
	BINARY_OPERATOR_ADD,
	BINARY_OPERATOR_SUB,
	BINARY_OPERATOR_MUL,
	BINARY_OPERATOR_DIV,

	_BINARY_OPERATOR_COUNT
};

struct Binary_Operator {
	Code_Type parameters[2];
	Code_Type output;
};

struct Code_Node_Binary_Operator : public Code_Node {
	Code_Node_Binary_Operator() { kind = CODE_NODE_BINARY_OPERATOR; }

	Binary_Operator_Kind op_kind;
	Binary_Operator *op = nullptr;

	Code_Node *left = nullptr;
	Code_Node *right = nullptr;
};

struct Code_Node_Expression : public Code_Node {
	Code_Node_Expression() { kind = CODE_NODE_EXPRESSION; }

	Code_Node *child = nullptr;
};
