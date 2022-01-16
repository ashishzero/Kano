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
	uint8_t *global = nullptr;
	uint64_t stack_top = 0;
	int64_t  return_count = 0;
	struct Code_Type_Procedure *current_procedure = nullptr;
	Intercep_Proc intercept = intercept_default;

	void *user_context = nullptr;
};

void            interp_init(Interpreter *interp, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interpreter *interp, Array_View<Code_Node_Assignment *> exprs);
int interp_eval_main(Interpreter *interp, struct Code_Type_Resolver *resolver);
