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

	Interp interp;
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
