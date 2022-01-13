#pragma once
#include "Printer.h"
#include "Token.h"

#define InterpProcStart(interp) (interp->stack + interp->stack_top) 
#define InterpProcNext(arg, type) *(type *)arg; arg += sizeof(type)

typedef void(*Intercep_Proc)(struct Interpreter *interp, struct Code_Node_Statement *statement);

inline void intercept_default(struct Interpreter *interp, struct Code_Node_Statement *statement){};

struct Interpreter
{
	uint8_t *stack = nullptr;
	uint8_t *global = nullptr;
	uint64_t stack_top = 0;
	int64_t  return_count = 0;
	Intercep_Proc intercept = intercept_default;
};

void            interp_init(Interpreter *interp, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interpreter *interp, Array_View<Code_Node_Assignment *> exprs);
void interp_eval_procedure_call(Interpreter *interp, Code_Node_Block *proc);
