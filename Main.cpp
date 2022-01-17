#include "Common.h"

#include "Parser.h"
#include "Resolver.h"
#include "Interp.h"
#include "Printer.h"

// Let's be lazy one in a while
#include <sstream>

using String_Stream = std::stringstream;

struct Call_Info {
	String procedure_name;
	uint64_t stack_top;
	Symbol_Table *symbols;
};

struct Interp_User_Context {
	String_Stream console_out;
	FILE *debug_json;

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

void print_type(FILE *out, Code_Type *type)
{
	switch (type->kind)
	{
		case CODE_TYPE_NULL: fprintf(out, "void"); return;
		case CODE_TYPE_INTEGER: fprintf(out, "int"); return;
		case CODE_TYPE_REAL: fprintf(out, "float"); return;
		case CODE_TYPE_BOOL: fprintf(out, "bool"); return;

		case CODE_TYPE_POINTER: {
			fprintf(out, "*"); 
			print_type(out, ((Code_Type_Pointer *)type)->base_type);
			return;
		}

		case CODE_TYPE_PROCEDURE: {
			auto proc = (Code_Type_Procedure *)type;
			fprintf(out, "proc (");
			for (int64_t index = 0; index < proc->argument_count; ++index)
			{
				print_type(out, proc->arguments[index]);
				if (index < proc->argument_count - 1) fprintf(out, ", ");
			}
			fprintf(out, ")");

			if (proc->return_type)
			{
				fprintf(out, " -> ");
				print_type(out, proc->return_type);
			}
			return;
		}

		case CODE_TYPE_STRUCT: {
			auto strt = (Code_Type_Struct *)type;
			fprintf(out, "%s", strt->name.data); 
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr = (Code_Type_Array_View *)type;
			fprintf(out, "[] ");
			print_type(out, arr->element_type);
			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr = (Code_Type_Static_Array *)type;
			fprintf(out, "[%u] ", arr->element_count);
			print_type(out, arr->element_type);
			return;
		}
	}
}

void print_value(FILE *out, Code_Type *type, void *data)
{
	switch (type->kind)
	{
		case CODE_TYPE_NULL: fprintf(out, "null"); return;
		case CODE_TYPE_INTEGER: fprintf(out, "%zd", *(Kano_Int *)data); return;
		case CODE_TYPE_REAL: fprintf(out, "%f", *(Kano_Real *)data); return;
		case CODE_TYPE_BOOL: fprintf(out, "%s", (*(Kano_Bool *)data) ? "true" : "false"); return;

		case CODE_TYPE_POINTER: {
			auto pointer = (Code_Type_Pointer *)type;
			fprintf(out, "%p { ", data);
			print_value(out, pointer->base_type, *(uint8_t **)data);
			fprintf(out, " }");
			return;
		}

		case CODE_TYPE_PROCEDURE: fprintf(out, "%p", data); return;

		case CODE_TYPE_STRUCT: {
			auto _struct = (Code_Type_Struct *)type;
			fprintf(out, "{ ");
			
			for (int64_t index = 0; index < _struct->member_count; ++index)
			{
				auto member = &_struct->members[index];
				fprintf(out, "%s: ", member->name.data);
				print_value(out, member->type, (uint8_t *)data + member->offset);
				if (index < _struct->member_count - 1)
					fprintf(out, ", ");
			}

			fprintf(out, " }");
			return;
		}

		case CODE_TYPE_ARRAY_VIEW: {
			auto arr_type = (Code_Type_Array_View *)type;
			
			auto arr_count = *(Kano_Int *)data;
			auto arr_data = (uint8_t *)data + sizeof(Kano_Int);
			
			fprintf(out, "[ ");
			for (int64_t index = 0; index < arr_count; ++index)
			{
				print_value(out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				if (index < arr_count - 1)
					fprintf(out, ", ");
			}
			fprintf(out, " ]");

			return;
		}

		case CODE_TYPE_STATIC_ARRAY: {
			auto arr_type = (Code_Type_Static_Array *)type;

			auto arr_data = (uint8_t *)data;

			fprintf(out, "[ ");
			for (int64_t index = 0; index < arr_type->element_count; ++index)
			{
				print_value(out, arr_type->element_type, arr_data + index * arr_type->element_type->runtime_size);
				if (index < arr_type->element_count - 1)
					fprintf(out, ", ");
			}
			fprintf(out, " ]");

			return;
		}
	}
}

static void string_replace_all(std::string &s, const String search, const String replace) {
	for (size_t pos = 0; ; pos += replace.length) {
		pos = s.find((char *)search.data, pos);
		if (pos == std::string::npos) break;
		s.erase(pos, search.length);
		s.insert(pos, (char *)replace.data);
	}
}

static void print_symbols(Interpreter *interp, FILE *out, Symbol_Table *symbols, uint64_t stack_top, uint64_t skip_stack_offset)
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
			data = symbol->address.ccall;
		else
			Unreachable();
		
		fprintf(out, "\t\t{ \"name\" : \"%s\", ", symbol->name.data);
		fprintf(out, "\"type\" : \"");
		print_type(out, symbol->type);
		fprintf(out, "\", \"value\" : \"");
		print_value(out, symbol->type, data);
		fprintf(out, "\"},\n");
	}
}

static void print_all_symbols_until_parent_is_reached(Interpreter *interp, FILE *out, Symbol_Table *symbol_table, uint64_t stack_top, uint64_t skip_stack_offset)
{
	for (auto symbols = symbol_table; symbols != interp->global_symbol_table; symbols = symbols->parent)
	{
		print_symbols(interp, out, symbols, stack_top, skip_stack_offset);
	}
}

static void print_symbols_from_procedure(Interpreter *interp, FILE *out, String procedure_name, Symbol_Table *symbol_table, uint64_t stack_top, uint64_t skip_stack_offset)
{
	fprintf(out, "\t{\n");

	fprintf(out, "\t\"procedure\" : \"%s\",\n", procedure_name.data); 
	fprintf(out, "\t\"variables\" : [\n");
	print_all_symbols_until_parent_is_reached(interp, out, symbol_table, stack_top, skip_stack_offset);
	fprintf(out, "\t      ], \n");
	
	fprintf(out, "\t},\n");
}

void intercept(Interpreter *interp, Intercept_Kind intercept, Code_Node *node)
{
	auto context = (Interp_User_Context *)interp->user_context;

	auto out = context->debug_json;

	if (intercept == INTERCEPT_PROCEDURE_CALL)
	{
		auto proc = (Code_Node_Block *)node;
		auto procedure_type = interp->current_procedure;

		fprintf(out, "{\n\"intercept\": \"procedure_call\",\n\"line_number\" : ");
		fprintf(out, "\"");
		fprintf(out, "%zu", proc->procedure_source_row);
		fprintf(out, "\",\n");
		fprintf(out, "\"procedure_name\" : \"%s\",\n", procedure_type->name.data);

		fprintf(out, "\"procedure_type\" : {\n");	
		fprintf(out, "\t\t\"arguments\" : [ ");
		for (int64_t i = 0; i < procedure_type->argument_count; ++i) {
			if (i != 0)
				fprintf(out, ",");
			fprintf(out, "\"");
			print_type(out, procedure_type->arguments[i]);
			fprintf(out, "\"");
		}
		fprintf(out, " ], \n\t\t\"return\": ");
		if (procedure_type->return_type == NULL)
			fprintf(out, "\"void\" \n\t\t   },\n");
		else {
			fprintf(out, "\"");
			print_type(out, procedure_type->return_type);
			fprintf(out, "\" \n\t\t   },\n");
		}

		fprintf(out, "},\n\n");

		context->callstack.add(make_procedure_call(procedure_type->name, interp->stack_top, &proc->symbols));
	}
	else if (intercept == INTERCEPT_PROCEDURE_RETURN)
	{
		auto proc = (Code_Node_Block *)node;
		auto procedure_type = interp->current_procedure;

		fprintf(out, "{\n\"intercept\": \"return\",\n\"line_number\" : ");
		fprintf(out, "\"");
		fprintf(out, "%zu", proc->procedure_source_row);
		fprintf(out, "\",\n");
		fprintf(out, "\"procedure_name\" : \"%s\",\n", procedure_type->name.data);

		fprintf(out, "\"procedure_type\" : {\n");	
		fprintf(out, "\t\t\"arguments\" : [ ");
		for (int64_t i = 0; i < procedure_type->argument_count; ++i) {
			if (i != 0)
				fprintf(out, ",");
			fprintf(out, "\"");
			print_type(out, procedure_type->arguments[i]);
			fprintf(out, "\"");
		}
		fprintf(out, " ], \n\t\t\"return\": ");
		if (procedure_type->return_type == NULL)
			fprintf(out, "\"void\" \n\t\t   },\n");
		else {
			fprintf(out, "\"");
			print_type(out, procedure_type->return_type);
			fprintf(out, "\" \n\t\t   },\n");
		}
		
		fprintf(out, "\n},\n");

		context->callstack.remove_last();
	}
	else if (intercept == INTERCEPT_STATEMENT)
	{
		auto statement = (Code_Node_Statement *)node;
		fprintf(out, "{\n\"intercept\": \"statement\",\n\"line_number\" : ");
		fprintf(out, "\"");
		fprintf(out, "%zu", statement->source_row);
		fprintf(out, "\",\n");

		fprintf(out, "\"globals\" : [\n");
		print_symbols(interp, out, interp->global_symbol_table, 0, 0);
		fprintf(out, "\t      ], \n");


		fprintf(out, "\"callstack\" : [\n");

		print_symbols_from_procedure(interp, out, interp->current_procedure->name, statement->symbol_table, interp->stack_top, UINT64_MAX);

		// Don't print the last added calstack before we are already printing it
		for (int64_t index = context->callstack.count - 2; index >= 0; --index) {
			auto call = &context->callstack[index];
			print_symbols_from_procedure(interp, out, call->procedure_name, call->symbols, call->stack_top, interp->stack_top);
		}

		fprintf(out, "\t      ], \n");

		{
			auto &con_out = context->console_out;
			auto buffer = con_out.rdbuf();
			auto string = buffer->str();

			string_replace_all(string, "\n", "\\n");
			fprintf(out, "\"console_out\": \"%s\"\n", string.data());
		}

		fprintf(out, "},\n\n");
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
			if (index < fmt.length)
			{
				if (fmt[index] == 'd')
				{
					index += 1;
					auto value = (Kano_Int *)(args);
					con_out << *value;
					args += sizeof(Kano_Int);
				}
				else if (fmt[index] == 'f')
				{
					index += 1;
					auto value = (Kano_Real *)(args);
					con_out << *value;
					args += sizeof(Kano_Real);
				}
				else if (fmt[index] == 'b')
				{
					index += 1;
					auto value = (Kano_Bool *)(args);
					con_out << (*value ? "true" : "false");
					args += sizeof(Kano_Bool);
				}
				else if (fmt[index] == '%')
				{
					con_out << "%";
					index += 1;
				}
			}
			else
			{
				con_out << "%";
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
					con_out << "\n";
				}
				else if (fmt[index] == '\\')
				{
					con_out << "\\";
					index += 1;
				}
			}
			else
			{
				con_out << "\\";
			}
		}
		else
		{
			con_out << (char)fmt[index];
			index += 1;
		}
	}

	auto buffer = con_out.rdbuf();
	auto string = buffer->str();
	printf("%s", string.data());
}

static void basic_allocate(Interpreter *interp) {
	Interp_Morph morph(interp);
	morph.OffsetReturn<void *>();
	auto size = morph.Arg<Kano_Int>();
	auto result = malloc(size);
	morph.Return(result);
}

static void basic_free(Interpreter *interp) {
	Interp_Morph morph(interp);
	auto ptr = morph.Arg<void *>();
	free(ptr);
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
	fprintf(out, "[\n");

	Interp_User_Context context;
	context.debug_json = out;

	Interpreter interp;
	interp.intercept = intercept;
	interp.user_context = &context;
	interp.global_symbol_table = code_type_resolver_global_symbol_table(resolver);
	interp_init(&interp, 1024 * 1024 * 4, code_type_resolver_bss_allocated(resolver));

	interp_eval_globals(&interp, exprs);
	int result = interp_eval_main(&interp, resolver);

	fprintf(out, "]\n");
	fclose(out);

	return result;
}
