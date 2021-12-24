#include "Printer.h"
#include "Token.h"

static inline String code_type_kind_string(Code_Type_Kind kind)
{
    static String strings[] = {"-null-", "integer", "real", "bool", "*", "proc"};
    static_assert(ArrayCount(strings) == _CODE_TYPE_COUNT);
    return strings[kind];
}

static inline String unary_operator_kind_string(Unary_Operator_Kind kind)
{
    static String strings[] = {"+", "-", "~", "!", "*", "?"};
    static_assert(ArrayCount(strings) == _UNARY_OPERATOR_COUNT);
    return strings[kind];
}

static inline String binary_operator_kind_string(Binary_Operator_Kind kind)
{
    static String strings[] = {"+",  "-",  "*",  "/",  "%",  ">>", ">>", "&",  "^",   "|",   ">",  "<",  ">=",
                               "<=", "==", "!=", "+=", "-=", "*=", "/=", "%=", ">>=", "<<=", "&=", "^=", "|="};
    static_assert(ArrayCount(strings) == _BINARY_OPERATOR_COUNT);
    return strings[kind];
}

//
//
//

static inline void indent(FILE *fp, uint32_t depth)
{
    fprintf(fp, "%*s", depth * 3, "");
}

void print_syntax(Syntax_Node *root, FILE *fp, int child_indent, const char *title)
{
    indent(fp, child_indent);
    child_indent += 1;

    if (title)
    {
        fprintf(fp, "%s:", title);
    }

    switch (root->kind)
    {
    case SYNTAX_NODE_NULL: {
        fprintf(fp, "Null()\n");
    }
    break;

    case SYNTAX_NODE_LITERAL: {
        auto node = (Syntax_Node_Literal *)root;

        switch (node->value.kind)
        {
        case Literal::INTEGER:
            fprintf(fp, "Literal(int:%d)\n", node->value.data.integer);
            break;
        case Literal::REAL:
            fprintf(fp, "Literal(float:%f)\n", node->value.data.real);
            break;
        case Literal::BOOL:
            fprintf(fp, "Literal(bool:%s)\n", node->value.data.boolean ? "true" : "false");
            break;
            NoDefaultCase();
        }
    }
    break;

    case SYNTAX_NODE_IDENTIFIER: {
        auto node = (Syntax_Node_Identifier *)root;
        fprintf(fp, "Identifier(%s)\n", node->name.data);
    }
    break;

    case SYNTAX_NODE_UNARY_OPERATOR: {
        auto node = (Syntax_Node_Unary_Operator *)root;
        fprintf(fp, "Unary Operator(%s)\n", token_kind_string(node->op).data);
        print_syntax(node->child, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_BINARY_OPERATOR: {
        auto node = (Syntax_Node_Binary_Operator *)root;
        fprintf(fp, "Binary Operator(%s)\n", token_kind_string(node->op).data);
        print_syntax(node->left, fp, child_indent);
        print_syntax(node->right, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_PROCEDURE_PROTOTYPE_ARGUMENT: {
        auto node = (Syntax_Node_Procedure_Prototype_Argument *)root;
        fprintf(fp, "Argument-Type()\n");
        print_syntax(node->type, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_PROCEDURE_PROTOTYPE: {
        auto node = (Syntax_Node_Procedure_Prototype *)root;
        fprintf(fp, "Procedure-Prototype()\n");
        for (auto arg = node->arguments_type; arg; arg = arg->next)
        {
            print_syntax(arg, fp, child_indent);
        }

        if (node->return_type)
        {
            print_syntax(node->return_type, fp, child_indent, "Return-Type");
        }
    }
    break;

    case SYNTAX_NODE_TYPE: {
        auto        node      = (Syntax_Node_Type *)root;

        const char *type_name = (char *)token_kind_string(node->token_type).data;
        fprintf(fp, "Type(%s)\n", type_name);

        if (node->type)
        {
            print_syntax(node->type, fp, child_indent);
        }
    }
    break;

    case SYNTAX_NODE_RETURN: {
        auto node = (Syntax_Node_Return *)root;
        fprintf(fp, "Return()\n");
        print_syntax(node->expression, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_ASSIGNMENT: {
        auto node = (Syntax_Node_Assignment *)root;
        fprintf(fp, "Assignment(=)\n");
        print_syntax(node->left, fp, child_indent);
        print_syntax(node->right, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_EXPRESSION: {
        auto node = (Syntax_Node_Expression *)root;
        fprintf(fp, "Expression()\n");
        print_syntax(node->child, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_IF: {
        auto node = (Syntax_Node_If *)root;
        fprintf(fp, "If()\n");
        print_syntax(node->condition, fp, child_indent, "Condition");
        print_syntax(node->true_statement, fp, child_indent, "True-Statement");

        if (node->false_statement)
        {
            print_syntax(node->false_statement, fp, child_indent, "False-Statement");
        }
    }
    break;

    case SYNTAX_NODE_FOR: {
        auto node = (Syntax_Node_For *)root;
        fprintf(fp, "For()\n");
        print_syntax(node->initialization, fp, child_indent, "Initialization");
        print_syntax(node->condition, fp, child_indent, "Condition");
        print_syntax(node->increment, fp, child_indent, "Increment");
        print_syntax(node->body, fp, child_indent, "Body");
    }
    break;

    case SYNTAX_NODE_WHILE: {
        auto node = (Syntax_Node_While *)root;
        fprintf(fp, "While()\n");
        print_syntax(node->condition, fp, child_indent, "Condition");
        print_syntax(node->body, fp, child_indent, "Body");
    }
    break;

    case SYNTAX_NODE_DO: {
        auto node = (Syntax_Node_Do *)root;
        fprintf(fp, "Do()\n");
        print_syntax(node->body, fp, child_indent, "Body");
        print_syntax(node->condition, fp, child_indent, "Condition");
    }
    break;

    case SYNTAX_NODE_PROCEDURE_ARGUMENT: {
        auto node = (Syntax_Node_Procedure_Argument *)root;
        fprintf(fp, "Arg()\n");
        print_syntax(node->declaration, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_PROCEDURE: {
        auto node = (Syntax_Node_Procedure *)root;
        fprintf(fp, "Procedure()\n");

        for (auto arg = node->arguments; arg; arg = arg->next)
        {
            print_syntax(arg, fp, child_indent, "Argument");
        }

        if (node->return_type)
        {
            print_syntax(node->return_type, fp, child_indent, "Return");
        }

        print_syntax(node->body, fp, child_indent, "Body");
    }
    break;

    case SYNTAX_NODE_DECLARATION: {
        auto node = (Syntax_Node_Declaration *)root;
        if (node->flags & SYMBOL_BIT_CONSTANT)
        {
            fprintf(fp, "Constant Declaration(%s)\n", node->identifier.data);
        }
        else
        {
            fprintf(fp, "Variable Declaration(%s)\n", node->identifier.data);
        }

        if (node->type)
        {
            print_syntax(node->type, fp, child_indent, "Type");
        }

        if (node->initializer)
        {
            print_syntax(node->initializer, fp, child_indent, "Initializer");
        }
    }
    break;

    case SYNTAX_NODE_STATEMENT: {
        auto node = (Syntax_Node_Statement *)root;
        fprintf(fp, "Statement()\n");
        print_syntax(node->node, fp, child_indent);
    }
    break;

    case SYNTAX_NODE_BLOCK: {
        auto node = (Syntax_Node_Block *)root;
        fprintf(fp, "Block()\n");
        for (auto statement = node->statement_head; statement; statement = statement->next)
        {
            print_syntax(statement, fp, child_indent);
        }
    }
    break;

        NoDefaultCase();
    }
}

//
//
//

void print_code(Code_Node *root, FILE *fp, int child_indent, const char *title)
{
    indent(fp, child_indent);
    child_indent += 1;

    if (title)
    {
        fprintf(fp, "%s:", title);
    }

    switch (root->kind)
    {
    case CODE_NODE_NULL: {
        fprintf(fp, "Null()\n");
    }
    break;

    case CODE_NODE_LITERAL: {
        auto node = (Code_Node_Literal *)root;

        switch (node->type->kind)
        {
        case CODE_TYPE_REAL:
            fprintf(fp, "Literal(float:%f)\n", node->data.real.value);
            break;
        case CODE_TYPE_INTEGER:
            fprintf(fp, "Literal(int:%d)\n", node->data.integer.value);
            break;
        case CODE_TYPE_BOOL:
            fprintf(fp, "Literal(bool:%s)\n", node->data.boolean.value ? "true" : "false");
            break;
            NoDefaultCase();
        }
    }
    break;

    case CODE_NODE_ADDRESS: {
        auto node = (Code_Node_Address *)root;
        fprintf(fp, "Address(%p)\n", node->offset);
    }
    break;

    case CODE_NODE_TYPE_CAST: {
        auto node = (Code_Node_Type_Cast *)root;
        fprintf(fp, "TypeCast()\n");
        print_code(node->child, fp, child_indent);
    }
    break;

    case CODE_NODE_UNARY_OPERATOR: {
        auto node = (Code_Node_Unary_Operator *)root;
        fprintf(fp, "Unary Operator(%s)\n", unary_operator_kind_string(node->op_kind).data);
        print_code(node->child, fp, child_indent);
    }
    break;

    case CODE_NODE_BINARY_OPERATOR: {
        auto node = (Code_Node_Binary_Operator *)root;
        fprintf(fp, "Binary Operator(%s)\n", binary_operator_kind_string(node->op_kind).data);
        print_code(node->left, fp, child_indent);
        print_code(node->right, fp, child_indent);
    }
    break;

    case CODE_NODE_EXPRESSION: {
        auto node = (Code_Node_Expression *)root;
        fprintf(fp, "Expression()\n");
        print_code(node->child, fp, child_indent);
    }
    break;

    case CODE_NODE_ASSIGNMENT: {
        auto node = (Code_Node_Assignment *)root;
        fprintf(fp, "Assignment()\n");
        print_code(node->destination, fp, child_indent);
        print_code(node->value, fp, child_indent);
    }
    break;

    case CODE_NODE_STATEMENT: {
        auto node = (Code_Node_Statement *)root;
        fprintf(fp, "Statement()\n");
        print_code(node->node, fp, child_indent);
    }
    break;

    case CODE_NODE_IF: {
        auto node = (Code_Node_If *)root;
        fprintf(fp, "If()\n");
        print_code(node->condition, fp, child_indent, "Condition");
        print_code(node->true_statement, fp, child_indent, "True-Statement");

        if (node->false_statement)
        {
            print_code(node->false_statement, fp, child_indent, "False-Statement");
        }
    }
    break;

    case CODE_NODE_FOR: {
        auto node = (Code_Node_For *)root;
        fprintf(fp, "For()\n");
        print_code(node->initialization, fp, child_indent, "Initialization");
        print_code(node->condition, fp, child_indent, "Condition");
        print_code(node->increment, fp, child_indent, "Increment");
        print_code(node->body, fp, child_indent, "Body");
    }
    break;

    case CODE_NODE_WHILE: {
        auto node = (Code_Node_While *)root;
        fprintf(fp, "While()\n");
        print_code(node->condition, fp, child_indent, "Condition");
        print_code(node->body, fp, child_indent, "Body");
    }
    break;

    case CODE_NODE_DO: {
        auto node = (Code_Node_Do *)root;
        fprintf(fp, "Do()\n");
        print_code(node->body, fp, child_indent, "Body");
        print_code(node->condition, fp, child_indent, "Condition");
    }
    break;

    case CODE_NODE_BLOCK: {
        auto node = (Code_Node_Block *)root;
        fprintf(fp, "Block()\n");
        for (auto statement = node->statement_head; statement; statement = statement->next)
        {
            print_code(statement, fp, child_indent);
        }
    }
    break;

        NoDefaultCase();
    }
}
