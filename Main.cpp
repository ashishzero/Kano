#include "Common.h"

#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"
#include "Printer.h"

void handle_assertion(const char *reason, const char *file, int line, const char *proc)
{
	fprintf(stderr, "%s. File: %s(%d)\n", reason, file, line);
	DebugTriggerbreakpoint();
}

String read_entire_file(const char *file)
{
	FILE *f = fopen(file, "rb");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *string = (uint8_t *)malloc(fsize + 1);
	fread(string, 1, fsize, f);
	fclose(f);

	string[fsize] = 0;
	return String(string, (int64_t)fsize);
}

bool print_error(Error_List *list)
{
	for (auto error = list->first.next; error; error = error->next)
	{
		auto row     = error->location.start_row;
		auto column  = error->location.start_column;
		auto message = error->message.data;
		fprintf(stderr, "%zu,%zu: %s\n", row, column, message);
	}

	return list->first.next != nullptr;
}

void print_type(Code_Type *type)
{
	switch (type->kind)
	{
		case CODE_TYPE_NULL: printf("void"); return;
		case CODE_TYPE_INTEGER: printf("int"); return;
		case CODE_TYPE_REAL: printf("float"); return;
		case CODE_TYPE_BOOL: printf("bool"); return;

		case CODE_TYPE_POINTER: {
			printf("*"); 
			print_type(((Code_Type_Pointer *)type)->base_type);
			return;
		}

		case CODE_TYPE_PROCEDURE: {
			auto proc = (Code_Type_Procedure *)type;
			printf("proc (");
			for (int64_t index = 0; index < proc->argument_count; ++index)
			{
				print_type(proc->arguments[index]);
				if (index < proc->argument_count - 1) printf(", ");
			}
			printf(")");

			if (proc->return_type)
			{
				printf(" -> ");
				print_type(proc->return_type);
			}
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto strt = (Code_Type_Struct *)type;
			printf("%s", strt->name.data); 
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr = (Code_Type_Array_View *)type;
			printf("[] ");
			print_type(arr->element_type);
			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr = (Code_Type_Static_Array *)type;
			printf("[%u] ", arr->element_count);
			print_type(arr->element_type);
			return;
		}
	}
}

void print_value(Code_Type *type, void *data)
{
	switch (type->kind)
	{
		case CODE_TYPE_NULL: printf("null"); return;
		case CODE_TYPE_INTEGER: printf("%zd", *(Kano_Int *)data); return;
		case CODE_TYPE_REAL: printf("%f", *(Kano_Real *)data); return;
		case CODE_TYPE_BOOL: printf("%s", (*(Kano_Bool *)data) ? "true" : "false"); return;

		case CODE_TYPE_POINTER: {
			auto pointer = (Code_Type_Pointer *)type;
			printf("%p { ", data);
			print_value(pointer->base_type, *(uint8_t **)data);
			printf(" }");
			return;
		}

		case CODE_TYPE_PROCEDURE: printf("%p", data); return;

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;
			printf("{ ");
			
			for (int64_t index = 0; index < _struct->member_count; ++index)
			{
				auto member = &_struct->members[index];
				printf("%s: ", member->name.data);
				print_value(member->type, (uint8_t *)data + member->offset);
				if (index < _struct->member_count - 1)
					printf(", ");
			}

			printf(" }");
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr_type = (Code_Type_Array_View *)type;
			
			auto arr_count = *(Kano_Int *)data;
			auto arr_data = (uint8_t *)data + sizeof(Kano_Int);
			
			printf("[ ");
			for (int64_t index = 0; index < arr_count; ++index)
			{
				print_value(arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				if (index < arr_count - 1)
					printf(", ");
			}
			printf(" ]");

			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr_type = (Code_Type_Static_Array *)type;

			auto arr_data = (uint8_t *)data;

			printf("[ ");
			for (int64_t index = 0; index < arr_type->element_count; ++index)
			{
				print_value(arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				if (index < arr_type->element_count - 1)
					printf(", ");
			}
			printf(" ]");

			return;
		}
	}
}

void intercept(Interpreter *interp, Intercept_Kind intercept, Code_Node *node)
{
	if (intercept == INTERCEPT_PROCEDURE_CALL)
	{
		auto proc = (Code_Node_Procedure_Call *)node;
		printf("Procedure Call: %s: %zu\n", proc->procedure_type->name.data, proc->source_row);
	}
	else if (intercept == INTERCEPT_PROCEDURE_RETURN)
	{
		auto proc = (Code_Node_Procedure_Call *)node;
		printf("Procedure Return: %s: %zu\n", proc->procedure_type->name.data, proc->source_row);
	}
	else if (intercept == INTERCEPT_STATEMENT)
	{
		auto statement = (Code_Node_Statement *)node;
		printf("Executing statement: %zu\n", statement->source_row);

		printf("%-15s %s\n", "Name", "Value");
		for (auto symbols = statement->symbol_table; symbols; symbols = symbols->parent)
		{
			for (auto symbol : symbols->buffer)
			{
				if ((symbol->flags & SYMBOL_BIT_TYPE))
					continue;

				if (symbol->type->kind == CODE_TYPE_PROCEDURE && (symbol->flags & SYMBOL_BIT_CONSTANT))
					continue;

				if (symbol->address.kind == Symbol_Address::CCALL)
					continue;

				void *data = interp->stack + symbols->stack_offset + symbol->address.offset;

				printf("%-15s ", symbol->name.data);
				//print_type(symbol->type);
				print_value(symbol->type, data);
				printf("\n");
			}
		}
		printf("\n\n");
	}
}

#include <math.h>

static void basic_print(String fmt, uint8_t *args)
{
	for (int64_t index = 0; index < fmt.length;)
	{
		if (fmt[index] == '%')
		{
			index += 1;
			if (index < fmt.length)
			{
				if (fmt[index] == 'd')
				{
					index += 1;
					auto value = (Kano_Int *)(args);
					printf("%zd", *value);
					args += sizeof(Kano_Int);
				}
				else if (fmt[index] == 'f')
				{
					index += 1;
					auto value = (Kano_Real *)(args);
					printf("%f", *value);
					args += sizeof(Kano_Real);
				}
				else if (fmt[index] == 'b')
				{
					index += 1;
					auto value = (Kano_Bool *)(args);
					printf("%s", *value ? "true" : "false");
					args += sizeof(Kano_Bool);
				}
				else if (fmt[index] == '%')
				{
					printf("%%");
					index += 1;
				}
			}
			else
			{
				printf("%%");
			}
		}
		else if (fmt[index] == '\\')
		{
			index += 1;
			if (index < fmt.length)
			{
				if (fmt[index] == 'n')
				{
					index += 1;
					printf("\n");
				}
				else if (fmt[index] == '\\')
				{
					printf("\\");
					index += 1;
				}
			}
			else
			{
				printf("\\");
			}
		}
		else
		{
			printf("%c", fmt[index]);
			index += 1;
		}
	}
}

static void *basic_allocate(Kano_Int size) { return malloc(size); }
static void basic_free(void *ptr) { free(ptr); }
static double basic_sin(double x) { return sin(x); }
static double basic_cos(double x) { return cos(x); }
static double basic_tan(double x) { return tan(x); }

void include_basic(Code_Type_Resolver *resolver)
{
	Procedure_Builder builder(resolver);
	
	proc_builder_argument(&builder, "string");
	proc_builder_variadic(&builder);
	proc_builder_register(&builder, "print", InterpMorphProc(basic_print));

	proc_builder_argument(&builder, "int");
	proc_builder_return(&builder, "*void");
	proc_builder_register(&builder, "allocate", InterpMorphProc(basic_allocate));

	proc_builder_argument(&builder, "*void");
	proc_builder_register(&builder, "free", InterpMorphProc(basic_free));

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "sin", InterpMorphProc(basic_sin));

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "cos", InterpMorphProc(basic_cos));

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "tan", InterpMorphProc(basic_tan));

	proc_builder_free(&builder);
}

int main()
{
	String content = read_entire_file("Simple.kano");

	auto   builder = new String_Builder;

	Parser parser;
	parser_init(&parser, content, builder);

	auto node = parse_global_scope(&parser);

	if (print_error(&parser.error))
		return 1;

	auto resolver = code_type_resolver_create();

	include_basic(resolver);

	auto exprs = code_type_resolve(resolver, node);

	Interpreter interp;
	interp.intercept = intercept;
	interp_init(&interp, 1024 * 1024 * 4, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	int result = interp_eval_main(&interp, resolver);

	return result;
}
