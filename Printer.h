#pragma once
#include "CodeNode.h"
#include <stdio.h>

void print(Syntax_Node *root, FILE *fp = stdout, int child_indent = 0);
void print(Code_Node *root, FILE *fp = stdout, int child_indent = 0);
