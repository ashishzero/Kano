#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"

#include <stdlib.h>

#include <Windows.h>

String read_entire_file(const char *file);
bool GenerateDebugCodeInfo(String code, Memory_Arena *arena, String_Builder *builder);

static void parser_on_error(Parser *parser) { ExitThread(1); }
static void code_type_resolver_on_error(Code_Type_Resolver *parser) { ExitThread(1); }

struct Code_Execution {
	String code;
	Memory_Arena *arena;
	String_Builder *builder;
};

DWORD WINAPI ExecuteCodeThreadProc(void *param) {
	auto exe = (Code_Execution *)param;

	InitThreadContext(0);

	if (GenerateDebugCodeInfo(exe->code, exe->arena, exe->builder)) {
		return 0;
	}

	return 1;
}

int main() {
	InitThreadContext(0);

	parser_register_error_proc(parser_on_error);
	code_type_resolver_register_error_proc(code_type_resolver_on_error);

	String content = read_entire_file("Simple.kano");

	auto arena = MemoryArenaCreate(MegaBytes(128));

	String_Builder builder;

	Code_Execution *exe = new Code_Execution;
	exe->arena = arena;
	exe->builder = &builder;
	exe->code = content;

	HANDLE thread = CreateThread(nullptr, 0, ExecuteCodeThreadProc, exe, 0, nullptr);

	WaitForSingleObject(thread, INFINITE);

	FILE *out = fopen("DebugInfo.json", "wb");

	for (auto buk = &builder.head; buk; buk = buk->next) {
		fwrite(buk->data, buk->written, 1, out);
	}

	fclose(out);

	FreeBuilder(&builder);

	return 0;
}
