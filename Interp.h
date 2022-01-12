#pragma once
#include "Printer.h"
#include "Token.h"

struct Interp
{
	uint8_t *stack;
	uint8_t *global;
	int64_t  return_count = 0;
};

struct Find_Type_Value
{
	struct Kano_Array
	{
		Kano_Int length;
		uint8_t *data;
	};

	union {
		Kano_Int   int_value;
		Kano_Real  real_value;
		Kano_Bool  bool_value;
		uint8_t *  pointer_value;
		Kano_Array array_value;

	} imm;

	uint8_t *  address = nullptr;
	Code_Type *type    = nullptr;
};

void            interp_init(Interp *interp, size_t stack_size, size_t bss_size);
Find_Type_Value evaluate_code_node_assignment(Code_Node_Assignment *node, Interp *interp, uint64_t top);
void            evaluate_node_block(Code_Node_Block *root, Interp *stack, uint64_t top, bool isproc);

void interp_eval_globals(Interp *interp, Array_View<Code_Node_Assignment *> exprs);
void interp_eval_procedure(Interp *interp, Code_Node_Block *proc);
