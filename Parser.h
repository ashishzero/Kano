#include "Lexer.h"
#include "SyntaxNode.h"
#include "StringBuilder.h"
#include "Error.h"

struct Parser
{
	Lexer           lexer;
	Syntax_Location location;
	Token_Value     value;

	Error_List      error;
	bool            parsing;

	String_Builder *builder;
};

Syntax_Node *             parse_subexpression(Parser *parser, uint32_t prec);
Syntax_Node *             parse_expression(Parser *parser, uint32_t prec);

Syntax_Node_Expression *  parse_root_expression(Parser *parser);
Syntax_Node_Type *        parse_type(Parser *parser);
Syntax_Node_Declaration * parse_declaration(Parser *parser);
Syntax_Node_Statement *   parse_statement(Parser *parser);
Syntax_Node_Block *       parse_block(Parser *parser);
Syntax_Node_Global_Scope *parse_global_scope(Parser *parser);

void                      parser_init(Parser *parser, String content, String_Builder *builder);
