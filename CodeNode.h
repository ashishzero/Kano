#pragma once
#include "SyntaxNode.h"

enum Code_Node_Kind {
	CODE_NODE_NULL,
	CODE_NODE_LITERAL,
	CODE_NODE_ADDRESS,
	CODE_NODE_TYPE_CAST,
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

	_CODE_TYPE_COUNT
};

struct Code_Type {
	Code_Type_Kind kind = CODE_TYPE_NULL;
	uint32_t runtime_size = 0;
};

struct Code_Type_Integer : public Code_Type {
	Code_Type_Integer() { kind = CODE_TYPE_INTEGER; runtime_size = sizeof(int32_t); }
};

struct Code_Type_Real : public Code_Type {
	Code_Type_Real() { kind = CODE_TYPE_REAL; runtime_size = sizeof(float); }
};

struct Code_Type_Bool : public Code_Type {
	Code_Type_Bool() { kind = CODE_TYPE_BOOL; runtime_size = sizeof(bool); }
};

struct Code_Type_Pointer : public Code_Type {
	Code_Type_Pointer() { kind = CODE_TYPE_POINTER; runtime_size = sizeof(void *); }

	Code_Type *base_type = nullptr;
};

struct Symbol {
	String          name;
	Code_Type *     type     = nullptr;
	uint32_t        flags    = 0;
	uint32_t        address  = UINT32_MAX;
	Syntax_Location location = {};
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
	uint32_t        flags = 0;
	Code_Type       *type = nullptr;
};

struct Code_Node_Literal : public Code_Node {
	Code_Node_Literal() { kind = CODE_NODE_LITERAL; }

	Code_Value data;
};

struct Code_Node_Address : public Code_Node {
	Code_Node_Address() { kind = CODE_NODE_ADDRESS; }

	uint32_t offset = UINT32_MAX;
};

struct Code_Node_Type_Cast : public Code_Node {
	Code_Node_Type_Cast() { kind = CODE_NODE_TYPE_CAST; }

	Code_Node *child          = nullptr;
};

enum Unary_Operator_Kind {
	UNARY_OPERATOR_PLUS,
	UNARY_OPERATOR_MINUS,
	UNARY_OPERATOR_BITWISE_NOT,
	UNARY_OPERATOR_LOGICAL_NOT,
	UNARY_OPERATOR_POINTER_TO,
	UNARY_OPERATOR_DEREFERENCE,

	_UNARY_OPERATOR_COUNT
};

struct Unary_Operator {
	Code_Type *parameter;
	Code_Type *output;
};

struct Code_Node_Unary_Operator : public Code_Node {
	Code_Node_Unary_Operator() { kind = CODE_NODE_UNARY_OPERATOR; }

	Unary_Operator_Kind op_kind;

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
	Code_Type *parameters[2];
	Code_Type *output;
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

struct Code_Node_Assignment : public Code_Node {
	Code_Node_Assignment() { kind = CODE_NODE_ASSIGNMENT; }

	Code_Node_Expression *destination = nullptr;
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
