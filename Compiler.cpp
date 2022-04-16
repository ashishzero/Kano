#include "Kr/KrBasic.h"
#include "Parser.h"
#include "Resolver.h"
#include "StdLib.h"

#include <stdio.h>
#include <stdlib.h>

void AssertHandle(const char *reason, const char *file, int line, const char *proc)
{
	fprintf(stderr, "Internal Compiler Error %s. File: %s(%d)\n", reason, file, line);
	DebugTriggerbreakpoint();
}

String read_entire_file(const char *file)
{
	FILE *f = fopen(file, "rb");
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *string = (uint8_t *)MemoryAllocate(fsize + 1);
	fread(string, 1, fsize, f);
	fclose(f);

	string[fsize] = 0;
	return String(string, (int64_t)fsize);
}

static void parser_on_error(Parser *parser) {
	String str = BuildString(parser->error);
	fprintf(stderr, "%s\n", str.data);
	exit(0);
}
static void code_type_resolver_on_error(Code_Type_Resolver *resolver) { 
	String str = BuildString(code_type_resolver_error_stream(resolver));
	fprintf(stderr, "%s\n", str.data);
	exit(0);
}

int main(int argc, char **argv)
{
	InitThreadContext(0);

	parser_register_error_proc(parser_on_error);
	code_type_resolver_register_error_proc(code_type_resolver_on_error);

	if (argc != 2) {
		fprintf(stderr, "Error: Expected file\n");
		fprintf(stderr, "\tUsage: %s <file>\n\n", argv[0]);
		return 1;
	}

	String code = read_entire_file(argv[1]);
	if (!code.data) {
		fprintf(stderr, "File \"%s\" could not be read.\n\n", argv[1]);
		return 1;
	}

	String_Builder builder;

	Parser parser;
	parser_init(&parser, code, &builder);

	auto node = parse_global_scope(&parser);

	if (parser.error_count) {
		String str = BuildString(&builder);
		fprintf(stderr, "%s\n", str.data);
		return 1;
	}

	auto resolver = code_type_resolver_create(&builder);

	include_basic(resolver);

	auto exprs = code_type_resolve(resolver, node);

	if (code_type_resolver_error_count(resolver)) {
		String str = BuildString(&builder);
		fprintf(stderr, "%s\n", str.data);
		return 1;
	}

	Heap_Allocator heap_allocator;

	const uint32_t stack_size = 1024 * 1024 * 4;

	Interpreter interp;
	interp.user_context = nullptr;
	interp.global_symbol_table = code_type_resolver_global_symbol_table(resolver);
	interp.heap = &heap_allocator;
	interp_init(&interp, resolver, stack_size, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	auto main_proc = interp_find_main(&interp);

	if (!main_proc) {
		String str = BuildString(&builder);
		fprintf(stderr, "%s\n", str.data);
		return 1;
	}

	interp_evaluate_procedure(&interp, main_proc);

	return 0;
}
