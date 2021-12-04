#include "Lexer.h"
#include "SyntaxNode.h"

struct Error_Node {
	String          message;
	Syntax_Location location;
	Error_Node *next;
};

struct Error_List {
	Error_Node first;
	Error_Node *last;
};

struct Parser {
	Lexer           lexer;
	Syntax_Location location;
	Token_Value      value;

	Error_List      error;
	uint64_t        error_count;
};

Syntax_Node *parse_subexpression(Parser *parser, uint32_t prec);
Syntax_Node *parse_expression(Parser *parser, uint32_t prec);

Syntax_Node *parse_statement(Parser *parser);

void parser_init(Parser *parser, String content);
