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

void print_syntax(Syntax_Node *root, FILE *fp, int child_indent) {
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

	case SYNTAX_NODE_IDENTIFIER:
	{
		auto node = (Syntax_Node_Identifier *)root;
		fprintf(fp, "Identifier(%s)\n", node->name.data);
	} break;

	case SYNTAX_NODE_UNARY_OPERATOR:
	{
		auto node = (Syntax_Node_Unary_Operator *)root;
		fprintf(fp, "Unary Operator(%s)\n", token_kind_string(node->op).data);
		print_syntax(node->child, fp, child_indent);
	} break;

	case SYNTAX_NODE_BINARY_OPERATOR:
	{
		auto node = (Syntax_Node_Binary_Operator *)root;
		fprintf(fp, "Binary Operator(%s)\n", token_kind_string(node->op).data);
		print_syntax(node->left, fp, child_indent);
		print_syntax(node->right, fp, child_indent);
	} break;

	case SYNTAX_NODE_TYPE:
	{
		auto node = (Syntax_Node_Type *)root;

		const char *type_name = nullptr;
		switch (node->syntax_type) {
			case SYNTAX_TYPE_FLOAT: type_name = "float"; break;
			NoDefaultCase();
		}

		fprintf(fp, "Type(%s)\n", type_name);
	} break;

	case SYNTAX_NODE_ASSIGNMENT:
	{
		auto node = (Syntax_Node_Assignment *)root;
		fprintf(fp, "Assignment(=)\n");
		print_syntax(node->left, fp, child_indent);
		print_syntax(node->right, fp, child_indent);
	} break;

	case SYNTAX_NODE_EXPRESSION:
	{
		auto node = (Syntax_Node_Expression *)root;
		fprintf(fp, "Expression()\n");
		print_syntax(node->child, fp, child_indent);
	} break;

	case SYNTAX_NODE_DECLARATION:
	{
		auto node = (Syntax_Node_Declaration *)root;
		if (node->flags & DECLARATION_IS_CONSTANT) {
			fprintf(fp, "Constant Declaration(%s)\n", node->identifier.data);
		}
		else {
			fprintf(fp, "Variable Declaration(%s)\n", node->identifier.data);
		}
		print_syntax(node->type, fp, child_indent);
	} break;

	case SYNTAX_NODE_STATEMENT:
	{
		auto node = (Syntax_Node_Statement *)root;
		fprintf(fp, "Statement()\n");
		print_syntax(node->node, fp, child_indent);
	} break;

	case SYNTAX_NODE_BLOCK:
	{
		auto node = (Syntax_Node_Block *)root;
		fprintf(fp, "Block()\n");
		for (auto statement = node->statement_head; statement; statement = statement->next) {
			print_syntax(statement, fp, child_indent);
		}
	} break;

	NoDefaultCase();
	}
}

//
//
//

void print_code(Code_Node *root, FILE *fp, int child_indent) {
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

	case CODE_NODE_STACK:
	{
		auto node = (Code_Node_Stack *)root;
		fprintf(fp, "Stack(0x%x)\n", node->offset);
	} break;

	case CODE_NODE_DESTINATION:
	{
		auto node = (Code_Node_Destination *)root;
		fprintf(fp, "Destination()\n");
		print_code(node->child, fp, child_indent);
	} break;

	case CODE_NODE_UNARY_OPERATOR:
	{
		auto node = (Code_Node_Unary_Operator *)root;
		printf("Unary Operator(%s): %p\n", unary_operator_kind_string(node->op_kind).data, node->op);
		print_code(node->child, fp, child_indent);
	} break;

	case CODE_NODE_BINARY_OPERATOR:
	{
		auto node = (Code_Node_Binary_Operator *)root;
		printf("Binary Operator(%s): %p\n", binary_operator_kind_string(node->op_kind).data, node->op);
		print_code(node->left, fp, child_indent);
		print_code(node->right, fp, child_indent);
	} break;

	case CODE_NODE_EXPRESSION:
	{
		auto node = (Code_Node_Expression *)root;
		printf("Expression()\n");
		print_code(node->child, fp, child_indent);
	} break;

	case CODE_NODE_ASSIGNMENT:
	{
		auto node = (Code_Node_Assignment *)root;
		printf("Assignment()\n");
		print_code(node->destination, fp, child_indent);
		print_code(node->value, fp, child_indent);
	} break;

	case CODE_NODE_STATEMENT:
	{
		auto node = (Code_Node_Statement *)root;
		printf("Statement()\n");
		print_code(node->node, fp, child_indent);
	} break;

	case CODE_NODE_BLOCK:
	{
		auto node = (Code_Node_Block *)root;
		printf("Block()\n");
		for (auto statement = node->statement_head; statement; statement = statement->next) {
			print_code(statement, fp, child_indent);
		}
	} break;

	NoDefaultCase();
	}
}
