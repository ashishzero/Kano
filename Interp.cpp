#include "Interp.h"
#include "CodeNode.h"

double evaluate_expression(Code_Node* root);

double evaluate_node_literal(Code_Node_Literal* root) {
	auto node = root;
	Assert(root->type->kind == CODE_TYPE_REAL);
	return node->data.real.value;
}
double evaluate_unary_operator(Code_Node_Unary_Operator* root) {
	auto node = root;
	switch (node->op_kind) {
	case UNARY_OPERATOR_PLUS: {
		return (0 + evaluate_expression(node->child));
		break;
	}
	case UNARY_OPERATOR_MINUS:
	{
		return (0 - evaluate_expression(node->child));
		break;
	}
	NoDefaultCase();
	}
	return 0;
}

double evaluate_binary_operator(Code_Node_Binary_Operator* root) {
	auto node = root;
	switch (node->op_kind) {
	case BINARY_OPERATOR_ADDITION:
	{
		return(evaluate_expression(node->left) + evaluate_expression(node->right));
	}break;
	case BINARY_OPERATOR_SUBTRACTION:
	{
		return(evaluate_expression(node->left) - evaluate_expression(node->right));
	}break;
	case BINARY_OPERATOR_MULTIPLICATION:
	{
		return(evaluate_expression(node->left) * evaluate_expression(node->right));
	}break;
	case BINARY_OPERATOR_DIVISION:
	{
		return(evaluate_expression(node->left) / evaluate_expression(node->right));
	}break;
	NoDefaultCase();
	}
	return 0;
}
double evaluate_expression(Code_Node* root) {
	switch (root->kind) {
		case CODE_NODE_LITERAL:
		{
			return evaluate_node_literal((Code_Node_Literal*)root);
		} break;

		case CODE_NODE_UNARY_OPERATOR:
		{
			return evaluate_unary_operator((Code_Node_Unary_Operator*)root);
		} break;

		case CODE_NODE_BINARY_OPERATOR:
		{
			return evaluate_binary_operator((Code_Node_Binary_Operator*)root);
		} break;
		NoDefaultCase();
	}
	return 0;
}

double evaluate_node_expression(Code_Node_Expression* root) {
	return evaluate_expression(root->child);
}

void evaluate_node_statement(Code_Node_Statement* root) {
	switch (root->node->kind) {
	case CODE_NODE_EXPRESSION:
	{
		double result = evaluate_node_expression((Code_Node_Expression*)root->node);
		printf("statement executes:: %f\n", result);
	}break;
	NoDefaultCase();

	}
}

void evaluate_node_block(Code_Node_Block* root) {
	for (auto statement = root->statement_head; statement; statement = statement->next) {
		evaluate_node_statement(statement);
	}
	printf("Block %d statement executed\n", (int)root->statement_count);
}