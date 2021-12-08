#include "Interp.h"
#include "CodeNode.h"
#include <iostream>

float evaluate_expression(Code_Node* root, Interp* interp);
float evaluate_node_expression(Code_Node_Expression* root, Interp* interp);

void interp_init(Interp* interp) {
	interp->stack = new uint8_t[1000];
}

float evaluate_node_literal(Code_Node_Literal* root) {
	auto node = root;
	return (float)(node->value);
}
float evaluate_unary_operator(Code_Node_Unary_Operator* root, Interp* interp) {
	auto node = root;
	switch (node->op_kind) {
	case UNARY_OPERATOR_PLUS: {
		return (0 + evaluate_expression(node->child, interp));
		break;
	}
	case UNARY_OPERATOR_MINUS:
	{
		return (0 - evaluate_expression(node->child, interp));
		break;
	}
	NoDefaultCase();
	}
	return 0;
}

float evaluate_binary_operator(Code_Node_Binary_Operator* root, Interp* interp) {
	auto node = root;
	switch (node->op_kind) {
	case BINARY_OPERATOR_ADD:
	{
		return(evaluate_expression(node->left, interp) + evaluate_expression(node->right, interp));
	}break;
	case BINARY_OPERATOR_SUB:
	{
		return(evaluate_expression(node->left, interp) - evaluate_expression(node->right, interp));
	}break;
	case BINARY_OPERATOR_MUL:
	{
		return(evaluate_expression(node->left, interp) * evaluate_expression(node->right, interp));
	}break;
	case BINARY_OPERATOR_DIV:
	{
		return(evaluate_expression(node->left, interp) / evaluate_expression(node->right, interp));
	}break;
	NoDefaultCase();
	}
	return 0;
}
float evaluate_expression(Code_Node* root, Interp* interp) {
	switch (root->kind) {
		case CODE_NODE_LITERAL:
		{
			return evaluate_node_literal((Code_Node_Literal*)root);
		} break;

		case CODE_NODE_UNARY_OPERATOR:
		{
			return evaluate_unary_operator((Code_Node_Unary_Operator*)root, interp);
		} break;

		case CODE_NODE_BINARY_OPERATOR:
		{
			return evaluate_binary_operator((Code_Node_Binary_Operator*)root, interp);
		} break;

		case CODE_NODE_STACK:
		{
			auto node = (Code_Node_Stack*)root;
			Assert(node->type.kind == CODE_TYPE_REAL);
			uint32_t offset = node->offset;
			float *value = (float *)(interp->stack + offset);
			return *value;
		} break;

		case CODE_NODE_ASSIGNMENT:
		{
			auto node = (Code_Node_Assignment*)root;
			float value = evaluate_node_expression((Code_Node_Expression*)node->value, interp);
			Assert(node->destination->child->kind == CODE_NODE_STACK);

			auto destiny = (Code_Node_Stack*)node->destination->child;
			interp_init(interp);
			float* dst = (float*)(interp->stack + destiny->offset);
			*dst = value;
			return value;
		}break;
		NoDefaultCase();
	}
	return 0;
}

float evaluate_node_expression(Code_Node_Expression* root, Interp* interp) {
	return evaluate_expression(root->child, interp);
}

void evaluate_node_statement(Code_Node_Statement* root, Interp* interp) {
	switch (root->node->kind) {
	case CODE_NODE_EXPRESSION:
	{
		float result = evaluate_node_expression((Code_Node_Expression*)root->node, interp);
		printf("statement executes:: %f\n", result);
	}break;
	NoDefaultCase();

	}
}

void evaluate_node_block(Code_Node_Block* root, Interp* interp) {
	for (auto statement = root->statement_head; statement; statement = statement->next) {
		evaluate_node_statement(statement, interp);
	}
	printf("Block %d statement executed\n", (int)root->statement_count);
}