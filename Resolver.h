#pragma once
#include "Common.h"
#include "CodeNode.h"

struct Code_Type_Resolver;

Code_Type_Resolver *code_type_resolver_create();
uint64_t code_type_resolver_stack_allocated(Code_Type_Resolver *resolver);
uint64_t code_type_resolver_bss_allocated(Code_Type_Resolver *resolver);

Array_View<Code_Node_Assignment *> code_type_resolve(Code_Type_Resolver *resolver, Syntax_Node_Global_Scope *code_node);

const Symbol *code_type_resolver_find(Code_Type_Resolver *resolver, String name);
