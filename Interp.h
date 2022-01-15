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
	//uint64_t return_stack = 0;
};

void            interp_init(Interpreter *interp, size_t stack_size, size_t bss_size);

void interp_eval_globals(Interpreter *interp, Array_View<Code_Node_Assignment *> exprs);
int interp_eval_main(Interpreter *interp, struct Code_Type_Resolver *resolver);

//
//
//

struct Interp_Proc_Arg
{
	uint8_t *arg;

	Interp_Proc_Arg(Interpreter *interp) 
	{ 
		arg = interp->stack + interp->stack_top; 
	}

	template <typename T> T deserialize_arg()
	{
		arg -= sizeof(T);
		T value = *(T *)arg; 
		return value;
	}

	template <typename ReturnType, typename... ArgumentTypes>
	void execute(ReturnType(*proc)(ArgumentTypes...))
	{
		auto ptr = arg;
		arg += sizeof(ReturnType) + (sizeof(ArgumentTypes)+...);
		ReturnType result = proc(deserialize_arg<ArgumentTypes>()...);
		memcpy(ptr, &result, sizeof(ReturnType));
	}

	template <typename... ArgumentTypes>
	void execute(void(*proc)(ArgumentTypes...))
	{
		arg += (sizeof(ArgumentTypes)+...);
		proc(deserialize_arg<ArgumentTypes>()...);
	}
};

#define InterpMorphProc(proc) \
	[](Interpreter *interp) { \
		Interp_Proc_Arg arg(interp); \
		arg.execute(proc); \
	}
