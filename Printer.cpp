#include "Printer.h"
#include "Token.h"

static inline String code_type_kind_string(Code_Type_Kind kind) {
	static String strings[] = {
		"-null-",
		"real",
	};
	static_assert(ArrayCount(strings) == _CODE_TYPE_COUNT);
	return strings[kind];
}

static inline String unary_operator_kind_string(Unary_Operator_Kind kind) {
	static String strings[] = {
		"+",
		"-",
	};
	static_assert(ArrayCount(strings) == _UNARY_OPERATOR_COUNT);
	return strings[kind];
}

static inline String binary_operator_kind_string(Binary_Operator_Kind kind) {
	static String strings[] = {
		"+", "-", "*", "/"
	};
	static_assert(ArrayCount(strings) == _BINARY_OPERATOR_COUNT);
	return strings[kind];
}

//
//
//

static inline void indent(FILE *fp, uint32_t depth) { fprintf(fp, "%*s", depth * 3, ""); }

void print(Syntax_Node *root, FILE *fp, int child_indent) {
	indent(fp, child_indent);
	child_indent += 1;

	switch (root->kind) {
	case SYNTAX_NODE_NULL:
	{
		fprintf(fp, "Null()\n");
	} break;

	case SYNTAX_NODE_LITERAL:
	{
		auto node = (Syntax_Node_Literal *)root;
		fprintf(fp, "Literal(%f)\n", node->value);
	} break;

	case SYNTAX_NODE_UNARY_OPERATOR:
	{
		auto node = (Syntax_Node_Unary_Operator *)root;
		printf("Unary Operator(%s)\n", token_kind_string(node->op).data);
		print(node->child, fp, child_indent);
	} break;

	case SYNTAX_NODE_BINARY_OPERATOR:
	{
		auto node = (Syntax_Node_Binary_Operator *)root;
		printf("Binary Operator(%s)\n", token_kind_string(node->op).data);
		print(node->left, fp, child_indent);
		print(node->right, fp, child_indent);
	} break;

	NoDefaultCase();
	}
}

//
//
//

void print(Code_Node *root, FILE *fp, int child_indent) {
	indent(fp, child_indent);
	child_indent += 1;

	switch (root->kind) {
	case CODE_NODE_NULL:
	{
		fprintf(fp, "Null()\n");
	} break;

	case CODE_NODE_LITERAL:
	{
		auto node = (Code_Node_Literal *)root;
		fprintf(fp, "Literal(%f)\n", node->value);
	} break;

	case CODE_NODE_UNARY_OPERATOR:
	{
		auto node = (Code_Node_Unary_Operator *)root;
		printf("Unary Operator(%s): %p\n", unary_operator_kind_string(node->op_kind).data, node->op);
		print(node->child, fp, child_indent);
	} break;

	case CODE_NODE_BINARY_OPERATOR:
	{
		auto node = (Code_Node_Binary_Operator *)root;
		printf("Binary Operator(%s): %p\n", binary_operator_kind_string(node->op_kind).data, node->op);
		print(node->left, fp, child_indent);
		print(node->right, fp, child_indent);
	} break;

	NoDefaultCase();
	}
}
