#include "Lexer.h"
#include "SyntaxNode.h"
#include "JsonWriter.h"

struct Parser
{
	Lexer           lexer;
	Syntax_Location location;
	Token_Value     value;

	int             error_count;
	Json_Writer *   error;
	bool            parsing;
};

typedef void (*Parser_On_Error)(struct Parser *parser);

void parser_register_error_proc(Parser_On_Error proc);

Syntax_Node *             parse_subexpression(Parser *parser, uint32_t prec);
Syntax_Node *             parse_expression(Parser *parser, uint32_t prec);

Syntax_Node_Expression *  parse_root_expression(Parser *parser);
Syntax_Node_Type *        parse_type(Parser *parser);
Syntax_Node_Declaration * parse_declaration(Parser *parser);
Syntax_Node_Statement *   parse_statement(Parser *parser);
Syntax_Node_Block *       parse_block(Parser *parser);
Syntax_Node_Global_Scope *parse_global_scope(Parser *parser);

void                      parser_init(Parser *parser, String content, Json_Writer *error);
