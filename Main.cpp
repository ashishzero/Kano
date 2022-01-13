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

void print(const char *filename, Syntax_Node *node)
{
	auto fp = fopen(filename, "wb");
	print_syntax(node, fp);
	fclose(fp);
}

void print(const char *filename, Array_View<Code_Node_Assignment *> exprs)
{
	auto fp = fopen(filename, "wb");
	for (auto expr : exprs)
		print_code(expr, fp);
	fclose(fp);
}

void print(const char *filename, Code_Node *code)
{
	auto fp = fopen(filename, "wb");
	print_code(code, fp);
	fclose(fp);
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
			for (uint64_t index = 0; index < proc->argument_count; ++index)
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
			
			for (uint64_t index = 0; index < _struct->member_count; ++index)
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

void intercept(Interpreter *interp, Code_Node_Statement *statement)
{
	printf("Executing statement: %zu\n", statement->source_row);

	printf("%-15s %s\n", "Name", "Value");
	for (auto symbols = statement->symbol_table; symbols; symbols = symbols->parent)
	{
		for (auto symbol : symbols->buffer)
		{
			if ((symbol->flags & SYMBOL_BIT_TYPE))
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

int main()
{
	String content = read_entire_file("Simple.kano");

	auto   builder = new String_Builder;

	Parser parser;
	parser_init(&parser, content, builder);

	auto node = parse_global_scope(&parser);

	if (print_error(&parser.error))
		return 1;

	print("syntax.txt", node);

	auto resolver = code_type_resolver_create();

	auto exprs = code_type_resolve(resolver, node);

	print("code.txt", exprs);

	Interpreter interp;
	interp.intercept = intercept;
	interp_init(&interp, 1024 * 1024 * 4, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);

	auto main_proc = code_type_resolver_find(resolver, "main");

	if (!main_proc)
	{
		fprintf(stderr, "\"main\" procedure not defined!\n");
		return 1;
	}

	if (!(main_proc->flags & SYMBOL_BIT_CONSTANT) || main_proc->address.kind != Symbol_Address::CODE)
	{
		fprintf(stderr, "The \"main\" procedure must be constant!\n");
		return 1;
	}

	if (main_proc->type->kind != CODE_TYPE_PROCEDURE)
	{
		fprintf(stderr, "The \"main\" symbol must be a procedure!\n");
		return 1;
	}

	auto proc_type = (Code_Type_Procedure *)main_proc->type;
	if (proc_type->argument_count != 0 || proc_type->return_type)
	{
		fprintf(stderr, "The \"main\" procedure must not take any arguments and should return nothing!\n");
		return 1;
	}

	auto proc = main_proc->address.code;
		
	{
		auto fp = fopen("code.txt", "ab");
		print_code(proc, fp);
		fclose(fp);
	}
		
	interp_eval_procedure_call(&interp, proc);

	return 0;
}
