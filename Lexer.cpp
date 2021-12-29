#include "Lexer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define lexer_iswhitespace(code) ((code) == ' ' || (code) == '\t' || (code) == '\v' || (code) == '\f')

static bool lexer_isalpha(uint32_t code)
{
    if (code >= 'A' && code <= 'Z')
        return true;
    if (code >= 'a' && code <= 'z')
        return true;
    if (code == '_')
        return true;
    if (code > 125)
        return true;
    return false;
}

static bool lexer_isnum(uint32_t code)
{
    return code >= '0' && code <= '9';
}

//
//
//

static bool lexer_continue(Lexer *lexer)
{
    lexer->finished = lexer->cursor >= (lexer->content.data + lexer->content.length);
    return !lexer->finished;
}

static String lexer_token_content(Lexer *lexer)
{
    String content;
    content.data   = lexer->token.content.data;
    content.length = (int64_t)(lexer->cursor - content.data);
    return content;
}

static void lexer_make_token(Lexer *lexer, Token_Kind kind)
{
    lexer->token.kind    = kind;
    lexer->token.content = lexer_token_content(lexer);
}

static void lexer_error(Lexer *lexer, const char *fmt, ...)
{
    va_list arg;
    va_start(arg, fmt);
    vsnprintf((char *)lexer->buffer, sizeof(lexer->buffer), fmt, arg);
    va_end(arg);

    lexer_make_token(lexer, TOKEN_KIND_ERROR);
}

//
//
//

static bool lexer_advance_newline(Lexer *lexer)
{
    if (*lexer->cursor == '\r')
    {
        lexer->cursor++;

        if (*lexer->cursor == '\n')
            lexer->cursor++;

        lexer->column = 1;
        lexer->row += 1;

        return true;
    }

    else if (*lexer->cursor == '\n')
    {
        lexer->cursor++;

        lexer->column = 1;
        lexer->row += 1;

        return true;
    }

    return false;
}

static bool lexer_advance_whitespace(Lexer *lexer)
{
    if (lexer_advance_newline(lexer))
    {
        return true;
    }

    if (lexer_iswhitespace(*lexer->cursor))
    {
        do
        {
            lexer->cursor += 1;
        } while (lexer_continue(lexer) && lexer_iswhitespace(*lexer->cursor));
        return true;
    }

    return false;
}

static bool lexer_advance_comments(Lexer *lexer)
{
    auto cursor = lexer->cursor;

    if (*lexer->cursor == '/')
    {
        lexer->cursor++;

        // single line comment
        if (*lexer->cursor == '/')
        {
            lexer->cursor++;

            while (lexer_continue(lexer))
            {
                if (lexer_advance_newline(lexer))
                    return true;
                lexer->cursor++;
            }

            return true;
        }

        // block comment
        else if (*lexer->cursor == '*')
        {
            lexer->cursor++;

            uint64_t comment_block_count = 1;

            while (lexer_continue(lexer) && comment_block_count != 0)
            {
                if (lexer_advance_whitespace(lexer))
                    continue;
                else if (*lexer->cursor == '/')
                {
                    lexer->cursor += 1;
                    if (*lexer->cursor == '*')
                    {
                        lexer->cursor += 1;
                        comment_block_count += 1;
                    }
                }
                else if (*lexer->cursor == '*')
                {
                    lexer->cursor += 1;
                    if (*lexer->cursor == '/')
                    {
                        lexer->cursor += 1;
                        comment_block_count -= 1;
                    }
                }
                else
                {
                    lexer->cursor++;
                }
            }

            if (comment_block_count != 0)
            {
                lexer_error(lexer, "Comment not closed");
            }

            return true;
        }
    }

    lexer->cursor = cursor;
    return false;
}

//
//
//

void lexer_next(Lexer *lexer)
{
    lexer->token.kind = TOKEN_KIND_ERROR;

    while (lexer_continue(lexer))
    {
        lexer->token.row          = lexer->row;
        lexer->token.column       = lexer->column;
        lexer->token.offset       = lexer->cursor - lexer->content.data;
        lexer->token.content.data = lexer->cursor;

        if (lexer_advance_whitespace(lexer))
            continue;

        if (lexer_advance_comments(lexer))
            continue;

        auto a = *lexer->cursor;
        auto b = *(lexer->cursor + 1);

        if (lexer_isnum(a) || (a == '.' && lexer_isnum(b)))
        {
            uint8_t *string = lexer->cursor;

            char *   endptr = NULL;
            double   value  = strtod((char *)string, &endptr);
            lexer->cursor   = (uint8_t *)endptr;

            if (!lexer_isalpha(*endptr))
            {
                auto kind = TOKEN_KIND_INTEGER;
                for (auto ptr = (char *)string; ptr < endptr; ++ptr)
                {
                    if (*ptr == '.')
                        kind = TOKEN_KIND_REAL;
                }

                if (kind == TOKEN_KIND_INTEGER)
                    lexer->value.integer = (uint64_t)value;
                else
                    lexer->value.real = value;

                lexer_make_token(lexer, kind);
                return;
            }

            lexer_error(lexer, "Invalid number");
            return;
        }

        if (a == '"')
        {
            lexer->cursor++;
            auto string = lexer->cursor;
            while (lexer_continue(lexer))
            {
                if (lexer_advance_newline(lexer))
                {
                    lexer_error(lexer, "Expected '\"");
                    return;
                }
                if (*lexer->cursor == '"')
                {
                    lexer->value.string.length = lexer->cursor - string;
                    lexer->value.string.data   = string;
                    lexer->cursor++;
                    lexer_make_token(lexer, TOKEN_KIND_STRING);
                    return;
                }
                lexer->cursor++;
            }

            lexer_error(lexer, "Expected '\"");
            return;
        }

        // three character tokens
        if (lexer->cursor + 2 < lexer->content.data + lexer->content.length)
        {
            auto c = *(lexer->cursor + 2);

            if (c == '=')
            {
                if (a == b)
                {
                    if (a == '>')
                    {
                        lexer->cursor += 3;
                        lexer_make_token(lexer, TOKEN_KIND_COMPOUND_BITWISE_SHIFT_RIGHT);
                        return;
                    }
                    else if (a == '<')
                    {
                        lexer->cursor += 3;
                        lexer_make_token(lexer, TOKEN_KIND_COMPOUND_BITWISE_SHIFT_LEFT);
                        return;
                    }
                }
            }
        }

        // double character tokens
        if (a == '>' && b == '>')
        {
            lexer->cursor += 2;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_SHIFT_RIGHT);
            return;
        }
        else if (a == '<' && b == '<')
        {
            lexer->cursor += 2;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_SHIFT_LEFT);
            return;
        }
        else if (a == '-' && b == '>')
        {
            lexer->cursor += 2;
            lexer_make_token(lexer, TOKEN_KIND_DASH_ARROW);
            return;
        }
        else if (b == '=')
        {
            switch (a)
            {
            case '>':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_RELATIONAL_GREATER_EQUAL);
                return;
            case '<':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_RELATIONAL_LESS_EQUAL);
                return;
            case '=':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPARE_EQUAL);
                return;
            case '!':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPARE_NOT_EQUAL);
                return;
            case '+':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_PLUS);
                return;
            case '-':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_MINUS);
                return;
            case '*':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_MULTIPLY);
                return;
            case '/':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_DIVIDE);
                return;
            case '%':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_REMAINDER);
                return;
            case '&':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_BITWISE_AND);
                return;
            case '^':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_BITWISE_XOR);
                return;
            case '|':
                lexer->cursor += 2;
                lexer_make_token(lexer, TOKEN_KIND_COMPOUND_BITWISE_OR);
                return;
            }
        }

        // single character tokens
        switch (a)
        {
        case ':':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_COLON);
            return;
        case ',':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_COMMA);
            return;
        case '?':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_DEREFERENCE);
            return;
        case '.':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_PERIOD);
            return;
        case '=':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_EQUALS);
            return;
        case '(':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_OPEN_BRACKET);
            return;
        case ')':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_CLOSE_BRACKET);
            return;
        case '{':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_OPEN_CURLY_BRACKET);
            return;
        case '}':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_CLOSE_CURLY_BRACKET);
            return;
        case '[':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_OPEN_SQUARE_BRACKET);
            return;
        case ']':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_CLOSE_SQUARE_BRACKET);
            return;
        case '+':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_PLUS);
            return;
        case '-':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_MINUS);
            return;
        case '*':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_ASTERISK);
            return;
        case '/':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_DIVISION);
            return;
        case '%':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_REMAINDER);
            return;
        case ';':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_SEMICOLON);
            return;
        case '&':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_AND);
            return;
        case '^':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_XOR);
            return;
        case '|':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_OR);
            return;
        case '~':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_BITWISE_NOT);
            return;
        case '!':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_LOGICAL_NOT);
            return;
        case '>':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_RELATIONAL_GREATER);
            return;
        case '<':
            lexer->cursor++;
            lexer_make_token(lexer, TOKEN_KIND_RELATIONAL_LESS);
            return;
        }

        if (lexer_isalpha(*lexer->cursor))
        {
            const char *string = (char *)lexer->cursor;

            do
            {
                lexer->cursor++;
            } while (lexer_isalpha(*lexer->cursor) || lexer_isnum(*lexer->cursor));

            String content;
            content.data                            = (uint8_t *)string;
            content.length                          = (lexer->cursor - content.data);

            static const String     KeyWords[]      = {"var",  "const", "true", "false", "int", "float", "bool",   "if",
                                              "then", "else",  "for",  "while", "do",  "size_of", "type_of",
                                              "proc",  "struct", "return", 
                                              "cast", "void", "null"};

            static const Token_Kind KeyWordTokens[] = {
                TOKEN_KIND_VAR,  TOKEN_KIND_CONST, TOKEN_KIND_TRUE,   TOKEN_KIND_FALSE,
                TOKEN_KIND_INT,  TOKEN_KIND_FLOAT, TOKEN_KIND_BOOL,   TOKEN_KIND_IF,
                TOKEN_KIND_THEN, TOKEN_KIND_ELSE,  TOKEN_KIND_FOR,    TOKEN_KIND_WHILE, TOKEN_KIND_DO,
                TOKEN_KIND_SIZE_OF, TOKEN_KIND_TYPE_OF,
                TOKEN_KIND_PROC,  TOKEN_KIND_STRUCT, TOKEN_KIND_RETURN,
                TOKEN_KIND_CAST, TOKEN_KIND_VOID, TOKEN_KIND_NULL};

            static_assert(ArrayCount(KeyWords) == ArrayCount(KeyWordTokens));

            for (uint32_t index = 0; index < ArrayCount(KeyWords); ++index)
            {
                if (content == KeyWords[index])
                {
                    lexer_make_token(lexer, KeyWordTokens[index]);
                    return;
                }
            }

            lexer->value.string.length = content.length;
            lexer->value.string.data   = content.data;
            lexer_make_token(lexer, TOKEN_KIND_IDENTIFIER);
            return;
        }

        lexer->cursor++;
        lexer_error(lexer, "Bad character");
        return;
    }

    lexer_make_token(lexer, TOKEN_KIND_END);
    return;
}

void lexer_init(Lexer *lexer, String content)
{
    lexer->content    = content;
    lexer->cursor     = content.data;

    lexer->token.kind = TOKEN_KIND_ERROR;
    lexer->buffer[0]  = 0;
    lexer->finished   = false;

    lexer->row        = 1;
    lexer->column     = 1;
}
