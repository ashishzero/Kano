#include "Kr/KrCommon.h"
#include "Interp.h"
#include "StringBuilder.h"
#include "HeapAllocator.h"
#include "JsonWriter.h"
#include "Kr/KrString.h"
#pragma once
#include "Resolver.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

struct Call_Info {
	String procedure_name;
	uint64_t stack_top;
	Symbol_Table *symbols;
};

struct Interp_User_Context {
	String_Builder   console_out;
	String           console_in;
	Json_Writer      json;
	Array<Call_Info> callstack;
	clock_t          prev_count;
	clock_t          first_count;
};

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

struct Interp_Morph {
	uint8_t *arg;
	uint64_t offset;
	Interp_Morph(Interpreter *interp) : arg(interp->stack + interp->stack_top), offset(0) {}

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

static void stdout_value(Interpreter *interp, String_Builder *sink, Code_Type *type, void *data)
{
	if (!data)
	{
		if (sink) Write(sink, "(null)"); 
		printf("(null)");
		return;
	}

	switch (type->kind)
	{
	case CODE_TYPE_NULL:
		if (sink) Write(sink, "(null)"); 
		printf("(null)");
		return;
	case CODE_TYPE_CHARACTER:
		if (sink) Write(sink, (int)*(Kano_Char *)data);
		printf("%d", (int)*(Kano_Char *)data);
		return;
	case CODE_TYPE_INTEGER:
		if (sink) Write(sink, *(Kano_Int *)data);
		printf("%zd", *(Kano_Int *)data);
		return;
	case CODE_TYPE_REAL:
		if (sink) Write(sink, *(Kano_Real *)data);
		printf("%f", *(Kano_Real *)data);
		return;
	case CODE_TYPE_BOOL:
		if (sink) Write(sink, (*(Kano_Bool *)data));
		printf("%s", (*(Kano_Bool *)data) ? "true" : "false");
		return;
	case CODE_TYPE_PROCEDURE:
		if (sink) WriteFormatted(sink, "0x%ll", data);
		printf("%p", data);
		return;

	case CODE_TYPE_POINTER: {
		auto pointer_type = (Code_Type_Pointer *)type;
		void *raw_ptr = *(void **)data;

		if (sink) Write(sink, "{ ");
		printf("{ ");

		if (raw_ptr)
		{
			if (sink) WriteFormatted(sink, "raw: %, ", raw_ptr);
			printf("raw: %p, ", raw_ptr);
		} else
		{
			if (sink) Write(sink, "raw: (null), ");
			printf("raw: (null), ");
		}

		auto mem_type = interp_get_memory_type(interp, raw_ptr);

		if (sink) Write(sink, "value: ");
		printf("value: ");

		if (mem_type != Memory_Type_INVALID)
		{
			stdout_value(interp, sink, pointer_type->base_type, raw_ptr);
			if (sink) Write(sink, " "); printf(" ");
		} else
		{
			if (sink) Write(sink, raw_ptr ? String("(garbage)") : String("(invalid)"));
			printf("%s ", raw_ptr ? "(garbage)" : "(invalid)");
		}

		if (sink) Write(sink, "}");
		printf("}");
		return;
	}

	case CODE_TYPE_STRUCT: {
		auto _struct = (Code_Type_Struct *)type;

		if (sink) Write(sink, "{ ");
		printf("{ ");

		for (int64_t index = 0; index < _struct->member_count; ++index)
		{
			auto member = &_struct->members[index];
			if (sink) Write(sink, member->name);
			printf("%.*s: ", (int)member->name.length, member->name.data);
			stdout_value(interp, sink, member->type, (uint8_t *)data + member->offset);

			if (index < _struct->member_count - 1)
			{
				if (sink) Write(sink, ",");
				printf(",");
			}

			if (sink) Write(sink, " ");
			printf(" ");
		}

		if (sink) Write(sink, "}");
		printf("}");
		return;
	}

	case CODE_TYPE_ARRAY_VIEW: {
		auto arr_type = (Code_Type_Array_View *)type;

		auto arr_count = *(Kano_Int *)data;
		auto arr_data = (uint8_t *)data + sizeof(Kano_Int);

		if (sink) Write(sink, "[ ");
		printf("[ ");
		for (int64_t index = 0; index < arr_count; ++index)
		{
			stdout_value(interp, sink, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
			if (sink) Write(sink, " ");
			printf(" ");
		}
		if (sink) Write(sink, "]");
		printf("]");

		return;
	}

	case CODE_TYPE_STATIC_ARRAY: {
		auto arr_type = (Code_Type_Static_Array *)type;

		auto arr_data = (uint8_t *)data;

		if (sink) Write(sink, "[ ");
		printf("[ ");
		for (int64_t index = 0; index < arr_type->element_count; ++index)
		{
			stdout_value(interp, sink, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
			if (sink) Write(sink, " ");
			printf(" ");
		}
		if (sink) Write(sink, "]");
		printf("]");

		return;
	}
	}
}

static void basic_print(Interpreter *interp) {
	Interp_Morph morph(interp);

#ifdef KANO_SERVER
	auto context = (Interp_User_Context *)interp->user_context;
	auto cout    = &context->console_out;
#else
	String_Builder *cout = nullptr;
#endif

	auto fmt = morph.Arg<String>(sizeof(int64_t));
	auto args = morph.Arg<uint8_t *>();

	for (int64_t index = 0; index < fmt.length;)
	{
		if (fmt[index] == '%')
		{
			index += 1;

			if (args) {
				auto type = *(Code_Type **)args;
				args += sizeof(Code_Type *);
				auto ptr = args;
				args += type->runtime_size;

				if (ptr >= interp->stack &&
					(ptr < interp->stack + interp->stack_top) &&
					interp_get_memory_type(interp, ptr) != Memory_Type_INVALID) {
					stdout_value(interp, cout, type, ptr);
				} else {
					if (cout) Write(cout, '%'); printf("%%");
				}
			} else {
				if (cout) Write(cout, '%'); printf("%%");
			}
		} else if (fmt[index] == '\\')
		{
			index += 1;
			if (index < fmt.length)
			{
				if (fmt[index] == 'n')
				{
					index += 1;
					if (cout) Write(cout, "\\n"); printf("\n");
				} else if (fmt[index] == '\\')
				{
					if (cout) Write(cout, "\\\\"); printf("\\");
					index += 1;
				}
			} else
			{
				if (cout) Write(cout, "\\\\"); printf("\\");
			}
		} else
		{
			if (cout) Write(cout, (char)fmt[index]); printf("%c", fmt[index]);
			index += 1;
		}
	}
}

static void basic_read_int(Interpreter *interp)
{
	Interp_Morph morph(interp);
	morph.OffsetReturn<Kano_Int>();

	Kano_Int result = 0;

#ifdef KANO_SERVER
	auto context = (Interp_User_Context *)interp->user_context;
	auto input = StrTrim(context->console_in);
	if (input.data && input.length)
	{
		char *end = nullptr;
		result = (Kano_Int)strtoll((char *)input.data, &end, 10);
		input.length -= (end - (char *)input.data);
		input.data = (uint8_t *)end;

		Write(&context->console_out, result);
		WriteFormatted(&context->console_out, "\\n");
		printf("%d\n", (int)result);
	} else
	{
		Write(&context->console_out, "Failed read_int: Input buffer empty\\n");
		printf("Failed read_int: Input buffer empty\n");
	}

	context->console_in = input;
#else
	int value = 0;
	scanf("%d", &value);
	result = value;
#endif

	morph.Return(result);
}

static void basic_read_float(Interpreter *interp)
{
	Interp_Morph morph(interp);
	morph.OffsetReturn<Kano_Real>();

	Kano_Real result = 0;

#ifdef KANO_SERVER
	auto context = (Interp_User_Context *)interp->user_context;
	auto input = StrTrim(context->console_in);

	if (input.data && input.length)
	{
		char *end = nullptr;
		result = (Kano_Real)strtod((char *)input.data, &end);
		input.length -= (end - (char *)input.data);
		input.data = (uint8_t *)end;
		Write(&context->console_out, result);
		WriteFormatted(&context->console_out, "\\n");
		printf("%f\n", (double)result);
	} else
	{
		Write(&context->console_out, "Failed read_float: Input buffer empty\\n");
		printf("Failed read_float: Input buffer empty\n");
	}

	context->console_in = input;
#else
	double value = 0;
	scanf("%f", &value);
	result = value;
#endif

	morph.Return(result);
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

static void basic_va_arg(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<void *>();
	auto x = morph.Arg<uint8_t *>();
	x += (sizeof(Code_Type *));
	morph.Return(x);
}

static void basic_va_arg_next(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<void *>();
	auto x = morph.Arg<uint8_t *>();
	auto type = *(Code_Type **)x;
	x += (type->runtime_size + sizeof(Code_Type *));
	morph.Return(x);
}

static void include_basic(Code_Type_Resolver *resolver)
{
	Procedure_Builder builder(resolver);

	proc_builder_argument(&builder, "string");
	proc_builder_variadic(&builder);
	proc_builder_register(&builder, "print", basic_print);

	proc_builder_return(&builder, "int");
	proc_builder_register(&builder, "read_int", basic_read_int);

	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "read_float", basic_read_float);

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

	proc_builder_argument(&builder, "*void");
	proc_builder_return(&builder, "*void");
	proc_builder_register(&builder, "va_arg_next", basic_va_arg_next);

	proc_builder_argument(&builder, "*void");
	proc_builder_return(&builder, "*void");
	proc_builder_register(&builder, "va_arg", basic_va_arg);

	proc_builder_free(&builder);
}
