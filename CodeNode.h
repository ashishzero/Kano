#pragma once
#include "SyntaxNode.h"

enum Code_Node_Kind {
	CODE_NODE_NULL,
	CODE_NODE_LITERAL,
	CODE_NODE_STACK,
	CODE_NODE_DESTINATION,
	CODE_NODE_UNARY_OPERATOR,
	CODE_NODE_BINARY_OPERATOR,
	CODE_NODE_EXPRESSION,
	CODE_NODE_ASSIGNMENT,
	CODE_NODE_STATEMENT,
	CODE_NODE_BLOCK,

	_CODE_NODE_COUNT,
};

enum Code_Type_Kind {
	CODE_TYPE_NULL,
	CODE_TYPE_INTEGER,
	CODE_TYPE_REAL,
	CODE_TYPE_BOOL,
	CODE_TYPE_POINTER,
//	CODE_TYPE_MEMORY_ADDRESS,

	_CODE_TYPE_COUNT
};

struct Code_Type {
	Code_Type_Kind kind = CODE_TYPE_NULL;
	Code_Type *next     = nullptr;
};

struct Code_Value_Integer {
	int32_t value;
};

struct Code_Value_Real {
	float value;
};

struct Code_Value_Bool {
	bool value;
};

union Code_Value {
	Code_Value_Integer integer;
	Code_Value_Real    real;
	Code_Value_Bool    boolean;

	Code_Value() = default;
};

struct Code_Node {
	Code_Node_Kind  kind = CODE_NODE_NULL;
	Code_Type       *type = nullptr;
};

struct Code_Node_Literal : public Code_Node {
	Code_Node_Literal() { kind = CODE_NODE_LITERAL; }

	Code_Value data;
};

struct Code_Node_Stack : public Code_Node {
	Code_Node_Stack() { kind = CODE_NODE_STACK; }

	uint32_t offset = UINT32_MAX;
};

struct Code_Node_Destination : public Code_Node {
	Code_Node_Destination() { kind = CODE_NODE_DESTINATION; }

	Code_Node *child = nullptr;
};

enum Unary_Operator_Kind {
	UNARY_OPERATOR_PLUS,
	UNARY_OPERATOR_MINUS,
	UNARY_OPERATOR_BITWISE_NOT,
	UNARY_OPERATOR_LOGICAL_NOT,
	UNARY_OPERATOR_ADDRESS_OF,

	_UNARY_OPERATOR_COUNT
};

struct Unary_Operator {
	Code_Type parameter;
	Code_Type output;
};

struct Code_Node_Unary_Operator : public Code_Node {
	Code_Node_Unary_Operator() { kind = CODE_NODE_UNARY_OPERATOR; }

	Unary_Operator_Kind op_kind;
	// Unary_Operator *op = nullptr; // FIXME: DO WE NEED THIS?

	Code_Node *child = nullptr;
};

enum Binary_Operator_Kind {
	BINARY_OPERATOR_ADDITION,
	BINARY_OPERATOR_SUBTRACTION,
	BINARY_OPERATOR_MULTIPLICATION,
	BINARY_OPERATOR_DIVISION,
	BINARY_OPERATOR_REMAINDER,
	BINARY_OPERATOR_BITWISE_SHIFT_RIGHT,
	BINARY_OPERATOR_BITWISE_SHIFT_LEFT,
	BINARY_OPERATOR_BITWISE_AND,
	BINARY_OPERATOR_BITWISE_XOR,
	BINARY_OPERATOR_BITWISE_OR,
	BINARY_OPERATOR_RELATIONAL_GREATER,
	BINARY_OPERATOR_RELATIONAL_LESS,
	BINARY_OPERATOR_RELATIONAL_GREATER_EQUAL,
	BINARY_OPERATOR_RELATIONAL_LESS_EQUAL,
	BINARY_OPERATOR_COMPARE_EQUAL,
	BINARY_OPERATOR_COMPARE_NOT_EQUAL,

	_BINARY_OPERATOR_COUNT
};

struct Binary_Operator {
	Code_Type parameters[2];
	Code_Type output;
};

struct Code_Node_Binary_Operator : public Code_Node {
	Code_Node_Binary_Operator() { kind = CODE_NODE_BINARY_OPERATOR; }

	Binary_Operator_Kind op_kind;

	Code_Node *left = nullptr;
	Code_Node *right = nullptr;
};

struct Code_Node_Expression : public Code_Node {
	Code_Node_Expression() { kind = CODE_NODE_EXPRESSION; }

	Code_Node *child = nullptr;
};

struct Assignment {
	Code_Type destination;
	Code_Type value;
};

struct Code_Node_Assignment : public Code_Node {
	Code_Node_Assignment() { kind = CODE_NODE_ASSIGNMENT; }

	Code_Node_Destination *destination = nullptr;
	Code_Node_Expression *value = nullptr;
};

struct Code_Node_Statement : public Code_Node {
	Code_Node_Statement() { kind = CODE_NODE_STATEMENT; }

	Code_Node *node           = nullptr;
	Code_Node_Statement *next = nullptr;
};

struct Code_Node_Block : public Code_Node {
	Code_Node_Block() { kind = CODE_NODE_BLOCK; }

	Code_Node_Statement *statement_head = nullptr;
	uint64_t statement_count            = 0;
};
