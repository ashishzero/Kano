#include "Interp.h"
#include "CodeNode.h"

double return_value(Code_Node* root, double result) {
	switch (root->kind) {
		case CODE_NODE_LITERAL:
		{
			auto node = (Code_Node_Literal*)root;
			result = node->value;
			return result;
		} break;

		case CODE_NODE_UNARY_OPERATOR:
		{
			auto node = (Code_Node_Unary_Operator*)root;
			switch (node->op_kind) {
				case UNARY_OPERATOR_PLUS: {
					result = 0 + return_value(node->child, result);
					break;
				}
				case UNARY_OPERATOR_MINUS:
				{
					result = 0 - return_value(node->child, result);
					break;
				}
				NoDefaultCase();
			}
			return result;
		} break;

		case CODE_NODE_BINARY_OPERATOR:
		{
			auto node = (Code_Node_Binary_Operator*)root;
			switch (node->op_kind) {
				case BINARY_OPERATOR_ADD:
				{
					result = return_value(node->left, result) + return_value(node->right, result);
					break;
				}
				case BINARY_OPERATOR_SUB:
				{
					result = return_value(node->left, result) - return_value(node->right, result);
					break;
				}
				case BINARY_OPERATOR_MUL:
				{
					result = return_value(node->left, result) * return_value(node->right, result);
					break;
				}
				case BINARY_OPERATOR_DIV:
				{
					result = return_value(node->left, result) / return_value(node->right, result);
					break;
				}
				NoDefaultCase();
			}

			return result;
		} break;

		NoDefaultCase();
	}
}