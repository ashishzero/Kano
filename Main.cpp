#include "Common.h"
#include "CodeNode.h"
#include "Parser.h"
#include "Printer.h"
#include "Interp.h"
#include <iostream>

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

static inline uint32_t murmur_32_scramble(uint32_t k) {
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return k;
}
uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed) {
	uint32_t h = seed;
	uint32_t k;
	/* Read in groups of 4. */
	for (size_t i = len >> 2; i; i--) {
		// Here is a source of differing results across endiannesses.
		// A swap here has no effects on hash properties though.
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= murmur_32_scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
	}
	/* Read the rest. */
	k = 0;
	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	// A swap is *not* necessary here because the preceding loop already
	// places the low bytes in the low places according to whatever endianness
	// we use. Swaps only apply when the memory is copied in a chunk.
	h ^= murmur_32_scramble(k);
	/* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
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

struct Symbol {
	String          name;
	Code_Type       type;
	uint32_t        flags;
	uint32_t        address;
	Syntax_Location location;
};


constexpr uint32_t SYMBOL_INDEX_BUCKET_SIZE  = 16;
constexpr uint32_t SYMBOL_INDEX_MASK         = SYMBOL_INDEX_BUCKET_SIZE - 1;
constexpr uint32_t SYMBOL_INDEX_SHIFT        = 4;
constexpr uint32_t SYMBOL_TABLE_BUCKET_COUNT = 128;
constexpr uint32_t HASH_SEED                = 0x2564;

struct Symbol_Index {
	uint32_t hash[SYMBOL_INDEX_BUCKET_SIZE]  = {};
	uint32_t index[SYMBOL_INDEX_BUCKET_SIZE] = {};
	Symbol_Index *next                       = nullptr;
};

struct Symbol_Lookup {
	Symbol_Index  buckets[SYMBOL_TABLE_BUCKET_COUNT];
};

struct Symbol_Table {
	Symbol_Lookup lookup;
	Array<Symbol> buffer;
};

static inline uint32_t next_power2(uint32_t n) {
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;
	return n;
}

static uint32_t symbol_table_put(Symbol_Table *table, const Symbol &sym) {
	String name = sym.name;
	auto hash = murmur3_32(name.data, name.length, HASH_SEED) + 1;

	auto pos       = hash & (SYMBOL_TABLE_BUCKET_COUNT - 1);
	auto buk_index = pos >> SYMBOL_INDEX_SHIFT;

	for (auto bucket = &table->lookup.buckets[buk_index]; bucket; bucket = bucket->next) {
		uint32_t count = 0;
		for (auto index = pos & SYMBOL_INDEX_MASK;
			count < SYMBOL_INDEX_BUCKET_SIZE;
			++count, index = (index + 1) & SYMBOL_INDEX_MASK) {
			auto found_hash = bucket->hash[index];
			if (found_hash == hash) {
				return bucket->index[index];
			}
			else if (found_hash == 0) {
				uint32_t offset = (uint32_t)table->buffer.count;
				bucket->hash[index] = hash;
				bucket->index[index] = offset;
				table->buffer.add(sym);
				return offset;
			}
		}

		if (!bucket->next) {
			bucket->next = new Symbol_Index;
		}
	}

	return 0;
}

static uint32_t symbol_table_get(Symbol_Table *table, String name) {
	auto hash = murmur3_32(name.data, name.length, HASH_SEED) + 1;

	auto pos = hash & (SYMBOL_TABLE_BUCKET_COUNT - 1);
	auto buk_index = pos >> SYMBOL_INDEX_SHIFT;

	for (auto bucket = &table->lookup.buckets[buk_index]; bucket; bucket = bucket->next) {
		uint32_t count = 0;
		for (auto index = pos & SYMBOL_INDEX_MASK;
			count < SYMBOL_INDEX_BUCKET_SIZE;
			++count, index = (index + 1) & SYMBOL_INDEX_MASK) {
			auto found_hash = bucket->hash[index];
			if (found_hash == hash) {
				return bucket->index[index];
			}
		}
	}

	return 0;
}

static inline uint32_t align(uint32_t location, uint32_t alignment) {
	return ((location + (alignment - 1)) & ~(alignment - 1));
}

struct Code_Type_Resolver {
	Symbol_Table symbols;
	uint32_t     vstack = 0;

	Bucket_Array<Unary_Operator, 8>  unary_operators[_UNARY_OPERATOR_COUNT];
	Bucket_Array<Binary_Operator, 8> binary_operators[_BINARY_OPERATOR_COUNT];
};

Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Syntax_Node_Literal *root);

Code_Node *code_resolve_expression(Code_Type_Resolver *resolver, Syntax_Node *root);
Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Syntax_Node_Unary_Operator *root);
Code_Node_Binary_Operator *code_resolve_binary_operator(Code_Type_Resolver *resolver, Syntax_Node_Binary_Operator *root);
Code_Node_Assignment *code_resolve_assignment(Code_Type_Resolver *resolver, Syntax_Node_Assignment *root);

Code_Node_Expression *code_resolve_root_expression(Code_Type_Resolver *resolver, Syntax_Node_Expression *root);
Code_Type code_resolve_type(Code_Type_Resolver *resolver, Syntax_Node_Type *root);
void code_resolve_declaration(Code_Type_Resolver *resolver, Syntax_Node_Declaration *root);

Code_Node_Statement *code_resolve_statement(Code_Type_Resolver *resolver, Syntax_Node_Statement *root);
Code_Node_Block *code_resolve_block(Code_Type_Resolver *resolver, Syntax_Node_Block *root);


Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Syntax_Node_Literal *root) {
	auto node = new Code_Node_Literal;
	node->type.kind = CODE_TYPE_REAL;
	node->value     = root->value;
	return node;
}

Code_Node_Stack *code_resolve_identifier(Code_Type_Resolver *resolver, Syntax_Node_Identifier *root) {
	uint32_t symbol_index = symbol_table_get(&resolver->symbols, root->name);

	if (symbol_index) {
		auto symbol = &resolver->symbols.buffer[symbol_index];

		auto stack = new Code_Node_Stack;
		stack->offset = symbol->address;
		stack->type = symbol->type;
		return stack;
	}

	Unimplemented();
	return nullptr;
}

Code_Node *code_resolve_expression(Code_Type_Resolver *resolver, Syntax_Node *root) {
	switch (root->kind) {
	case SYNTAX_NODE_LITERAL:
		return code_resolve_literal(resolver, (Syntax_Node_Literal *)root);

	case SYNTAX_NODE_IDENTIFIER:
		return code_resolve_identifier(resolver, (Syntax_Node_Identifier *)root);

	case SYNTAX_NODE_UNARY_OPERATOR:
		return code_resolve_unary_operator(resolver, (Syntax_Node_Unary_Operator *)root);

	case SYNTAX_NODE_BINARY_OPERATOR:
		return code_resolve_binary_operator(resolver, (Syntax_Node_Binary_Operator *)root);

	case SYNTAX_NODE_ASSIGNMENT:
		return code_resolve_assignment(resolver, (Syntax_Node_Assignment *)root);

	NoDefaultCase();
	}
	return nullptr;
}

Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Syntax_Node_Unary_Operator *root) {
	auto child = code_resolve_expression(resolver, root->child);

	auto op_kind = token_to_unary_operator(root->op);

	auto &operators = resolver->unary_operators[op_kind];

	for (auto buk = &operators.first; buk; buk = buk->next) {
		auto max_count = buk->next ? operators.bucket_size() : operators.index;
		for (uint32_t index = 0; index < max_count; ++index) {
			auto op = &buk->data[index];

			if (op->parameter.kind == child->type.kind) {
				auto node = new Code_Node_Unary_Operator;

				node->type     = op->output;
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
	auto left  = code_resolve_expression(resolver, root->left);
	auto right = code_resolve_expression(resolver, root->right);

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

Code_Node_Destination *code_resolve_destination(Code_Type_Resolver *resolver, Syntax_Node *root) {
	switch (root->kind) {
		case SYNTAX_NODE_IDENTIFIER:
		{
			auto stack  = code_resolve_identifier(resolver, (Syntax_Node_Identifier *)root);

			auto dest   = new Code_Node_Destination;
			dest->child = stack;
			dest->type  = stack->type;
			return dest;
		} break;

		NoDefaultCase();
	}

	Unimplemented();
	return nullptr;
}

Code_Node_Assignment *code_resolve_assignment(Code_Type_Resolver *resolver, Syntax_Node_Assignment *root) {
	auto destination = code_resolve_destination(resolver, root->left);
	auto value       = code_resolve_root_expression(resolver, root->right);

	if (destination->type.kind == value->type.kind) {
		auto assignment         = new Code_Node_Assignment;
		assignment->destination = destination;
		assignment->value       = value;
		assignment->type        = destination->type;
		return assignment;
	}

	Unimplemented();
	return nullptr;
}

Code_Node_Expression *code_resolve_root_expression(Code_Type_Resolver *resolver, Syntax_Node_Expression *root) {
	auto child = code_resolve_expression(resolver, root->child);

	Code_Node_Expression *expression = new Code_Node_Expression;
	expression->child                = child;
	expression->type                 = child->type;

	return expression;
}

Code_Type code_resolve_type(Code_Type_Resolver *resolver, Syntax_Node_Type *root) {
	if (root->syntax_type == SYNTAX_TYPE_FLOAT) {
		Code_Type type;
		type.kind = CODE_TYPE_REAL;
		return type;
	}

	Unreachable();
	return Code_Type{};
}

void code_resolve_declaration(Code_Type_Resolver *resolver, Syntax_Node_Declaration *root) {
	String sym_name = root->identifier;

	if (symbol_table_get(&resolver->symbols, sym_name) == 0) {
		Symbol symbol;
		symbol.name     = sym_name;
		symbol.type     = code_resolve_type(resolver, root->type);
		symbol.flags    = root->flags;
		symbol.location = root->location;

		if (symbol.flags & DECLARATION_IS_CONSTANT) {
			symbol.address = UINT32_MAX;
		}
		else {
			Assert(symbol.type.kind == CODE_TYPE_REAL);
			uint32_t alignment = align(resolver->vstack, sizeof(float));
			symbol.address = resolver->vstack + alignment;
			resolver->vstack += alignment + sizeof(float);
		}

		symbol_table_put(&resolver->symbols, symbol);
		return;
	}

	// Already defined in this scope previously
	Unimplemented();
}

Code_Node_Statement *code_resolve_statement(Code_Type_Resolver *resolver, Syntax_Node_Statement *root) {
	auto node = root->node;

	switch (node->kind) {
		case SYNTAX_NODE_EXPRESSION:
		{
			auto expression = code_resolve_root_expression(resolver, (Syntax_Node_Expression *)node);
			Code_Node_Statement *statement = new Code_Node_Statement;
			statement->node     = expression;
			statement->type     = expression->type;
			return statement;
		} break;

		case SYNTAX_NODE_DECLARATION:
		{
			code_resolve_declaration(resolver, (Syntax_Node_Declaration *)node);
			return nullptr;
		} break;

		NoDefaultCase();
	}

	return nullptr;
}

Code_Node_Block *code_resolve_block(Code_Type_Resolver *resolver, Syntax_Node_Block *root) {
	Code_Node_Block *block = new Code_Node_Block;
	block->type.kind       = CODE_TYPE_NULL;

	Code_Node_Statement statement_stub_head;
	Code_Node_Statement *parent_statement = &statement_stub_head;

	uint32_t statement_count = 0;
	for (auto statement = root->statement_head; statement; statement = statement->next) {
		auto code_statement = code_resolve_statement(resolver, statement);
		if (code_statement) {
			parent_statement->next = code_statement;
			parent_statement       = code_statement;
			statement_count += 1;
		}
	}

	block->statement_head  = statement_stub_head.next;
	block->statement_count = statement_count;

	return block;
}

int main() {
	String content = read_entire_file("Simple.kano");

	auto builder = new String_Builder;

	Parser parser;
	parser_init(&parser, content, builder);

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

	print_syntax(node);

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

	{
		Symbol sym;
		sym.name      = "";
		sym.type.kind = CODE_TYPE_NULL;
		resolver.symbols.buffer.add(sym);
	}

	auto code = code_resolve_block(&resolver, node);
	print_code(code);
	evaluate_node_block(code);

	return 0;
}
