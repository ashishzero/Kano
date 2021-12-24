#include "Lexer.h"
#include "SyntaxNode.h"
#include "StringBuilder.h"

struct Error_Node
{
    String          message;
    Syntax_Location location;
    Error_Node *    next;
};

struct Error_List
{
    Error_Node  first;
    Error_Node *last;
};

struct Parser
{
    Lexer           lexer;
    Syntax_Location location;
    Token_Value     value;

    Error_List      error;
    uint64_t        error_count;
    bool            parsing;

    String_Builder *builder;
};

Syntax_Node *            parse_subexpression(Parser *parser, uint32_t prec);
Syntax_Node *            parse_expression(Parser *parser, uint32_t prec);

Syntax_Node_Expression * parse_root_expression(Parser *parser);
Syntax_Node_Type *       parse_type(Parser *parser);
Syntax_Node_Declaration *parse_declaration(Parser *parser);
Syntax_Node_Statement *  parse_statement(Parser *parser);
Syntax_Node_Block *      parse_block(Parser *parser);

void                     parser_init(Parser *parser, String content, String_Builder *builder);
