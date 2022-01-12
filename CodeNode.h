#pragma once
#include "SyntaxNode.h"

using Kano_Int  = int64_t;
using Kano_Real = double;
using Kano_Bool = bool;

enum Code_Type_Kind
{
	CODE_TYPE_NULL,
	CODE_TYPE_INTEGER,
	CODE_TYPE_REAL,
	CODE_TYPE_BOOL,
	CODE_TYPE_POINTER,
	CODE_TYPE_PROCEDURE,
	CODE_TYPE_STRUCT,
	CODE_TYPE_ARRAY_VIEW,
	CODE_TYPE_STATIC_ARRAY,

	_CODE_TYPE_COUNT
};

struct Code_Type
{
	Code_Type_Kind kind         = CODE_TYPE_NULL;
	uint32_t       runtime_size = 0;
	uint32_t       alignment    = 0;
};

struct Code_Type_Integer : public Code_Type
{
	Code_Type_Integer()
	{
		kind         = CODE_TYPE_INTEGER;
		runtime_size = sizeof(Kano_Int);
		alignment    = sizeof(Kano_Int);
	}
};

struct Code_Type_Real : public Code_Type
{
	Code_Type_Real()
	{
		kind         = CODE_TYPE_REAL;
		runtime_size = sizeof(Kano_Real);
		alignment    = sizeof(Kano_Real);
	}
};

struct Code_Type_Bool : public Code_Type
{
	Code_Type_Bool()
	{
		kind         = CODE_TYPE_BOOL;
		runtime_size = sizeof(Kano_Bool);
		alignment    = sizeof(Kano_Bool);
	}
};

struct Code_Type_Pointer : public Code_Type
{
	Code_Type_Pointer()
	{
		kind         = CODE_TYPE_POINTER;
		runtime_size = sizeof(void *);
		alignment    = sizeof(void *);
	}

	Code_Type *base_type = nullptr;
};

struct Code_Type_Procedure : public Code_Type
{
	Code_Type_Procedure()
	{
		kind         = CODE_TYPE_PROCEDURE;
		runtime_size = sizeof(void *);
		alignment    = sizeof(void *);
	}

	Code_Type **arguments      = nullptr;
	uint64_t    argument_count = 0;
	bool        is_variadic    = false;

	Code_Type * return_type    = nullptr;
};

struct Code_Type_Struct : public Code_Type
{
	Code_Type_Struct()
	{
		kind = CODE_TYPE_STRUCT;
	}

	struct Member
	{
		String     name;
		Code_Type *type;
		uint64_t   offset;
	};

	String   name;
	uint64_t member_count;
	Member * members;

	uint64_t id;
};

struct Code_Type_Array_View : public Code_Type
{
	Code_Type_Array_View()
	{
		kind         = CODE_TYPE_ARRAY_VIEW;
		runtime_size = sizeof(Array_View<void *>);
		alignment    = sizeof(int64_t);
	}

	Code_Type *element_type = nullptr;
};

struct Code_Type_Static_Array : public Code_Type
{
	Code_Type_Static_Array()
	{
		kind = CODE_TYPE_STATIC_ARRAY;
	}

	Code_Type *element_type  = nullptr;
	uint32_t   element_count = 0;
};

//
//
//
typedef void (*CCall)(struct Interpreter *interp);
struct Symbol_Address
{
	enum Kind
	{
		STACK  = 0,
		GLOBAL = 1,
		CODE,
		CCALL,
	};
	Kind kind;
	union {
		uint64_t                offset;
		struct Code_Node_Block *code;
		CCall                   ccall;
	};
};

inline Symbol_Address symbol_address_offset(uint32_t offset, Symbol_Address::Kind kind)
{
	Symbol_Address address;
	address.kind   = kind;
	address.offset = offset;
	return address;
}

inline Symbol_Address symbol_address_code(struct Code_Node_Block *block)
{
	Symbol_Address code;
	code.kind = Symbol_Address::CODE;
	code.code = block;
	return code;
}

struct Symbol
{
	String          name;
	Code_Type *     type     = nullptr;
	Symbol_Address  address  = {Symbol_Address::CODE, 0};
	uint32_t        flags    = 0;
	Syntax_Location location = {};
};

constexpr uint32_t SYMBOL_INDEX_BUCKET_SIZE  = 16;
constexpr uint32_t SYMBOL_INDEX_MASK         = SYMBOL_INDEX_BUCKET_SIZE - 1;
constexpr uint32_t SYMBOL_INDEX_SHIFT        = 4;
constexpr uint32_t SYMBOL_TABLE_BUCKET_COUNT = 64;
constexpr uint32_t HASH_SEED                 = 0x2564;

struct Symbol_Index
{
	uint32_t      hash[SYMBOL_INDEX_BUCKET_SIZE]  = {};
	uint32_t      index[SYMBOL_INDEX_BUCKET_SIZE] = {};
	Symbol_Index *next                            = nullptr;
};

struct Symbol_Lookup
{
	Symbol_Index buckets[SYMBOL_TABLE_BUCKET_COUNT];
};

struct Symbol_Table
{
	Symbol_Lookup   lookup;
	Array<Symbol *> buffer;
	Symbol_Table *  parent = nullptr;
};

//
//
//

enum Code_Node_Kind
{
	CODE_NODE_NULL,
	CODE_NODE_LITERAL,
	CODE_NODE_ADDRESS,
	CODE_NODE_TYPE_CAST,
	CODE_NODE_UNARY_OPERATOR,
	CODE_NODE_BINARY_OPERATOR,
	CODE_NODE_EXPRESSION,
	CODE_NODE_ASSIGNMENT,
	CODE_NODE_RETURN,
	CODE_NODE_STATEMENT,
	CODE_NODE_PROCEDURE_CALL,
	CODE_NODE_SUBSCRIPT,
	CODE_NODE_IF,
	CODE_NODE_FOR,
	CODE_NODE_WHILE,
	CODE_NODE_DO,
	CODE_NODE_BLOCK,

	_CODE_NODE_COUNT,
};

struct Code_Node
{
	Code_Node_Kind kind  = CODE_NODE_NULL;
	uint32_t       flags = 0;
	Code_Type *    type  = nullptr;
};

struct Code_Value_Integer
{
	Kano_Int value;
};

struct Code_Value_Real
{
	Kano_Real value;
};

struct Code_Value_Bool
{
	Kano_Bool value;
};

struct Code_Value_Pointer
{
	uint8_t *value;
};

struct Code_Value_String
{
	String value;
};

union Code_Value {
	Code_Value_String  string = {};
	Code_Value_Integer integer;
	Code_Value_Real    real;
	Code_Value_Bool    boolean;
	Code_Value_Pointer pointer;

	Code_Value(){};
};

struct Code_Node_Literal : public Code_Node
{
	Code_Node_Literal()
	{
		kind = CODE_NODE_LITERAL;
	}

	Code_Value data;
};

struct Code_Node_Address : public Code_Node
{
	Code_Node_Address()
	{
		kind = CODE_NODE_ADDRESS;
	}

	Code_Node *           child   = nullptr; // If child is null, then address is valid

	const Symbol_Address *address = nullptr;

	uint64_t              offset  = 0;
};

struct Code_Node_Type_Cast : public Code_Node
{
	Code_Node_Type_Cast()
	{
		kind = CODE_NODE_TYPE_CAST;
	}

	struct Code_Node_Expression *child    = nullptr;
	bool                         implicit = false;
};

enum Unary_Operator_Kind
{
	UNARY_OPERATOR_PLUS,
	UNARY_OPERATOR_MINUS,
	UNARY_OPERATOR_BITWISE_NOT,
	UNARY_OPERATOR_LOGICAL_NOT,
	UNARY_OPERATOR_POINTER_TO,
	UNARY_OPERATOR_DEREFERENCE,

	_UNARY_OPERATOR_COUNT
};

struct Unary_Operator
{
	Code_Type *parameter;
	Code_Type *output;
};

struct Code_Node_Unary_Operator : public Code_Node
{
	Code_Node_Unary_Operator()
	{
		kind = CODE_NODE_UNARY_OPERATOR;
	}

	Unary_Operator_Kind op_kind;

	Code_Node *         child = nullptr;
};

enum Binary_Operator_Kind
{
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
	BINARY_OPERATOR_COMPOUND_ADDITION,
	BINARY_OPERATOR_COMPOUND_SUBTRACTION,
	BINARY_OPERATOR_COMPOUND_MULTIPLICATION,
	BINARY_OPERATOR_COMPOUND_DIVISION,
	BINARY_OPERATOR_COMPOUND_REMAINDER,
	BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_RIGHT,
	BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_LEFT,
	BINARY_OPERATOR_COMPOUND_BITWISE_AND,
	BINARY_OPERATOR_COMPOUND_BITWISE_XOR,
	BINARY_OPERATOR_COMPOUND_BITWISE_OR,

	_BINARY_OPERATOR_COUNT
};

struct Binary_Operator
{
	Code_Type *parameters[2];
	Code_Type *output;
	bool       compound = false;
};

struct Code_Node_Binary_Operator : public Code_Node
{
	Code_Node_Binary_Operator()
	{
		kind = CODE_NODE_BINARY_OPERATOR;
	}

	Binary_Operator_Kind op_kind;

	Code_Node *          left  = nullptr;
	Code_Node *          right = nullptr;
};

struct Code_Node_Expression : public Code_Node
{
	Code_Node_Expression()
	{
		kind = CODE_NODE_EXPRESSION;
	}

	Code_Node *child = nullptr;
};

struct Code_Node_Assignment : public Code_Node
{
	Code_Node_Assignment()
	{
		kind = CODE_NODE_ASSIGNMENT;
	}

	Code_Node_Expression *destination = nullptr;
	Code_Node_Expression *value       = nullptr;
};

struct Code_Node_Return : public Code_Node
{
	Code_Node_Return()
	{
		kind = CODE_NODE_RETURN;
	}

	Code_Node *expression = nullptr;
};

struct Code_Node_Statement : public Code_Node
{
	Code_Node_Statement()
	{
		kind = CODE_NODE_STATEMENT;
	}

	uint64_t             source_row = -1;

	Code_Node *          node       = nullptr;
	Code_Node_Statement *next       = nullptr;

	Symbol_Table *symbol_table = nullptr;
};

struct Code_Node_Procedure_Call : public Code_Node
{
	Code_Node_Procedure_Call()
	{
		kind = CODE_NODE_PROCEDURE_CALL;
	}

	Code_Node_Expression * procedure       = nullptr;
	uint64_t               parameter_count = 0;
	Code_Node_Expression **paraments       = nullptr;
	uint64_t               variadic_count  = 0;
	Code_Node_Expression **variadics       = nullptr;

	uint64_t               stack_top       = 0;
};

struct Code_Node_Subscript : public Code_Node
{
	Code_Node_Subscript()
	{
		kind = CODE_NODE_SUBSCRIPT;
	}

	Code_Node_Expression *expression = nullptr;
	Code_Node_Expression *subscript  = nullptr;
};

struct Code_Node_If : public Code_Node
{
	Code_Node_If()
	{
		kind = CODE_NODE_IF;
	}

	Code_Node_Expression *condition       = nullptr;
	Code_Node_Statement * true_statement  = nullptr;
	Code_Node_Statement * false_statement = nullptr;
};

struct Code_Node_For : public Code_Node
{
	Code_Node_For()
	{
		kind = CODE_NODE_FOR;
	}

	Code_Node_Statement *initialization = nullptr;
	Code_Node_Statement *condition      = nullptr;
	Code_Node_Statement *increment      = nullptr;

	Code_Node_Statement * body           = nullptr;

	Symbol_Table          symbols;
};

struct Code_Node_While : public Code_Node
{
	Code_Node_While()
	{
		kind = CODE_NODE_WHILE;
	}

	Code_Node_Statement *condition = nullptr;

	Code_Node_Statement *body      = nullptr;
};

struct Code_Node_Do : public Code_Node
{
	Code_Node_Do()
	{
		kind = CODE_NODE_DO;
	}

	Code_Node_Statement *body      = nullptr;

	Code_Node_Statement *condition = nullptr;
};

struct Code_Node_Block : public Code_Node
{
	Code_Node_Block()
	{
		kind = CODE_NODE_BLOCK;
	}

	Code_Node_Statement *statement_head  = nullptr;
	uint64_t             statement_count = 0;

	Symbol_Table         symbols;
};
