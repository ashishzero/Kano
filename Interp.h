#pragma once
#include "Printer.h"
#include "Token.h"

struct Interp {
	uint8_t* stack;
};
void evaluate_node_block(Code_Node_Block* root, Interp* stack);
