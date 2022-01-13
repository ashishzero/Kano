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

#include <math.h>

struct Interp_Proc_Arg
{
	uint8_t *arg;

	Interp_Proc_Arg(Interpreter *interp) 
	{ 
		arg = interp->stack + interp->stack_top; 
	}

	template <typename T> T deserialize_arg()
	{ 
		T value = *(T *)arg; 
		arg += sizeof(T); 
		return value;
	}

	template <typename ReturnType, typename... ArgumentTypes>
	void execute(ReturnType(*proc)(ArgumentTypes...))
	{
		auto ptr = arg;
		arg += sizeof(ReturnType);
		ReturnType result = proc(deserialize_arg<ArgumentTypes>()...);
		memcpy(ptr, &result, sizeof(ReturnType));
	}

	template <typename... ArgumentTypes>
	void execute(void(*proc)(ArgumentTypes...))
	{
		proc(deserialize_arg<ArgumentTypes>()...);
	}
};

#define InterpMorphProc(proc) \
	[](Interpreter *interp) { \
		Interp_Proc_Arg arg(interp); \
		arg.execute(proc); \
	}

struct Procedure_Builder
{
	Code_Type_Resolver *resolver = nullptr;
	Array<Code_Type *> arguments;
	Code_Type *return_type = nullptr;
	bool is_variadic = false;

	Procedure_Builder(Code_Type_Resolver *type_resolver = nullptr)
	{
		resolver = type_resolver;
	}
};

void proc_builder_argument(Procedure_Builder *builder, String name)
{
	Assert(builder->is_variadic == false);
	auto type = code_type_resolver_find_type(builder->resolver, name);
	Assert(type);
	builder->arguments.add(type);
}

void proc_builder_variadic(Procedure_Builder *builder)
{
	Assert(builder->is_variadic == false);
	builder->is_variadic = true;
	auto type = code_type_resolver_find_type(builder->resolver, "*void");
	Assert(type);
	builder->arguments.add(type);
}

void proc_builder_return(Procedure_Builder *builder, String name)
{
	Assert(builder->return_type == nullptr);
	auto type = code_type_resolver_find_type(builder->resolver, name);
	Assert(type);
	builder->return_type = type;
}

void proc_builder_register(Procedure_Builder *builder, String name, CCall ccall)
{
	auto type = new Code_Type_Procedure;
	type->argument_count = builder->arguments.count;
	type->arguments = new Code_Type *[type->argument_count];
	memcpy(type->arguments, builder->arguments.data, sizeof(type->arguments[0]) * type->argument_count);
	type->return_type = builder->return_type;
	type->is_variadic = builder->is_variadic;
	
	Assert(code_type_resolver_register_ccall(builder->resolver, name, ccall, type));

	builder->arguments.reset();
	builder->return_type = nullptr;
	builder->is_variadic = false;
}

void proc_builder_free(Procedure_Builder *builder)
{
	array_free(&builder->arguments);
	builder->return_type = nullptr;
	builder->resolver = nullptr;
}

//
//
//

static void print_hello(String str, String args)
{
	printf("%s %s\n", str.data, args.data);
}

static void basic_print(Interpreter *interp)
{
	auto arg = InterpProcStart(interp);
	auto fmt = InterpProcNext(arg, String);
	auto args = InterpProcNext(arg, uint8_t *);
	
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

void include_basic(Code_Type_Resolver *resolver)
{
	Procedure_Builder builder(resolver);
	
	proc_builder_argument(&builder, "string");
	proc_builder_variadic(&builder);
	proc_builder_register(&builder, "print", basic_print);

	proc_builder_argument(&builder, "string");
	proc_builder_argument(&builder, "string");
	//proc_builder_variadic(&builder);
	proc_builder_register(&builder, "print_magic", InterpMorphProc(print_hello));

	proc_builder_argument(&builder, "int");
	proc_builder_return(&builder, "*void");
	proc_builder_register(&builder, "allocate", InterpMorphProc(basic_allocate));

	proc_builder_argument(&builder, "*void");
	proc_builder_register(&builder, "free", InterpMorphProc(basic_free));

	proc_builder_argument(&builder, "float");
	proc_builder_return(&builder, "float");
	proc_builder_register(&builder, "sin", InterpMorphProc(basic_sin));

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

	print("syntax.txt", node);

	auto resolver = code_type_resolver_create();

	include_basic(resolver);

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
