#include "Kr/KrBasic.h"

#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"
#include "HeapAllocator.h"
#include "Kr/KrString.h"

#include "StringBuilder.h"
#include "StdLib.h"

//
//
//

void AssertHandle(const char *reason, const char *file, int line, const char *proc)
{
	fprintf(stderr, "Internal Compiler Error %s. File: %s(%d)\n", reason, file, line);
	DebugTriggerbreakpoint();
}

//
//
//

static inline Call_Info make_procedure_call(String name, uint64_t stack_top, Symbol_Table *table)
{
	Call_Info call;
	call.procedure_name = name;
	call.stack_top = stack_top;
	call.symbols = table;
	return call;
}

struct Call_Stack {
	String procedure_name;
	uint64_t stack_top;
	Symbol_Table *symbols;
};

static void json_write_symbol(Json_Writer *json, Interpreter *interp, String name, Code_Type *type, void *data);

static void json_write_type_name(Json_Writer *json, Code_Type *type)
{
	if (!type)
	{
		json->append_string_value("void");
		return;
	}

	switch (type->kind)
	{
		case CODE_TYPE_NULL: json->append_string_value("void"); return;
		case CODE_TYPE_CHARACTER: json->append_string_value("byte"); return;
		case CODE_TYPE_INTEGER: json->append_string_value("int"); return;
		case CODE_TYPE_REAL: json->append_string_value("float"); return;
		case CODE_TYPE_BOOL: json->append_string_value("bool"); return;

		case CODE_TYPE_POINTER: {
			json->append_string_value("*"); 
			json_write_type_name(json, ((Code_Type_Pointer *)type)->base_type);
			return;
		}

		case CODE_TYPE_PROCEDURE: {
			auto proc = (Code_Type_Procedure *)type;
			json->append_string_value("proc(");
			for (int64_t index = 0; index < proc->argument_count; ++index)
			{
				json_write_type_name(json, proc->arguments[index]);
				if (index < proc->argument_count - 1) json->append_string_value(", ");
			}
			json->append_string_value(")");

			if (proc->return_type)
			{
				json->append_string_value("->");
				json_write_type_name(json, proc->return_type);
			}
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto strt = (Code_Type_Struct *)type;
			json->append_string_value("%", strt->name); 
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr = (Code_Type_Array_View *)type;
			json->append_string_value("[]");
			json_write_type_name(json, arr->element_type);
			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr = (Code_Type_Static_Array *)type;
			json->append_string_value("[%]", arr->element_count);
			json_write_type_name(json, arr->element_type);
			return;
		}
	}
}

static void json_write_type(Json_Writer *json, Code_Type *type)
{
	json->begin_object();
	json->write_key("name");
	json->begin_string_value();
	json_write_type_name(json, type);
	json->end_string_value();

	if (type->kind == CODE_TYPE_POINTER)
	{
		json->write_key("pointer");
		auto base_type = ((Code_Type_Pointer *)type)->base_type;
		json_write_type(json, base_type);
	}
	else
	{
		json->write_key_value("pointer", "null");
	}

	if (type->kind == CODE_TYPE_ARRAY_VIEW)
	{
		auto arr = (Code_Type_Array_View *)type;
		json->write_key("array_view");
		json_write_type(json, arr->element_type);
	}
	else
	{
		json->write_key_value("array_view", "null");
	}

	if (type->kind == CODE_TYPE_STATIC_ARRAY)
	{
		auto arr = (Code_Type_Static_Array *)type;
		json->write_key("static_array");
		json->begin_object();
		json->write_key_value("count", arr->element_count);
		json->write_key("element");
		json_write_type(json, arr->element_type);
		json->end_object();
	}
	else
	{
		json->write_key_value("static_array", "null");
	}

	json->end_object();
}

static void json_write_value(Json_Writer *json, Interpreter *interp, Code_Type *type, void *data)
{
	if (!data)
	{
		json->write_single_value("(null)");
		return;
	}

	switch (type->kind)
	{
		case CODE_TYPE_NULL: json->write_single_value("(null)"); return;
		case CODE_TYPE_CHARACTER: json->write_single_value("%", (int) *(Kano_Char *)data); return;
		case CODE_TYPE_INTEGER: json->write_single_value("%", *(Kano_Int *)data); return;
		case CODE_TYPE_REAL: json->write_single_value("%", *(Kano_Real *)data); return;
		case CODE_TYPE_BOOL: json->write_single_value("%", (*(Kano_Bool *)data) ? "true" : "false"); return;
		case CODE_TYPE_PROCEDURE: json->write_single_value("%", data); return;

		case CODE_TYPE_POINTER: {
			auto pointer_type = (Code_Type_Pointer *)type;
			void *raw_ptr = *(void **)data;

			json->begin_object();

			if (raw_ptr)
				json->write_key_value_formatted("raw", "0x%", raw_ptr);
			else
				json->write_key_value_formatted("raw", "%", "null");

			json->write_key("base_type");
			json->begin_string_value();
			json_write_type_name(json, pointer_type->base_type);
			json->end_string_value();

			auto mem_type = interp_get_memory_type(interp, raw_ptr);

			json->write_key_value_formatted("memory", "%", memory_type_string(mem_type));

			json->write_key("value");
			if (mem_type != Memory_Type_INVALID)
			{
				json_write_value(json, interp, pointer_type->base_type, raw_ptr);
			}
			else
			{
				json->write_single_value("%", raw_ptr ? String("(garbage)") : String("(invalid)"));
			}

			json->end_object();
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;

			json->begin_array();

			for (int64_t index = 0; index < _struct->member_count; ++index)
			{
				auto member = &_struct->members[index];

				json_write_symbol(json, interp, member->name, member->type, (uint8_t *)data + member->offset);
			}

			json->end_array();
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr_type = (Code_Type_Array_View *)type;
			
			Kano_Int *ptr = (Kano_Int *)data;

			auto arr_count = ptr[0];
			auto arr_data  = reinterpret_cast<uint8_t *>(*(size_t *)(ptr + 1));

			json->begin_array();
			for (int64_t index = 0; index < arr_count; ++index)
			{
				json_write_value(json, interp, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
			}
			json->end_array();

			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr_type = (Code_Type_Static_Array *)type;

			auto arr_data = (uint8_t *)data;

			json->begin_array();
			for (int64_t index = 0; index < arr_type->element_count; ++index)
			{
				json_write_value(json, interp, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
			}
			json->end_array();

			return;
		}
	}
}

static void json_write_symbol(Json_Writer *json, Interpreter *interp, String name, Code_Type *type, void *data)
{
	json->begin_object();
	json->write_key_value_formatted("name", "%", name);
	
	json->write_key("type");
	json->begin_string_value();
	json_write_type_name(json, type);
	json->end_string_value();
	
	auto mem_type = interp_get_memory_type(interp, data);
	json->write_key_value_formatted("address", "0x%", data);
	json->write_key_value_formatted("memory", "%", memory_type_string(mem_type));
	
	json->write_key("value");
	json_write_value(json, interp, type, data);
	
	json->end_object();
}

static void json_write_symbols(Interpreter *interp, Json_Writer *json, Symbol_Table *symbols, uint64_t stack_top, uint64_t skip_stack_offset)
{
	for (auto &pair : symbols->map)
	{
		auto symbol = pair.value;

		if ((symbol->flags & SYMBOL_BIT_TYPE))
			continue;
		
		if (symbol->type->kind == CODE_TYPE_PROCEDURE && (symbol->flags & SYMBOL_BIT_CONSTANT))
			continue;
		
		if (symbol->address.kind == Symbol_Address::CCALL)
			continue;
		
		void *data = nullptr;
		
		if (symbol->address.kind == Symbol_Address::STACK)
		{
			auto offset = stack_top + symbol->address.offset;
			if (offset > skip_stack_offset) continue;
			data = interp->stack + offset;
		}
		else if (symbol->address.kind == Symbol_Address::GLOBAL)
			data = interp->global + symbol->address.offset;
		else if (symbol->address.kind == Symbol_Address::CODE)
			data = symbol->address.code;
		else if (symbol->address.kind == Symbol_Address::CCALL)
			data = (void *)symbol->address.ccall;
		else
			Unreachable();

		json_write_symbol(json, interp, symbol->name, symbol->type, data);
	}
}

static void json_write_table_symbols(Interpreter *interp, Json_Writer *json, Symbol_Table *symbol_table, uint64_t stack_top, uint64_t skip_stack_offset)
{
	for (auto symbols = symbol_table; symbols != interp->global_symbol_table; symbols = symbols->parent)
	{
		json_write_symbols(interp, json, symbols, stack_top, skip_stack_offset);
	}
}

static void json_write_procedure_symbols(Interpreter *interp, Json_Writer *json, String procedure_name, Symbol_Table *symbol_table, uint64_t stack_top, uint64_t skip_stack_offset)
{
	json->begin_object();

	json->write_key_value_formatted("procedure", "%", procedure_name);
	json->write_key("variables");
	json->begin_array();
	json_write_table_symbols(interp, json, symbol_table, stack_top, skip_stack_offset);
	json->end_array();
	
	json->end_object();
}

static void intercept(Interpreter *interp, Intercept_Kind intercept, Code_Node *node)
{
	auto context = (Interp_User_Context *)interp->user_context;

	auto json = &context->json;

	if (intercept == INTERCEPT_PROCEDURE_CALL || intercept == INTERCEPT_PROCEDURE_RETURN)
	{
		auto proc = (Code_Node_Block *)node;
		auto procedure_type = interp->current_procedure;

		const char *intercept_type = (intercept == INTERCEPT_PROCEDURE_CALL) ? "call" : "return";

		json->begin_object();

		json->write_key_value_formatted("intercept", "procedure_%", intercept_type);
		json->write_key_value("line_number", proc->procedure_source_row);

		clock_t count = clock();
		float ms = ((count - context->prev_count) * 1000.0f) / (float)CLOCKS_PER_SEC;
		json->write_key_value("exe_dt", ms);
		ms = ((count - context->first_count) * 1000.0f) / (float)CLOCKS_PER_SEC;
		json->write_key_value("exe_time", ms);
		context->prev_count = count;

		json->write_key("globals");
		json->begin_array();
		json_write_symbols(interp, json, interp->global_symbol_table, 0, 0);
		json->end_array();

		json->write_key("callstack");
		json->begin_array();
		
		// Don't print the last added calstack before we are already printing it
		for (int64_t index = 0; index < context->callstack.count; ++index)
		{
			auto call = &context->callstack[index];
			json_write_procedure_symbols(interp, json, call->procedure_name, call->symbols, call->stack_top, interp->stack_top);
		}

		json->end_array();

		json->write_key("console_out");
		json->begin_string_value();
		json->append_builder(&context->console_out);
		json->end_string_value();

		json->write_key_value_formatted("console_in", "%", context->console_in);

		json->end_object();

		if (intercept == INTERCEPT_PROCEDURE_CALL)
		{
			context->callstack.Add(make_procedure_call(procedure_type->name, interp->stack_top, &proc->symbols));
		}
		else
		{
			Assert(intercept == INTERCEPT_PROCEDURE_RETURN);
			context->callstack.RemoveLast();
		}
	}
	else if (intercept == INTERCEPT_STATEMENT)
	{
		auto statement = (Code_Node_Statement *)node;

		json->begin_object();
		
		json->write_key_value_formatted("intercept", "statement");
		json->write_key_value("line_number", statement->source_row);

		clock_t count = clock();
		float ms = ((count - context->prev_count) * 1000.0f) / (float)CLOCKS_PER_SEC;
		json->write_key_value("exe_dt", ms);
		ms = ((count - context->first_count) * 1000.0f) / (float)CLOCKS_PER_SEC;
		json->write_key_value("exe_time", ms);
		context->prev_count = count;

		json->write_key("globals");
		json->begin_array();
		json_write_symbols(interp, json, interp->global_symbol_table, 0, 0);
		json->end_array();

		json->write_key("callstack");
		json->begin_array();

		// Don't print the last added calstack before we are already printing it
		for (int64_t index = 0; index < context->callstack.count - 1; ++index)
		{
			auto call = &context->callstack[index];
			json_write_procedure_symbols(interp, json, call->procedure_name, call->symbols, call->stack_top, interp->stack_top);
		}

		json_write_procedure_symbols(interp, json, interp->current_procedure->name, statement->symbol_table, interp->stack_top, UINT64_MAX);

		json->end_array();

		json->write_key("console_out");
		json->begin_string_value();
		json->append_builder(&context->console_out);
		json->end_string_value();

		json->write_key_value_formatted("console_in", "%", context->console_in);

		json->end_object();
	}
}

void json_write_syntax_node(Json_Writer *json, Syntax_Node *root)
{
	static const String SyntaxNodeTypeNames[] = {
		"null", "literal", "identifier", "unary_operator", "binary_operator",
		"procedure_prototype_argument", "procedure_prototype", "type", "size_of", "type_of", "type_cast",
		"return", "break", "continue", "assignment", "expression", "procedure_parameter", "procedure_call",
		"subscript", "if", "for", "while", "do", "procedure_argument", "procedure", "declaration",
		"struct", "array_view", "static_array", "statement", "block", "global_scope"
	};

	Assert(root->kind < ArrayCount(SyntaxNodeTypeNames));

	json->begin_object();
	json->write_key_value_formatted("id", "%", SyntaxNodeTypeNames[root->kind]);

	json->write_key("payload");
	json->begin_object();

	switch (root->kind)
	{
		case SYNTAX_NODE_NULL: break;

		case SYNTAX_NODE_LITERAL:
		{
			auto node = (Syntax_Node_Literal *)root;

			static const String LiteralTypes[] = { "byte", "integer", "real", "bool", "string", "null" };
			Assert(node->value.kind < ArrayCount(LiteralTypes));

			json->write_key_value_formatted("type", "%", LiteralTypes[node->value.kind]);
			switch (node->value.kind)
			{
				case Literal::BYTE: json->write_key_value_formatted("value", "%", node->value.data.integer); break;
				case Literal::INTEGER: json->write_key_value_formatted("value", "%", node->value.data.integer); break;
				case Literal::REAL: json->write_key_value_formatted("value", "%", node->value.data.real); break;
				case Literal::BOOL: json->write_key_value_formatted("value", "%", node->value.data.boolean ? "true" : "false"); break;
				case Literal::STRING: json->write_key_value_formatted("value", "%", node->value.data.string.data); break;
				case Literal::NULL_POINTER: json->write_key_value_formatted("value", "null"); break;
				NoDefaultCase();
			}
		} break;

		case SYNTAX_NODE_IDENTIFIER:
		{
			auto node = (Syntax_Node_Identifier *)root;
			json->write_key_value_formatted("name", "%", node->name);
		} break;

		case SYNTAX_NODE_UNARY_OPERATOR:
		{
			auto node = (Syntax_Node_Unary_Operator *)root;
			json->write_key_value_formatted("sym", "%", token_kind_string(node->op));
			json->write_key("child");
			json_write_syntax_node(json, node->child);
		} break;

		case SYNTAX_NODE_BINARY_OPERATOR:
		{
			auto node = (Syntax_Node_Binary_Operator *)root;
			json->write_key_value_formatted("sym", "%", token_kind_string(node->op));
			json->write_key("left");
			json_write_syntax_node(json, node->left);
			json->write_key("right");
			json_write_syntax_node(json, node->right);
		} break;

		case SYNTAX_NODE_PROCEDURE_PROTOTYPE_ARGUMENT:
		{
			auto node = (Syntax_Node_Procedure_Prototype_Argument *)root;
			json->write_key("arg_type");
			json_write_syntax_node(json, node->type);
		} break;

		case SYNTAX_NODE_PROCEDURE_PROTOTYPE:
		{
			auto node = (Syntax_Node_Procedure_Prototype *)root;
			json->write_key("arg_types");
			json->begin_array();
			for (auto arg = node->arguments_type; arg; arg = arg->next)
			{
				json_write_syntax_node(json, arg);
			}
			json->end_array();
			
			if (node->return_type)
			{
				json->write_key("return_type");
				json_write_syntax_node(json, node->return_type);
			}
			else
			{
				json->write_key_value("return_type", "null");
			}
		} break;

		case SYNTAX_NODE_TYPE:
		{
			auto        node = (Syntax_Node_Type *)root;

			static const String TypeIdNames[] = { 
				"error", "void", "byte", "int", "float", "bool", "variadic_argument",
				"pointer", "procedure", "identifier", "type_of", "array_view", "static_array" };

			Assert(node->id < ArrayCount(TypeIdNames));
			json->write_key_value_formatted("name", "%", TypeIdNames[node->id]);

			if (node->type)
			{
				json->write_key("underlying_type");
				json_write_syntax_node(json, node->type);
			}
		} break;

		case SYNTAX_NODE_SIZE_OF:
		{
			auto node = (Syntax_Node_Size_Of *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->type);
		} break;

		case SYNTAX_NODE_TYPE_OF:
		{
			auto node = (Syntax_Node_Type_Of *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->expression);
		} break;

		case SYNTAX_NODE_TYPE_CAST:
		{
			auto node = (Syntax_Node_Type_Cast *)root;
			json->write_key("target_type");
			json_write_syntax_node(json, node->type);
			json->write_key("src_expr");
			json_write_syntax_node(json, node->expression);
		} break;

		case SYNTAX_NODE_RETURN:
		{
			auto node = (Syntax_Node_Return *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->expression);
		} break;

		case SYNTAX_NODE_BREAK: break;
		case SYNTAX_NODE_CONTINUE: break;

		case SYNTAX_NODE_ASSIGNMENT:
		{
			auto node = (Syntax_Node_Assignment *)root;
			json->write_key("left");
			json_write_syntax_node(json, node->left);
			json->write_key("right");
			json_write_syntax_node(json, node->right);
		} break;

		case SYNTAX_NODE_EXPRESSION:
		{
			auto node = (Syntax_Node_Expression *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->child);
		} break;

		case SYNTAX_NODE_PROCEDURE_PARAMETER:
		{
			auto node = (Syntax_Node_Procedure_Parameter *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->expression);
		} break;

		case SYNTAX_NODE_PROCEDURE_CALL:
		{
			auto node = (Syntax_Node_Procedure_Call *)root;
			json->write_key("parameters");
			json->begin_array();
			for (auto param = node->parameters; param; param = param->next)
			{
				json_write_syntax_node(json, param);
			}
			json->end_array();
		} break;

		case SYNTAX_NODE_SUBSCRIPT:
		{
			auto node = (Syntax_Node_Subscript *)root;
			json->write_key("expr");
			json_write_syntax_node(json, node->expression);
			json->write_key("subscript");
			json_write_syntax_node(json, node->subscript);
		} break;

		case SYNTAX_NODE_IF:
		{
			auto node = (Syntax_Node_If *)root;
			json->write_key("condition");
			json_write_syntax_node(json, node->condition);
			json->write_key("true-block");
			json_write_syntax_node(json, node->true_statement);

			if (node->false_statement)
			{
				json->write_key("false-block");
				json_write_syntax_node(json, node->false_statement);
			}
			else
			{
				json->write_key_value("false-block", "null");
			}
		} break;

		case SYNTAX_NODE_FOR:
		{
			auto node = (Syntax_Node_For *)root;
			json->write_key("initialization"); json_write_syntax_node(json, node->initialization);
			json->write_key("condition"); json_write_syntax_node(json, node->condition);
			json->write_key("increment"); json_write_syntax_node(json, node->increment);
			json->write_key("body"); json_write_syntax_node(json, node->body);
		} break;

		case SYNTAX_NODE_WHILE:
		{
			auto node = (Syntax_Node_While *)root;
			json->write_key("condition"); json_write_syntax_node(json, node->condition);
			json->write_key("body"); json_write_syntax_node(json, node->body);
		} break;

		case SYNTAX_NODE_DO:
		{
			auto node = (Syntax_Node_Do *)root;
			json->write_key("body"); json_write_syntax_node(json, node->body);
			json->write_key("condition"); json_write_syntax_node(json, node->condition);
		} break;

		case SYNTAX_NODE_PROCEDURE_ARGUMENT:
		{
			auto node = (Syntax_Node_Procedure_Argument *)root;
			json->write_key("child");
			json_write_syntax_node(json, node->declaration);
		} break;

		case SYNTAX_NODE_PROCEDURE:
		{
			auto node = (Syntax_Node_Procedure *)root;

			json->write_key("arguments");
			json->begin_array();
			for (auto arg = node->arguments; arg; arg = arg->next)
			{
				json_write_syntax_node(json, arg);
			}
			json->end_array();

			if (node->return_type)
			{
				json->write_key("return_type");
				json_write_syntax_node(json, node->return_type);
			}
			else
			{
				json->write_key_value("return_type", "null");
			}

			json->write_key("body");
			json_write_syntax_node(json, node->body);
		} break;

		case SYNTAX_NODE_DECLARATION:
		{
			auto node = (Syntax_Node_Declaration *)root;
			json->write_key_value_formatted("decl_type", "%", (node->flags & SYMBOL_BIT_CONSTANT) ? "const" : "var");
			json->write_key_value_formatted("identifier", "%", node->identifier);

			if (node->type)
			{
				json->write_key("type");
				json_write_syntax_node(json, node->type);
			}
			else
			{
				json->write_key_value("type", "null");
			}
			
			if (node->initializer)
			{
				json->write_key("initializer");
				json_write_syntax_node(json, node->initializer);
			}
			else
			{
				json->write_key_value("initializer", "null");
			}
		} break;

		case SYNTAX_NODE_STRUCT:
		{
			auto node = (Syntax_Node_Struct *)root;
			json->write_key("decls");
			json->begin_array();
			for (auto decl = node->members; decl; decl = decl->next)
			{
				json_write_syntax_node(json, decl->declaration);
			}
			json->end_array();
		} break;

		case SYNTAX_NODE_ARRAY_VIEW:
		{
			auto node = (Syntax_Node_Array_View *)root;
			json->write_key("element_type");
			json_write_syntax_node(json, node->element_type);
		} break;

		case SYNTAX_NODE_STATIC_ARRAY:
		{
			auto node = (Syntax_Node_Static_Array *)root;
			json->write_key("count");
			json_write_syntax_node(json, node->expression);
			json->write_key("element_type");
			json_write_syntax_node(json, node->element_type);
		} break;

		case SYNTAX_NODE_STATEMENT:
		{
			auto node = (Syntax_Node_Statement *)root;
			json->write_key_value("line", node->location.start_row);
			json->write_key("child");
			json_write_syntax_node(json, node->node);
		} break;

		case SYNTAX_NODE_BLOCK:
		{
			auto node = (Syntax_Node_Block *)root;
			json->write_key("statements");
			json->begin_array();
			for (auto statement = node->statements; statement; statement = statement->next)
			{
				json_write_syntax_node(json, statement);
			}
			json->end_array();
		} break;

		case SYNTAX_NODE_GLOBAL_SCOPE:
		{
			auto node = (Syntax_Node_Global_Scope *)root;
			json->write_key("decls");
			json->begin_array();
			for (auto decl = node->declarations; decl; decl = decl->next)
			{
				json_write_syntax_node(json, decl->declaration);
			}
			json->end_array();
		} break;
	}

	json->end_object();
	json->end_object();
}

static void collect_symbols_from_block(Array<Key_Value<String, Symbol *>> &arr, Code_Node_Block *block)
{
	for (auto sym : block->symbols.map)
		arr.Add(sym);

	for (auto statement = block->statement_head; statement; statement = statement->next)
	{
		if (statement->node->kind == CODE_NODE_BLOCK)
		{
			auto block = (Code_Node_Block *)statement->node;
			collect_symbols_from_block(arr, block);
		}
	}
}

static void json_write_symbol_table(Json_Writer *json, const Array_View<Key_Value<String, Symbol *>> table);

static void json_write_symbols_from_block(Json_Writer *json, Code_Node_Block *block)
{
	Array<Key_Value<String, Symbol *>> symbols;
	symbols.Reserve(block->statement_count + block->symbols.map.ElementCount());

	collect_symbols_from_block(symbols, block);

	qsort(symbols.data, symbols.count, sizeof(*symbols.data), [](const void *_a, const void *_b) -> int {
		auto a = (Key_Value<String, Symbol *>*)_a;
		auto b = (Key_Value<String, Symbol *>*)_b;
		return (int)((int64_t)a->value->location.start_row - (int64_t)b->value->location.start_row);
	});

	json_write_symbol_table(json, symbols);

	Free(&symbols);
}

static void json_write_symbol_table(Json_Writer *json, const Array_View<Key_Value<String, Symbol *>> table)
{
	json->begin_array();

	for (const auto &sym : table)
	{
		if (sym.value->flags & SYMBOL_BIT_COMPILER_DEF || sym.value->address.kind == Symbol_Address::CCALL)
			continue;

		json->begin_object();
		json->write_key_value_formatted("name", "%", sym.key);
		json->write_key("type");
		json->begin_string_value();
		json_write_type_name(json, sym.value->type);
		json->end_string_value();

		size_t line = sym.value->location.start_row;
		json->write_key_value("line", line);

		auto type = sym.value->type;
		if (type->kind == CODE_TYPE_PROCEDURE && sym.value->address.kind == Symbol_Address::CODE)
		{
			json->write_key("arguments");
			json_write_symbol_table(json, sym.value->address.code->symbols.parent->map.storage);
			json->write_key("symbols");
			json_write_symbols_from_block(json, sym.value->address.code);
		}
		else if (type->kind == CODE_TYPE_STRUCT && sym.value->address.kind == Symbol_Address::CODE)
		{
			json->write_key_null("arguments");
			json->write_key("symbols");
			json_write_symbols_from_block(json, sym.value->address.code);
		}
		else
		{
			json->write_key_null("arguments");
			json->write_key_null("symbols");
		}

		json->end_object();
	}

	json->end_array();
}

bool GenerateDebugCodeInfo(String code, String input, Memory_Arena *arena, String_Builder *builder)
{
	Interp_User_Context context;
	context.json.builder = builder;

	context.console_in = input;

	context.json.begin_object();

	auto prev_allocator = ThreadContext.allocator;
	Defer{ ThreadContext.allocator = prev_allocator; };
	
	ThreadContext.allocator = MemoryArenaAllocator(arena);

	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	Parser parser;
	parser_init(&parser, code, context.json.builder);

	context.json.write_key("error");
	context.json.begin_string_value();

	auto node = parse_global_scope(&parser);

	if (parser.error_count) {
		context.json.end_string_value();
		context.json.end_object();
		return false;
	}

	auto resolver = code_type_resolver_create(context.json.builder);

	include_basic(resolver);

	auto exprs = code_type_resolve(resolver, node);

	if (code_type_resolver_error_count(resolver)) {
		context.json.end_string_value();
		context.json.end_object();
		return false;
	}

	Heap_Allocator heap_allocator;

	const uint32_t stack_size = 1024 * 1024 * 4;

	Interpreter interp;
	interp.intercept = intercept;
	interp.user_context = &context;
	interp.global_symbol_table = code_type_resolver_global_symbol_table(resolver);
	interp.heap = &heap_allocator;
	interp_init(&interp, resolver, stack_size, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	auto main_proc = interp_find_main(&interp);

	if (!main_proc) {
		context.json.end_string_value();
		context.json.end_object();
		return false;
	}

	context.json.end_string_value();

	context.json.write_key("runtime");
	context.json.begin_array();

	clock_t count = clock();
	context.prev_count = count;
	context.first_count = count;

	interp_evaluate_procedure(&interp, main_proc);

	count = clock();
	float ms = ((count - context.first_count) * 1000.0f) / (float)CLOCKS_PER_SEC;

	context.json.end_array();

	context.json.write_key_value("exe_time", ms);
	context.json.write_key_value("bss_size", code_type_resolver_bss_allocated(resolver));
	context.json.write_key_value("stack_size", stack_size);
	context.json.write_key_value("heap_allocated", heap_allocator.total_allocated);
	context.json.write_key_value("heap_freed", heap_allocator.total_freed);
	context.json.write_key_value("heap_leaked", heap_allocator.total_allocated - heap_allocator.total_freed);

	context.json.write_key("map");
	json_write_symbol_table(&context.json, interp.global_symbol_table->map.storage);

	//context.json.write_key("ast");
	//json_write_syntax_node(&context.json, node);

	context.json.end_object();

	return true;
}
