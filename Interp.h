#pragma once
#include "Printer.h"
#include "Token.h"

struct Interp {
	uint8_t* stack;
};
struct Find_Type_Value {
	Code_Value		 value;
	Code_Type_Kind	 type;
};

void interp_init(Interp* interp, size_t size);
void evaluate_node_block(Code_Node_Block* root, Interp* stack);
