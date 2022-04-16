#include "Kr/KrBasic.h"
#include "Parser.h"
#include "Resolver.h"

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

static void parser_on_error(Parser *parser) { exit(0); }
static void code_type_resolver_on_error(Code_Type_Resolver *resolver) { exit(0); }

int main()
{
	InitThreadContext(0);

	parser_register_error_proc(parser_on_error);
	code_type_resolver_register_error_proc(code_type_resolver_on_error);

	String_Builder builder;

	Parser parser;
	parser_init(&parser, code, &builder);

	auto node = parse_global_scope(&parser);

	if (parser.error_count) {
		String str = BuildString(&builder);
		fprintf(stderr, "%s", str.data);
		return false;
	}

	auto resolver = code_type_resolver_create(&context.json);

	include_basic(resolver);

	auto exprs = code_type_resolve(resolver, node);

	if (code_type_resolver_error_count(resolver)) {
		context.json.end_array();
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
		context.json.end_array();
		context.json.end_object();
		return false;
	}

	context.json.end_array();

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
	return 0;
}

