#pragma once
#include "Printer.h"
#include "Token.h"

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
