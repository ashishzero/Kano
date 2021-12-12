#include "Interp.h"
#include "CodeNode.h"

Find_Type_Value evaluate_expression(Code_Node* root, Interp* interp);
Find_Type_Value evaluate_node_expression(Code_Node_Expression* root, Interp* interp);

void interp_init(Interp* interp) {
	interp->stack = new uint8_t[1000];
}

Find_Type_Value evaluate_node_literal(Code_Node_Literal* root){
	auto node = root;
	auto check_kind = (Code_Node*)root;
	Find_Type_Value type_value;
	switch (check_kind->type->kind) { 
		case CODE_TYPE_BOOL:
		{
			type_value.value.boolean.value = node->data.boolean.value;
			type_value.type = CODE_TYPE_BOOL;
		}break;
		case CODE_TYPE_INTEGER:
		{
			type_value.value.integer.value = node->data.integer.value;
			type_value.type = CODE_TYPE_INTEGER;
		}break;
		case CODE_TYPE_REAL:
		{
			type_value.value.real.value = node->data.real.value;
			type_value.type = CODE_TYPE_REAL;
		}break;
		NoDefaultCase();
	}
	return type_value;
}
Find_Type_Value evaluate_unary_operator(Code_Node_Unary_Operator* root, Interp* interp) {
	auto node = root;
	auto check_kind = (Code_Node*)root;
	switch (check_kind->type->kind) {
		case CODE_TYPE_BOOL:
		{
			switch (node->op_kind) {
				case UNARY_OPERATOR_LOGICAL_NOT: {
					auto type_value = evaluate_expression(node->child, interp);
					type_value.value.boolean.value =  !type_value.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
					NoDefaultCase();
			}
		}break;
		case CODE_TYPE_INTEGER:
		{
			switch (node->op_kind) {
				case UNARY_OPERATOR_PLUS: {
					auto type_value = evaluate_expression(node->child, interp);
					type_value.value.integer.value = 0 + type_value.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case UNARY_OPERATOR_MINUS:
				{
					auto type_value = evaluate_expression(node->child, interp);
					type_value.value.integer.value = 0 - type_value.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case UNARY_OPERATOR_BITWISE_NOT: {
					auto type_value = evaluate_expression(node->child, interp);
					type_value.value.integer.value =  ~type_value.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				NoDefaultCase();
			}

		}break;
		case CODE_TYPE_REAL:
		{
			switch (node->op_kind) {
			case UNARY_OPERATOR_PLUS: {
				auto type_value = evaluate_expression(node->child, interp);
				type_value.value.real.value = 0 + type_value.value.real.value;
				type_value.type = CODE_TYPE_REAL;
				return type_value;
			}break;
			case UNARY_OPERATOR_MINUS:
			{
				auto type_value = evaluate_expression(node->child, interp);
				type_value.value.real.value = 0 - type_value.value.real.value;
				type_value.type = CODE_TYPE_REAL;
				return type_value;
			}break;
			NoDefaultCase();
			}

		}break;
		NoDefaultCase();
	} 
}

Find_Type_Value evaluate_binary_operator(Code_Node_Binary_Operator* root, Interp* interp) {
	auto node = root;
	auto check_kind = (Code_Node*)root;
	Find_Type_Value type_value;
	switch (check_kind->type->kind) {
		case CODE_TYPE_INTEGER:
		{
			auto a = evaluate_expression(node->left, interp);
			auto b = evaluate_expression(node->right, interp);
			switch (node->op_kind) {
				case BINARY_OPERATOR_ADDITION:
				{
					type_value.value.integer.value = a.value.integer.value + b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_SUBTRACTION:
				{
					type_value.value.integer.value = a.value.integer.value - b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_MULTIPLICATION:
				{
					type_value.value.integer.value = a.value.integer.value * b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_DIVISION:
				{
					type_value.value.integer.value = a.value.integer.value / b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_REMAINDER:
				{
					type_value.value.integer.value = a.value.integer.value % b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_BITWISE_AND:
				{
					type_value.value.integer.value = a.value.integer.value & b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;			
				case BINARY_OPERATOR_BITWISE_OR:
				{
					type_value.value.integer.value = a.value.integer.value | b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_BITWISE_XOR:
				{
					type_value.value.integer.value = a.value.integer.value ^ b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_BITWISE_SHIFT_RIGHT:
				{
					type_value.value.integer.value = a.value.integer.value >> b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_BITWISE_SHIFT_LEFT:
				{
					type_value.value.integer.value = a.value.integer.value << b.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case BINARY_OPERATOR_COMPARE_EQUAL:
				{
					type_value.value.boolean.value = (a.value.integer.value == b.value.integer.value);
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				NoDefaultCase();
			}

		}break;
		case CODE_TYPE_REAL:
		{
				auto a = evaluate_expression(node->left, interp);
				auto b = evaluate_expression(node->right, interp);
				switch (node->op_kind) {
				case BINARY_OPERATOR_ADDITION:
				{
					type_value.value.real.value = a.value.real.value + b.value.real.value;
					type_value.type = CODE_TYPE_REAL;
					return type_value;
				}break;
				case BINARY_OPERATOR_SUBTRACTION:
				{
					type_value.value.real.value = a.value.real.value - b.value.real.value;
					type_value.type = CODE_TYPE_REAL;
					return type_value;
				}break;
				case BINARY_OPERATOR_MULTIPLICATION:
				{
					type_value.value.real.value = a.value.real.value * b.value.real.value;
					type_value.type = CODE_TYPE_REAL;
					return type_value;
				}break;
				case BINARY_OPERATOR_DIVISION:
				{
					type_value.value.real.value = a.value.real.value / b.value.real.value;
					type_value.type = CODE_TYPE_REAL;
					return type_value;
				}break;
				case BINARY_OPERATOR_COMPARE_EQUAL:
				{
					type_value.value.boolean.value = (a.value.real.value == b.value.real.value);
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				NoDefaultCase();
				}
		}break;
		case CODE_TYPE_BOOL:
		{
			auto a = evaluate_expression(node->left, interp);
			auto b = evaluate_expression(node->right, interp);
			switch (node->op_kind) {
				case BINARY_OPERATOR_RELATIONAL_GREATER:
				{
					type_value.value.boolean.value = a.value.boolean.value > b.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				case BINARY_OPERATOR_RELATIONAL_LESS:
				{
					type_value.value.boolean.value = a.value.boolean.value < b.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;

				}break;
				case BINARY_OPERATOR_RELATIONAL_GREATER_EQUAL:
				{
					type_value.value.boolean.value = a.value.boolean.value >= b.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				case BINARY_OPERATOR_RELATIONAL_LESS_EQUAL:
				{
					type_value.value.boolean.value = a.value.boolean.value <= b.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;

				}break;
				case BINARY_OPERATOR_COMPARE_EQUAL:
				{
					type_value.value.boolean.value = (a.value.boolean.value == b.value.boolean.value);
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				case BINARY_OPERATOR_COMPARE_NOT_EQUAL:
				{
					type_value.value.boolean.value = a.value.boolean.value != b.value.boolean.value;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				NoDefaultCase();
			}

		}break;
		NoDefaultCase();
	}
}
Find_Type_Value evaluate_expression(Code_Node* root, Interp* interp) {
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

		case CODE_NODE_ADDRESS:
		{
			auto node = (Code_Node_Address*)root;
			//Assert(node->type.kind == CODE_TYPE_REAL);
			uint32_t offset = node->offset;
			Find_Type_Value type_value;

			switch (node->type->kind) {
			case CODE_TYPE_INTEGER: {
				int* value = (int*)(interp->stack + offset);
				type_value.value.integer.value = *value;
				type_value.type = CODE_TYPE_INTEGER;
				return type_value;
			}break;
			case CODE_TYPE_REAL: {

				float* value = (float*)(interp->stack + offset);
				type_value.value.real.value = *value;
				type_value.type = CODE_TYPE_REAL;
				return type_value;
			}break;

			case CODE_TYPE_BOOL: {
				bool* value = (bool*)(interp->stack + offset);
				type_value.value.boolean.value = *value;
				type_value.type = CODE_TYPE_BOOL;
				return type_value;
			}break;
				NoDefaultCase();
			}
		} break;

		case CODE_NODE_ASSIGNMENT:
		{
			auto node = (Code_Node_Assignment*)root;
		/*float*/	auto value = evaluate_node_expression((Code_Node_Expression*)node->value, interp);
			Assert(node->destination->child->kind == CODE_NODE_ADDRESS);

			auto destiny = (Code_Node_Address*)node->destination->child;
			interp_init(interp);
			Find_Type_Value type_value;

			switch (destiny->type->kind) {
				case CODE_TYPE_REAL:
				{
					float* dst = (float*)(interp->stack + destiny->offset);
					*dst = value.value.real.value;
					//return value;
					type_value.value.real.value = value.value.real.value;;
					type_value.type = CODE_TYPE_REAL;
					return type_value;
				}break;
				case CODE_TYPE_INTEGER: {
					int* dst = (int*)(interp->stack + destiny->offset);
					*dst = value.value.integer.value;
					//return value;
					type_value.value.integer.value = value.value.integer.value;
					type_value.type = CODE_TYPE_INTEGER;
					return type_value;
				}break;
				case CODE_TYPE_BOOL: {
					bool* dst = (bool*)(interp->stack + destiny->offset);
					*dst = value.value.boolean.value;
					//return value;
					type_value.value.boolean.value = value.value.boolean.value;;
					type_value.type = CODE_TYPE_BOOL;
					return type_value;
				}break;
				NoDefaultCase();
			}
		}break;
		NoDefaultCase();
	}
	//return 0;
}

Find_Type_Value evaluate_node_expression(Code_Node_Expression* root, Interp* interp) {
	return evaluate_expression(root->child, interp);
}

void evaluate_node_statement(Code_Node_Statement* root, Interp* interp) {
	Find_Type_Value type_value;// = new Find_Type_Value;
	switch (root->node->kind) {
	case CODE_NODE_EXPRESSION:
	{
		auto result = evaluate_node_expression((Code_Node_Expression*)root->node, interp);
		switch (result.type) {
		case CODE_TYPE_INTEGER: {
			printf("statement executes:: %d\n", result.value.integer.value);
		}break;
		case CODE_TYPE_REAL: {
			printf("statement executes:: %f\n", result.value.real.value);
		}break;
						 
		case CODE_TYPE_BOOL: {
			printf("statement executes:: %d\n", (int)result.value.boolean.value);
		}break;
			NoDefaultCase();
		}

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