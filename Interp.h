#pragma once
#include "Printer.h"
#include "Token.h"

struct Interp
{
	uint8_t *stack;
	uint8_t *global;
	int64_t  return_count = 0;
};

void            interp_init(Interp *interp, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interp *interp, Array_View<Code_Node_Assignment *> exprs);
void interp_eval_procedure(Interp *interp, Code_Node_Block *proc);
