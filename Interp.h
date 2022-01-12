#pragma once
#include "Printer.h"
#include "Token.h"

struct Interp
{
	uint8_t *stack = nullptr;
	uint8_t *global = nullptr;
	uint64_t stack_top = 0;
	int64_t  return_count = 0;
};

void            interp_init(Interp *interp, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interp *interp, Array_View<Code_Node_Assignment *> exprs);
void interp_eval_procedure_call(Interp *interp, Code_Node_Block *proc);
