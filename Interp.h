#pragma once
#include "Printer.h"
#include "Token.h"

enum Intercept_Kind
{
	INTERCEPT_STATEMENT,
	INTERCEPT_PROCEDURE_CALL,
	INTERCEPT_PROCEDURE_RETURN
};

typedef void(*Intercep_Proc)(struct Interpreter *interp, Intercept_Kind intercept, struct Code_Node *node);

inline void intercept_default(struct Interpreter *interp, Intercept_Kind intercept, struct Code_Node *statement){};

struct Interpreter
{
	uint8_t *stack = nullptr;
	uint64_t stack_size = 0;
	uint8_t *global = nullptr;
	uint64_t global_size = 0;
	uint64_t stack_top = 0;
	int64_t  return_count = 0;
	struct Code_Type_Procedure *current_procedure = nullptr;
	Symbol_Table *global_symbol_table = nullptr;
	struct Heap_Allocator *heap = nullptr;

	struct Code_Type_Resolver *resolver = nullptr;

	Intercep_Proc intercept = intercept_default;
	void *user_context = nullptr;
};

void            interp_init(Interpreter *interp, struct Code_Type_Resolver *resolver, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interpreter *interp, Array_View<Code_Node_Assignment *> exprs);
Code_Node_Procedure_Call *interp_find_main(Interpreter *interp);
void interp_evaluate_procedure(Interpreter *interp, Code_Node_Procedure_Call *proc);
