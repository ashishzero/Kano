#include "Kr/KrBasic.h"

#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"
#include "HeapAllocator.h"

#include <stdlib.h>

struct Json_Writer {
	bool depth[4096] = {};
	int depth_index = 0;

	String_Builder *builder = nullptr;

	void next_element()
	{
		if (depth[depth_index])
			Write(builder, ",");
		else
			depth[depth_index] = true;
	}

	void push_scope()
	{
		Assert(depth_index < ArrayCount(depth));
		next_element();
		depth_index += 1;
		depth[depth_index] = false;
	}

	void pop_scope()
	{
		depth_index -= 1;
	}

	void begin_object()
	{
		push_scope();
		Write(builder, "{");
	}

	void end_object()
	{
		Write(builder, "}");
		pop_scope();
	}

	void begin_array()
	{
		push_scope();
		Write(builder, "[");
	}

	void end_array()
	{
		Write(builder, "]");
		pop_scope();
	}

	void write_key(const char *key)
	{
		push_scope();
		WriteFormatted(builder, "\"%\": ", key);
	}

	template <typename ...Args>
	void write_single_value(const char *fmt, Args... args)
	{
		Write(builder, "\"");
		WriteFormatted(builder, fmt, args...);
		Write(builder, "\"");
		pop_scope();
	}

	void begin_string_value()
	{
		Write(builder, "\"");
	}

	void append_builder(String_Builder *src)
	{
		for (auto buk = &src->head; buk; buk = buk->next)
		{
			WriteBuffer(builder, buk->data, buk->written);
		}
	}

	template <typename ...Args>
	void append_string_value(const char *fmt, Args... args)
	{
		WriteFormatted(builder, fmt, args...);
	}

	void end_string_value()
	{
		Write(builder, "\"");
		pop_scope();
	}

	template <typename ...Args>
	void write_key_value(const char *key, const char *fmt, Args... args)
	{
		next_element();
		WriteFormatted(builder, "\"%\": ", key);
		Write(builder, "\"");
		WriteFormatted(builder, fmt, args...);
		Write(builder, "\"");
	}
};

struct Call_Info {
	String procedure_name;
	uint64_t stack_top;
	Symbol_Table *symbols;
};

struct Interp_User_Context {
	String_Builder console_out;
	Json_Writer json;
	Array<Call_Info> callstack;
};

//
//
//

void AssertHandle(const char *reason, const char *file, int line, const char *proc)
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

	uint8_t *string = (uint8_t *)MemoryAllocate(fsize + 1);
	fread(string, 1, fsize, f);
	fclose(f);

	string[fsize] = 0;
	return String(string, (int64_t)fsize);
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

static void json_write_type(Json_Writer *json, Code_Type *type)
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
			json_write_type(json, ((Code_Type_Pointer *)type)->base_type);
			return;
		}

		case CODE_TYPE_PROCEDURE: {
			auto proc = (Code_Type_Procedure *)type;
			json->append_string_value("proc (");
			for (int64_t index = 0; index < proc->argument_count; ++index)
			{
				json_write_type(json, proc->arguments[index]);
				if (index < proc->argument_count - 1) json->append_string_value(", ");
			}
			json->append_string_value(")");

			if (proc->return_type)
			{
				json->append_string_value(" -> ");
				json_write_type(json, proc->return_type);
			}
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto strt = (Code_Type_Struct *)type;
			json->append_string_value("%", strt->name.data); 
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr = (Code_Type_Array_View *)type;
			json->append_string_value("[] ");
			json_write_type(json, arr->element_type);
			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr = (Code_Type_Static_Array *)type;
			json->append_string_value("[%] ", arr->element_count);
			json_write_type(json, arr->element_type);
			return;
		}
	}
}

enum Memory_Type {
	Memory_Type_INVALID,
	Memory_Type_STACK,
	Memory_Type_GLOBAL,
	Memory_Type_HEAP,
};

static const char *memory_type_string(Memory_Type type) {
	if (type == Memory_Type_INVALID) return "(invalid)";
	if (type == Memory_Type_STACK) return "stack";
	if (type == Memory_Type_GLOBAL) return "global";
	if (type == Memory_Type_HEAP) return "heap";
	return "(null)";
}

static Memory_Type interp_get_memory_type(Interpreter *interp, void *ptr)
{
	if (ptr >= interp->stack && ptr < interp->stack + interp->stack_size)
		return Memory_Type_STACK;
	if (ptr >= interp->global && ptr < interp->global + interp->global_size)
		return Memory_Type_GLOBAL;
	if (heap_contains_memory(interp->heap, ptr))
		return Memory_Type_HEAP;
	return Memory_Type_INVALID;
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
				json->write_key_value("raw", "%", raw_ptr);
			else
				json->write_key_value("raw", "(null)");

			json->write_key("base_type");
			json->begin_string_value();
			json_write_type(json, pointer_type->base_type);
			json->end_string_value();

			auto mem_type = interp_get_memory_type(interp, raw_ptr);

			json->write_key_value("memory", "%", memory_type_string(mem_type));

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
			
			auto arr_count = *(Kano_Int *)data;
			auto arr_data = (uint8_t *)data + sizeof(Kano_Int);

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
	json->write_key_value("name", "%", name.data);
	
	json->write_key("type");
	json->begin_string_value();
	json_write_type(json, type);
	json->end_string_value();
	
	auto mem_type = interp_get_memory_type(interp, data);
	json->write_key_value("address", "%", data);
	json->write_key_value("memory", "%", memory_type_string(mem_type));
	
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

	json->write_key_value("procedure", "%", procedure_name.data);
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

		json->write_key_value("intercept", "procedure_%", intercept_type);
		json->write_key_value("line_number", "%", (int)proc->procedure_source_row);

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
		
		json->write_key_value("intercept", "statement");
		json->write_key_value("line_number", "%", (int)statement->source_row);

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

		json->end_object();
	}
}

#include <math.h>

struct Interp_Morph {
	uint8_t *arg;
	uint64_t offset;
	Interp_Morph(Interpreter *interp): arg(interp->stack + interp->stack_top), offset(0) {}

	template <typename T>
	void OffsetReturn() { offset += sizeof(T); }

	template <typename T>
	void Return(const T &src) { memcpy(arg, &src, sizeof(T)); }

	template <typename T>
	T Arg(uint64_t alignment = sizeof(T))
	{ 
		offset = AlignPower2Up(offset, sizeof(alignment));
		auto ptr = arg + offset;
		offset += sizeof(T);
		return *(T *)ptr;
	}
};

static void stdout_value(Interpreter *interp, String_Builder *out, Code_Type *type, void *data)
{
	if (!data)
	{
		Write(out, "(null)"); printf("(null)");
		return;
	}

	switch (type->kind)
	{
		case CODE_TYPE_NULL: 
			Write(out, "(null)"); printf("(null)");
			return;
		case CODE_TYPE_CHARACTER: 
			Write(out, *(Kano_Char *)data); printf("%c", *(Kano_Char *)data);
			return;
		case CODE_TYPE_INTEGER: 
			Write(out, *(Kano_Int *)data); printf("%zd", *(Kano_Int *)data);
			return;
		case CODE_TYPE_REAL: 
			Write(out, *(Kano_Real *)data); printf("%f", *(Kano_Real *)data);
			return;
		case CODE_TYPE_BOOL: 
			Write(out, (*(Kano_Bool *)data)); printf("%s", (*(Kano_Bool *)data) ? "true" : "false");
			return;
		case CODE_TYPE_PROCEDURE: 
			Write(out, data); printf("%p", data);
			return;

		case CODE_TYPE_POINTER: {
			auto pointer_type = (Code_Type_Pointer *)type;
			void *raw_ptr = *(void **)data;

			Write(out, "{ "); printf("{ ");

			if (raw_ptr)
			{
				WriteFormatted(out, "raw: %, ", raw_ptr); printf("raw: %p, ", raw_ptr);
			}
			else
			{
				Write(out, "raw: (null), "); printf("raw: (null), ");
			}

			auto mem_type = interp_get_memory_type(interp, raw_ptr);

			Write(out, "value: "); printf("value: ");

			if (mem_type != Memory_Type_INVALID)
			{
				stdout_value(interp, out, pointer_type->base_type, raw_ptr);
				Write(out, " "); printf(" ");
			}
			else
			{
				Write(out, raw_ptr ? String("(garbage)") : String("(invalid)")); printf("%s ", raw_ptr ? "(garbage)" : "(invalid)");
			}

			Write(out, "}"); printf("}");
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;

			Write(out, "{ "); printf("{ ");

			for (int64_t index = 0; index < _struct->member_count; ++index)
			{
				auto member = &_struct->members[index];
				Write(out, member->name); printf("%*.s: ", (int)member->name.length, member->name.data);
				stdout_value(interp, out, member->type, (uint8_t *)data + member->offset);

				if (index < _struct->member_count - 1)
				{
					Write(out, ","); printf(",");
				}

				Write(out, " "); printf(" ");
			}

			Write(out, "}"); printf("}");
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr_type = (Code_Type_Array_View *)type;
			
			auto arr_count = *(Kano_Int *)data;
			auto arr_data = (uint8_t *)data + sizeof(Kano_Int);

			Write(out, "[ ");
			printf("[ ");
			for (int64_t index = 0; index < arr_count; ++index)
			{
				stdout_value(interp, out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				Write(out, " "); printf(" ");
			}
			Write(out, "]"); printf("]");

			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr_type = (Code_Type_Static_Array *)type;

			auto arr_data = (uint8_t *)data;

			Write(out, "[ ");
			printf("[ ");
			for (int64_t index = 0; index < arr_type->element_count; ++index)
			{
				stdout_value(interp, out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				Write(out, " "); printf(" ");
			}
			Write(out, "]"); printf("]");

			return;
		}
	}
}

static void basic_print(Interpreter *interp) {
	Interp_Morph morph(interp);

	auto context = (Interp_User_Context *)interp->user_context;
	auto con_out = &context->console_out;

	auto fmt = morph.Arg<String>(sizeof(int64_t));
	auto args = morph.Arg<uint8_t *>();

	for (int64_t index = 0; index < fmt.length;)
	{
		if (fmt[index] == '%')
		{
			index += 1;

			auto type = *(Code_Type **)args;
			args += sizeof(Code_Type *);
			auto ptr = args;
			args += type->runtime_size;

			stdout_value(interp, con_out, type, ptr);
		}
		else if (fmt[index] == '\\')
		{
			index += 1;
			if (index < fmt.length)
			{
				if (fmt[index] == 'n')
				{
					index += 1;
					Write(con_out, "\\n"); printf("\n");
				}
				else if (fmt[index] == '\\')
				{
					Write(con_out, "\\\\"); printf("\\");
					index += 1;
				}
			}
			else
			{
				Write(con_out, "\\\\"); printf("\\");
			}
		}
		else
		{
			Write(con_out, (char)fmt[index]); printf("%c", fmt[index]);
			index += 1;
		}
	}
}

static void basic_allocate(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<void *>();
	auto size = morph.Arg<Kano_Int>();
	auto result = heap_alloc(interp->heap, size);
	morph.Return(result);
}

static void basic_free(Interpreter *interp) {
	Interp_Morph morph(interp);
	auto ptr = morph.Arg<void *>();
	heap_free(interp->heap, ptr);
}

static void basic_sin(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<double>();
	auto x = morph.Arg<double>();
	auto y = sin(x);
	morph.Return(y);
}

static void basic_cos(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<double>();
	auto x = morph.Arg<double>();
	auto y = cos(x);
	morph.Return(y);
}

static void basic_tan(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<double>();
	auto x = morph.Arg<double>();
	auto y = tan(x);
	morph.Return(y);
}

static void include_basic(Code_Type_Resolver *resolver)
{
	Procedure_Builder builder(resolver);
	
	proc_builder_argument(&builder, "string");
	proc_builder_variadic(&builder);
	proc_builder_register(&builder, "print", basic_print);

	proc_builder_argument(&builder, "int");
	proc_builder_return(&builder, "*void");
	proc_builder_register(&builder, "allocate", basic_allocate);

	proc_builder_argument(&builder, "*void");
	proc_builder_register(&builder, "free", basic_free);

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "sin", basic_sin);

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "cos", basic_cos);

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "tan", basic_tan);

	proc_builder_free(&builder);
}

static void parser_on_error(Parser *parser) { exit(0); }
static void code_type_resolver_on_error(Code_Type_Resolver *parser) { exit(0); }


bool GenerateDebugCodeInfo(String code, Memory_Arena *arena, String_Builder *builder)
{
	auto prev_allocator = ThreadContext.allocator;
	Defer{ ThreadContext.allocator = prev_allocator; };
	
	ThreadContext.allocator = MemoryArenaAllocator(arena);

	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	Parser parser;
	parser_init(&parser, code, builder);

	auto node = parse_global_scope(&parser);

	if (parser.error_count)
		return false;

	auto resolver = code_type_resolver_create(builder);

	if (code_type_resolver_error_count(resolver))
		return false;

	include_basic(resolver);

	auto exprs = code_type_resolve(resolver, node);

	Interp_User_Context context;
	context.json.builder = builder;

	context.json.begin_array();

	Heap_Allocator heap_allocator;

	Interpreter interp;
	interp.intercept = intercept;
	interp.user_context = &context;
	interp.global_symbol_table = code_type_resolver_global_symbol_table(resolver);
	interp.heap = &heap_allocator;
	interp_init(&interp, resolver, 1024 * 1024 * 4, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	int result = interp_eval_main(&interp);

	context.json.end_array();

	return true;
}

#if 0
int main()
{
	InitThreadContext(0);

	parser_register_error_proc(parser_on_error);
	code_type_resolver_register_error_proc(code_type_resolver_on_error);

	String content = read_entire_file("Simple.kano");

	auto arena = MemoryArenaCreate(MegaBytes(128));

	String_Builder builder;

	GenerateDebugCodeInfo(content, arena, &builder);

	FILE *out = fopen("DebugInfo.json", "wb");

	for (auto buk = &builder.head; buk; buk = buk->next)
	{
		fwrite(buk->data, buk->written, 1, out);
	}

	fclose(out);

	FreeBuilder(&builder);

	return 0;
}
#endif
