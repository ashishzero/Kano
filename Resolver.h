#pragma once
#include "Common.h"
#include "CodeNode.h"

struct Code_Type_Resolver;

Code_Type_Resolver *code_type_resolver_create();
uint64_t code_type_resolver_stack_allocated(Code_Type_Resolver *resolver);
uint64_t code_type_resolver_bss_allocated(Code_Type_Resolver *resolver);

Array_View<Code_Node_Assignment *> code_type_resolve(Code_Type_Resolver *resolver, Syntax_Node_Global_Scope *code_node);

const Symbol *code_type_resolver_find(Code_Type_Resolver *resolver, String name);
Code_Type *code_type_resolver_find_type(Code_Type_Resolver *resolver, String name);

bool code_type_resolver_register_ccall(Code_Type_Resolver *resolver, String name, CCall proc, Code_Type_Procedure *type);

//
//
//

struct Procedure_Builder
{
	Code_Type_Resolver *resolver = nullptr;
	Array<Code_Type *> arguments;
	Code_Type *return_type = nullptr;
	bool is_variadic = false;

	Procedure_Builder(Code_Type_Resolver *type_resolver = nullptr)
	{
		resolver = type_resolver;
	}
};

void proc_builder_argument(Procedure_Builder *builder, String name);
void proc_builder_variadic(Procedure_Builder *builder);
void proc_builder_return(Procedure_Builder *builder, String name);
void proc_builder_register(Procedure_Builder *builder, String name, CCall ccall);
void proc_builder_free(Procedure_Builder *builder);
