#include "Common.h"

#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"
#include "Printer.h"
#include "HeapAllocator.h"

struct String_Stream {
	Array<char> buffer;

	void write_fmt(const char *fmt, ...)
	{
		va_list args0, args1;
		va_start(args0, fmt);
		va_copy(args1, args0);
		
		int   len = 1 + vsnprintf(NULL, 0, fmt, args1);
		buffer.reserve(buffer.count + len);
		char *buf = buffer.data + buffer.count;
		vsnprintf(buf, len, fmt, args0);
		buffer.count += (len - 1);
		
		va_end(args1);
		va_end(args0);
	}

	const char *get_cstring() {
		buffer.reserve(buffer.count + 1);
		buffer.data[buffer.count] = 0;
		return buffer.data;
	}
};

struct Json_Writer {
	Array<int> depth;

	FILE *out = nullptr;
	int indent = 0;
	bool same_line = false;

	void init(FILE *fp)
	{
		out = fp;
		depth.add(0);
	}

	void write_newline(int count = 1)
	{
		for (int i = 0; i < count; ++i)
			fprintf(out, "\n");
	}

	void next_element(int newline_count = 1)
	{
		auto value = depth.last();
		if (*value)
		{
			fprintf(out, ",");
			if (!same_line)
				write_newline(newline_count);
		}

		*value += 1;
	}

	void push_scope(int newline_count = 1)
	{
		next_element(newline_count);
		depth.add(0);
	}

	void pop_scope()
	{
		depth.remove_last();
	}

	void write_indent()
	{
		if (indent == 0) return;
		fprintf(out, "%*s", indent, "\t");
	}

	void begin_object(bool imm = false, int newline_count = 1)
	{
		push_scope(newline_count);

		if (!imm)
			write_indent();

		fprintf(out, "{");

		if (!same_line)
		{
			fprintf(out, "\n");
			indent += 1;
		}

		indent += 1;
	}

	void end_object()
	{
		indent -= 1;

		if (!same_line)
		{
			indent -= 1;
			fprintf(out, "\n");
			write_indent();
		}

		fprintf(out, "}");
		pop_scope();
	}

	void begin_array(bool imm = false, int newline_count = 1)
	{
		push_scope(newline_count);
		
		if (!imm)
			write_indent();

		fprintf(out, "[");

		if (!same_line)
		{
			fprintf(out, "\n");
			indent += 1;
		}

		indent += 1;
	}

	void end_array()
	{
		indent -= 1;

		if (!same_line)
		{
			indent -= 1;
			write_newline();
			write_indent();
		}
		fprintf(out, "]");
		pop_scope();
	}

	void write_key(const char *key)
	{
		push_scope();
		write_indent();
		fprintf(out, "\"%s\": ", key);
	}

	void write_single_value(const char *fmt, ...)
	{
		fprintf(out, "\"");
		va_list args;
		va_start(args, fmt);
		vfprintf(out, fmt, args);
		va_end(args);
		fprintf(out, "\"");
		pop_scope();
	}

	void begin_string_value()
	{
		fprintf(out, "\"");
	}

	void append_string_value(const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		vfprintf(out, fmt, args);
		va_end(args);
	}

	void end_string_value()
	{
		fprintf(out, "\"");
		pop_scope();
	}

	void write_key_value(const char *key, const char *fmt, ...)
	{
		next_element();
		write_indent();
		fprintf(out, "\"%s\": ", key);
		fprintf(out, "\"");
		va_list args;
		va_start(args, fmt);
		vfprintf(out, fmt, args);
		va_end(args);
		fprintf(out, "\"");
	}
};

struct Call_Info {
	String procedure_name;
	uint64_t stack_top;
	Symbol_Table *symbols;
};

struct Interp_User_Context {
	String_Stream console_out;

	Json_Writer json;

	Array<Call_Info> callstack;
};

//
//
//

void handle_assertion(const char *reason, const char *file, int line, const char *proc)
{
	fprintf(stderr, "%s. File: %s(%d)\n", reason, file, line);
	DebugTriggerbreakpoint();
}

static String read_entire_file(const char *file)
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

static bool print_error(Error_List *list)
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
			json->append_string_value("%s", strt->name.data); 
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
			json->append_string_value("[%u] ", arr->element_count);
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
		case CODE_TYPE_CHARACTER: json->write_single_value("%d", (int) *(Kano_Char *)data); return;
		case CODE_TYPE_INTEGER: json->write_single_value("%zd", *(Kano_Int *)data); return;
		case CODE_TYPE_REAL: json->write_single_value("%f", *(Kano_Real *)data); return;
		case CODE_TYPE_BOOL: json->write_single_value("%s", (*(Kano_Bool *)data) ? "true" : "false"); return;
		case CODE_TYPE_PROCEDURE: json->write_single_value("0x%8zx", data); return;

		case CODE_TYPE_POINTER: {
			auto pointer_type = (Code_Type_Pointer *)type;
			void *raw_ptr = *(void **)data;

			json->begin_object(true);

			if (raw_ptr)
				json->write_key_value("raw", "0x%8zx", raw_ptr);
			else
				json->write_key_value("raw", "(null)");

			json->write_key("base_type");
			json->begin_string_value();
			json_write_type(json, pointer_type->base_type);
			json->end_string_value();

			auto mem_type = interp_get_memory_type(interp, raw_ptr);

			json->write_key_value("memory", "%s", memory_type_string(mem_type));

			json->write_key("value");
			if (mem_type != Memory_Type_INVALID)
			{
				json_write_value(json, interp, pointer_type->base_type, raw_ptr);
			}
			else
			{
				json->write_single_value("%s", raw_ptr ? "(garbage)" : "(invalid)");
			}

			json->end_object();
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;

			json->begin_array(true);

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

			json->begin_array(true);			
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

			json->begin_array(true);
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
	json->write_key_value("name", "%s", name.data);
	
	json->write_key("type");
	json->begin_string_value();
	json_write_type(json, type);
	json->end_string_value();
	
	auto mem_type = interp_get_memory_type(interp, data);
	json->write_key_value("address", "0x%8zx", data);
	json->write_key_value("memory", "%s", memory_type_string(mem_type));
	
	json->write_key("value");
	json_write_value(json, interp, type, data);
	
	json->end_object();
}

static void json_write_symbols(Interpreter *interp, Json_Writer *json, Symbol_Table *symbols, uint64_t stack_top, uint64_t skip_stack_offset)
{
	for (auto symbol : symbols->buffer)
	{
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

	json->write_key_value("procedure", "%s", procedure_name.data);
	json->write_key("variables");
	json->begin_array(true);
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

		json->begin_object(false, 2);

		json->write_key_value("intercept", "procedure_%s", intercept_type);
		json->write_key_value("line_number", "%d", (int)proc->procedure_source_row);

#if 0
		json->write_key_value("procedure_name", "%s", procedure_type->name.data);

		json->write_key("procedure_arguments");

		{
			json->same_line = true;
			Defer { json->same_line = false; };
		
			json->begin_array(true);
			for (int64_t i = 0; i < procedure_type->argument_count; ++i)
			{
				json->begin_string_value();
				json_write_type(json, procedure_type->arguments[i]);
				json->end_string_value();
			}
			json->end_array();
		}

		json->write_key("procedure_return");
		json->begin_string_value();
		json_write_type(json, procedure_type->return_type);
		json->end_string_value();
#endif

		json->write_key("globals");
		json->begin_array(true);
		json_write_symbols(interp, json, interp->global_symbol_table, 0, 0);
		json->end_array();

		json->write_key("callstack");
		json->begin_array(true);
		
		// Don't print the last added calstack before we are already printing it
		for (int64_t index = 0; index < context->callstack.count; ++index)
		{
			auto call = &context->callstack[index];
			json_write_procedure_symbols(interp, json, call->procedure_name, call->symbols, call->stack_top, interp->stack_top);
		}

		json->end_array();

		json->write_key_value("console_out", "%s", context->console_out.get_cstring());

		json->end_object();

		if (intercept == INTERCEPT_PROCEDURE_CALL)
		{
			context->callstack.add(make_procedure_call(procedure_type->name, interp->stack_top, &proc->symbols));
		}
		else
		{
			Assert(intercept == INTERCEPT_PROCEDURE_RETURN);
			context->callstack.remove_last();
		}
	}
	else if (intercept == INTERCEPT_STATEMENT)
	{
		auto statement = (Code_Node_Statement *)node;

		json->begin_object(false, 2);
		
		json->write_key_value("intercept", "statement");
		json->write_key_value("line_number", "%d", (int)statement->source_row);

		json->write_key("globals");
		json->begin_array(true);
		json_write_symbols(interp, json, interp->global_symbol_table, 0, 0);
		json->end_array();

		json->write_key("callstack");
		json->begin_array(true);

		// Don't print the last added calstack before we are already printing it
		for (int64_t index = 0; index < context->callstack.count - 1; ++index)
		{
			auto call = &context->callstack[index];
			json_write_procedure_symbols(interp, json, call->procedure_name, call->symbols, call->stack_top, interp->stack_top);
		}

		json_write_procedure_symbols(interp, json, interp->current_procedure->name, statement->symbol_table, interp->stack_top, UINT64_MAX);

		json->end_array();

		json->write_key_value("console_out", "%s", context->console_out.get_cstring());

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

static void stdout_value(Interpreter *interp, String_Stream &out, Code_Type *type, void *data)
{
	if (!data)
	{
		out.write_fmt("(null)");
		printf("(null)");
		return;
	}

	switch (type->kind)
	{
		case CODE_TYPE_NULL: 
			out.write_fmt("(null)");
			printf("(null)");
			return;
		case CODE_TYPE_CHARACTER: 
			out.write_fmt("%c", *(Kano_Char *)data);
			printf("%c", *(Kano_Char *)data);
			return;
		case CODE_TYPE_INTEGER: 
			out.write_fmt("%zd", *(Kano_Int *)data);
			printf("%zd", *(Kano_Int *)data);
			return;
		case CODE_TYPE_REAL: 
			out.write_fmt("%f", *(Kano_Real *)data);
			printf("%f", *(Kano_Real *)data);
			return;
		case CODE_TYPE_BOOL: 
			out.write_fmt("%s", (*(Kano_Bool *)data) ? "true" : "false");
			printf("%s", (*(Kano_Bool *)data) ? "true" : "false");
			return;
		case CODE_TYPE_PROCEDURE: 
			out.write_fmt("0x%8zx", (uint64_t)data);
			printf("0x%8zx", (uint64_t)data);
			return;

		case CODE_TYPE_POINTER: {
			auto pointer_type = (Code_Type_Pointer *)type;
			void *raw_ptr = *(void **)data;

			out.write_fmt("{ ");
			printf("{ ");

			if (raw_ptr)
			{
				out.write_fmt("raw: 0x%8zx, ", (uint64_t)raw_ptr);
				printf("raw: 0x%8zx, ", (uint64_t)raw_ptr);
			}
			else
			{
				out.write_fmt("raw: (null), ");
				printf("raw: (null), ");
			}

			auto mem_type = interp_get_memory_type(interp, raw_ptr);

			out.write_fmt("value: ");
			printf("value: ");
			if (mem_type != Memory_Type_INVALID)
			{
				stdout_value(interp, out, pointer_type->base_type, raw_ptr);
				out.write_fmt(" ");
				printf(" ");
			}
			else
			{
				out.write_fmt("%s ", raw_ptr ? "(garbage)" : "(invalid)");
				printf("%s ", raw_ptr ? "(garbage)" : "(invalid)");
			}

			out.write_fmt("}");
			printf("}");
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;

			out.write_fmt("{ ");
			printf("{ ");

			for (int64_t index = 0; index < _struct->member_count; ++index)
			{
				auto member = &_struct->members[index];
				out.write_fmt("%s: ", member->name.data);
				printf("%s: ", member->name.data);
				stdout_value(interp, out, member->type, (uint8_t *)data + member->offset);

				if (index < _struct->member_count - 1)
				{
					out.write_fmt(",");
					printf(",");
				}

				out.write_fmt(" ");
				printf(" ");
			}

			out.write_fmt("}");
			printf("}");
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr_type = (Code_Type_Array_View *)type;
			
			auto arr_count = *(Kano_Int *)data;
			auto arr_data = (uint8_t *)data + sizeof(Kano_Int);

			out.write_fmt("[ ");
			printf("[ ");
			for (int64_t index = 0; index < arr_count; ++index)
			{
				stdout_value(interp, out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				out.write_fmt(" ");
			}
			out.write_fmt("]");
			printf("]");

			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr_type = (Code_Type_Static_Array *)type;

			auto arr_data = (uint8_t *)data;

			out.write_fmt("[ ");
			printf("[ ");
			for (int64_t index = 0; index < arr_type->element_count; ++index)
			{
				stdout_value(interp, out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				out.write_fmt(" ");
				printf(" ");
			}
			out.write_fmt("]");
			printf("]");

			return;
		}
	}
}

static void basic_print(Interpreter *interp) {
	Interp_Morph morph(interp);

	auto context = (Interp_User_Context *)interp->user_context;
	auto &con_out = context->console_out;

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
					con_out.write_fmt("\\n");
					printf("\n");
				}
				else if (fmt[index] == '\\')
				{
					con_out.write_fmt("\\\\");
					printf("\\");
					index += 1;
				}
			}
			else
			{
				con_out.write_fmt("\\\\");
				printf("\\");
			}
		}
		else
		{
			con_out.write_fmt("%c", fmt[index]);
			printf("%c", fmt[index]);
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

	FILE *out = fopen("DebugInfo.json", "wb");

	Interp_User_Context context;
	context.json.init(out);

	context.json.begin_array();

	Interpreter interp;
	interp.intercept = intercept;
	interp.user_context = &context;
	interp.global_symbol_table = code_type_resolver_global_symbol_table(resolver);
	interp.heap = new Heap_Allocator;
	interp_init(&interp, resolver, 1024 * 1024 * 4, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	int result = interp_eval_main(&interp);

	context.json.end_array();
	fclose(out);

	return result;
}
