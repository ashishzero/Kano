#include "Common.h"
#include "CodeNode.h"
#include "Parser.h"
#include "Printer.h"

#include <stdlib.h>

String read_entire_file(const char *file) {
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

Unary_Operator_Kind token_to_unary_operator(Token_Kind kind) {
	switch (kind) {
		case TOKEN_KIND_PLUS:  return UNARY_OPERATOR_PLUS;
		case TOKEN_KIND_MINUS: return UNARY_OPERATOR_MINUS;
		NoDefaultCase();
	}

	Unreachable();
	return _UNARY_OPERATOR_COUNT;
}

Binary_Operator_Kind token_to_binary_operator(Token_Kind kind) {
	switch (kind) {
		case TOKEN_KIND_PLUS: return BINARY_OPERATOR_ADD;
		case TOKEN_KIND_MINUS: return BINARY_OPERATOR_SUB;
		case TOKEN_KIND_ASTRICK: return BINARY_OPERATOR_MUL;
		case TOKEN_KIND_DIVISION: return BINARY_OPERATOR_DIV;
		NoDefaultCase();
	}

	Unreachable();
	return _BINARY_OPERATOR_COUNT;
}

template <typename T, uint32_t N>
struct Bucket_Array {
	struct Bucket {
		T data[N];
		Bucket *next = nullptr;
	};

	Bucket first;
	Bucket *last;
	uint32_t index;

	Bucket_Array() {
		last = &first;
		index = 0;
	}

	void add(T d) {
		if (index == N) {
			Bucket *buk = new Bucket;
			index = 0;
			last->next = buk;
			last = buk;
		}

		last->data[index++] = d;
	}

	constexpr uint32_t bucket_size() {
		return N;
	}
};

struct Code_Type_Resolver {
	Bucket_Array<Unary_Operator, 8>  unary_operators[_UNARY_OPERATOR_COUNT];
	Bucket_Array<Binary_Operator, 8> binary_operators[_BINARY_OPERATOR_COUNT];
};

Code_Node *code_resolve(Code_Type_Resolver *resolver, Syntax_Node *root);
Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Syntax_Node_Literal *root);
Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Syntax_Node_Unary_Operator *root);
Code_Node_Binary_Operator *code_resolve_binary_operator(Code_Type_Resolver *resolver, Syntax_Node_Binary_Operator *root);
Code_Node_Expression *code_resolve_expression(Code_Type_Resolver *resolver, Syntax_Node_Expression *root);
Code_Node_Statement *code_resolve_statement(Code_Type_Resolver *resolver, Syntax_Node_Statement *root);
Code_Node_Block *code_resolve_block(Code_Type_Resolver *resolver, Syntax_Node_Block *root);

Code_Node *code_resolve(Code_Type_Resolver *resolver, Syntax_Node *root) {
	switch (root->kind) {
		case SYNTAX_NODE_LITERAL: return code_resolve_literal(resolver, (Syntax_Node_Literal *)root);
		case SYNTAX_NODE_UNARY_OPERATOR: return code_resolve_unary_operator(resolver, (Syntax_Node_Unary_Operator *)root);
		case SYNTAX_NODE_BINARY_OPERATOR: return code_resolve_binary_operator(resolver, (Syntax_Node_Binary_Operator *)root);
		case SYNTAX_NODE_EXPRESSION: return code_resolve_expression(resolver, (Syntax_Node_Expression *)root);
		case SYNTAX_NODE_STATEMENT: return code_resolve_statement(resolver, (Syntax_Node_Statement *)root);
		case SYNTAX_NODE_BLOCK: return code_resolve_block(resolver, (Syntax_Node_Block *)root);

		NoDefaultCase();
	}

	Unimplemented();
	return nullptr;
}

Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Syntax_Node_Literal *root) {
	auto node = new Code_Node_Literal;
	node->type.kind = CODE_TYPE_REAL;
	node->location  = root->location;
	node->value     = root->value;
	return node;
}

Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Syntax_Node_Unary_Operator *root) {
	auto child = code_resolve(resolver, root->child);

	auto op_kind = token_to_unary_operator(root->op);

	auto &operators = resolver->unary_operators[op_kind];

	for (auto buk = &operators.first; buk; buk = buk->next) {
		auto max_count = buk->next ? operators.bucket_size() : operators.index;
		for (uint32_t index = 0; index < max_count; ++index) {
			auto op = &buk->data[index];

			if (op->parameter.kind == child->type.kind) {
				auto node = new Code_Node_Unary_Operator;

				node->type     = op->output;
				node->location = root->location;
				node->child    = child;
				node->op_kind  = op_kind;
				node->op       = op;

				return node;
			}
		}
	}

	Unimplemented();

	return nullptr;
}

Code_Node_Binary_Operator *code_resolve_binary_operator(Code_Type_Resolver *resolver, Syntax_Node_Binary_Operator *root) {
	auto left  = code_resolve(resolver, root->left);
	auto right = code_resolve(resolver, root->right);

	auto op_kind = token_to_binary_operator(root->op);

	auto &operators = resolver->binary_operators[op_kind];

	for (auto buk = &operators.first; buk; buk = buk->next) {
		auto max_count = buk->next ? operators.bucket_size() : operators.index;
		for (uint32_t index = 0; index < max_count; ++index) {
			auto op = &buk->data[index];

			if (op->parameters[0].kind == left->type.kind &&
				op->parameters[1].kind == right->type.kind) {
				auto node = new Code_Node_Binary_Operator;

				node->type     = op->output;
				node->location = root->location;
				node->left     = left;
				node->right    = right;
				node->op_kind  = op_kind;
				node->op       = op;

				return node;
			}
		}
	}

	Unimplemented();

	return nullptr;
}

Code_Node_Expression *code_resolve_expression(Code_Type_Resolver *resolver, Syntax_Node_Expression *root) {
	auto child = code_resolve(resolver, root->child);

	Code_Node_Expression *expression = new Code_Node_Expression;
	expression->location             = root->location;
	expression->child                = child;
	expression->type                 = child->type;

	return expression;
}

Code_Node_Statement *code_resolve_statement(Code_Type_Resolver *resolver, Syntax_Node_Statement *root) {
	auto node = code_resolve(resolver, root->node);

	Code_Node_Statement *statement = new Code_Node_Statement;
	statement->location            = root->location;
	statement->node                = node;
	statement->type                = node->type;

	return statement;
}

Code_Node_Block *code_resolve_block(Code_Type_Resolver *resolver, Syntax_Node_Block *root) {
	Code_Node_Block *block = new Code_Node_Block;
	block->location        = root->location;
	block->type.kind       = CODE_TYPE_NULL;

	Code_Node_Statement statement_stub_head;
	Code_Node_Statement *parent_statement = &statement_stub_head;

	for (auto statement = root->statement_head; statement; statement = statement->next) {
		auto code_statement    = code_resolve_statement(resolver, statement);
		parent_statement->next = code_statement;
		parent_statement       = code_statement;
	}

	block->statement_head  = statement_stub_head.next;
	block->statement_count = root->statement_count;

	return block;
}

int main() {
	String content = read_entire_file("Simple.kano");

	Parser parser;
	parser_init(&parser, content);

	auto node = parse_block(&parser);

	if (parser.error_count) {
		for (auto error = parser.error.first.next; error; error = error->next) {
			auto row     = error->location.start_row;
			auto column  = error->location.start_column;
			auto message = error->message.data;
			fprintf(stderr, "%zu,%zu: %s\n", row, column, message);
		}
		return 1;
	}

	print(node);

	printf("\n\nType Resolution\n");

	Code_Type_Resolver resolver;

	{
		Unary_Operator unary_operator;
		unary_operator.parameter.kind = CODE_TYPE_REAL;
		unary_operator.output.kind    = CODE_TYPE_REAL;

		resolver.unary_operators[UNARY_OPERATOR_PLUS].add(unary_operator);
		resolver.unary_operators[UNARY_OPERATOR_MINUS].add(unary_operator);
	}

	{
		Binary_Operator binary_operator;
		binary_operator.parameters[0].kind = CODE_TYPE_REAL;
		binary_operator.parameters[1].kind = CODE_TYPE_REAL;
		binary_operator.output.kind = CODE_TYPE_REAL;

		resolver.binary_operators[BINARY_OPERATOR_ADD].add(binary_operator);
		resolver.binary_operators[BINARY_OPERATOR_SUB].add(binary_operator);
		resolver.binary_operators[BINARY_OPERATOR_MUL].add(binary_operator);
		resolver.binary_operators[BINARY_OPERATOR_DIV].add(binary_operator);
	}

	auto code = code_resolve(&resolver, node);
	print(code);

	return 0;
}
