#include "Common.h"
#include "CodeNode.h"
#include "Parser.h"
#include "Printer.h"
#include "Interp.h"

#include <stdlib.h>

void handle_assertion(const char *reason, const char *file, int line, const char *proc)
{
    fprintf(stderr, "%s. File: %s(%d)\n", reason, file, line);
    DebugTriggerbreakpoint();
}

String read_entire_file(const char *file)
{
    FILE *f = fopen(file, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *string = (uint8_t *)malloc(fsize + 1);
    fread(string, 1, fsize, f);
    fclose(f);

    string[fsize] = 0;
    return String(string, (int64_t)fsize);
}

static inline uint32_t murmur_32_scramble(uint32_t k)
{
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed)
{
    uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--)
    {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--)
    {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

//
//
//

Unary_Operator_Kind token_to_unary_operator(Token_Kind kind)
{
    switch (kind)
    {
    case TOKEN_KIND_PLUS:
        return UNARY_OPERATOR_PLUS;
    case TOKEN_KIND_MINUS:
        return UNARY_OPERATOR_MINUS;
    case TOKEN_KIND_BITWISE_NOT:
        return UNARY_OPERATOR_BITWISE_NOT;
    case TOKEN_KIND_LOGICAL_NOT:
        return UNARY_OPERATOR_LOGICAL_NOT;
    case TOKEN_KIND_ASTERISK:
        return UNARY_OPERATOR_POINTER_TO;
    case TOKEN_KIND_DEREFERENCE:
        return UNARY_OPERATOR_DEREFERENCE;
        NoDefaultCase();
    }

    Unreachable();
    return _UNARY_OPERATOR_COUNT;
}

Binary_Operator_Kind token_to_binary_operator(Token_Kind kind)
{
    switch (kind)
    {
    case TOKEN_KIND_PLUS:
        return BINARY_OPERATOR_ADDITION;
    case TOKEN_KIND_MINUS:
        return BINARY_OPERATOR_SUBTRACTION;
    case TOKEN_KIND_ASTERISK:
        return BINARY_OPERATOR_MULTIPLICATION;
    case TOKEN_KIND_DIVISION:
        return BINARY_OPERATOR_DIVISION;
    case TOKEN_KIND_REMAINDER:
        return BINARY_OPERATOR_REMAINDER;
    case TOKEN_KIND_BITWISE_SHIFT_RIGHT:
        return BINARY_OPERATOR_BITWISE_SHIFT_RIGHT;
    case TOKEN_KIND_BITWISE_SHIFT_LEFT:
        return BINARY_OPERATOR_BITWISE_SHIFT_LEFT;
    case TOKEN_KIND_BITWISE_AND:
        return BINARY_OPERATOR_BITWISE_AND;
    case TOKEN_KIND_BITWISE_XOR:
        return BINARY_OPERATOR_BITWISE_XOR;
    case TOKEN_KIND_BITWISE_OR:
        return BINARY_OPERATOR_BITWISE_OR;
    case TOKEN_KIND_RELATIONAL_GREATER:
        return BINARY_OPERATOR_RELATIONAL_GREATER;
    case TOKEN_KIND_RELATIONAL_LESS:
        return BINARY_OPERATOR_RELATIONAL_LESS;
    case TOKEN_KIND_RELATIONAL_GREATER_EQUAL:
        return BINARY_OPERATOR_RELATIONAL_GREATER_EQUAL;
    case TOKEN_KIND_RELATIONAL_LESS_EQUAL:
        return BINARY_OPERATOR_RELATIONAL_LESS_EQUAL;
    case TOKEN_KIND_COMPARE_EQUAL:
        return BINARY_OPERATOR_COMPARE_EQUAL;
    case TOKEN_KIND_COMPARE_NOT_EQUAL:
        return BINARY_OPERATOR_COMPARE_NOT_EQUAL;
    case TOKEN_KIND_COMPOUND_PLUS:
        return BINARY_OPERATOR_COMPOUND_ADDITION;
    case TOKEN_KIND_COMPOUND_MINUS:
        return BINARY_OPERATOR_COMPOUND_SUBTRACTION;
    case TOKEN_KIND_COMPOUND_MULTIPLY:
        return BINARY_OPERATOR_COMPOUND_MULTIPLICATION;
    case TOKEN_KIND_COMPOUND_DIVIDE:
        return BINARY_OPERATOR_COMPOUND_DIVISION;
    case TOKEN_KIND_COMPOUND_REMAINDER:
        return BINARY_OPERATOR_COMPOUND_REMAINDER;
    case TOKEN_KIND_COMPOUND_BITWISE_SHIFT_RIGHT:
        return BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_RIGHT;
    case TOKEN_KIND_COMPOUND_BITWISE_SHIFT_LEFT:
        return BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_LEFT;
    case TOKEN_KIND_COMPOUND_BITWISE_AND:
        return BINARY_OPERATOR_COMPOUND_BITWISE_AND;
    case TOKEN_KIND_COMPOUND_BITWISE_XOR:
        return BINARY_OPERATOR_COMPOUND_BITWISE_XOR;
    case TOKEN_KIND_COMPOUND_BITWISE_OR:
        return BINARY_OPERATOR_COMPOUND_BITWISE_OR;
        NoDefaultCase();
    }

    Unreachable();
    return _BINARY_OPERATOR_COUNT;
}

static inline uint32_t next_power2(uint32_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

static void symbol_table_put(Symbol_Table *table, Symbol *sym)
{
    const String name      = sym->name;
    auto         hash      = murmur3_32(name.data, name.length, HASH_SEED);

    auto         pos       = hash & (SYMBOL_TABLE_BUCKET_COUNT - 1);
    auto         buk_index = pos >> SYMBOL_INDEX_SHIFT;

    for (auto bucket = &table->lookup.buckets[buk_index]; bucket; bucket = bucket->next)
    {
        uint32_t count = 0;
        for (auto index = pos & SYMBOL_INDEX_MASK; count < SYMBOL_INDEX_BUCKET_SIZE;
             ++count, index = (index + 1) & SYMBOL_INDEX_MASK)
        {
            auto found_hash = bucket->hash[index];
            if (found_hash == hash)
            {
                auto sym_index           = bucket->index[index];
                table->buffer[sym_index] = sym;
                return;
            }
            else if (found_hash == 0)
            {
                uint32_t offset      = (uint32_t)table->buffer.count;
                bucket->hash[index]  = hash;
                bucket->index[index] = offset;
                table->buffer.add(sym);
                return;
            }
        }

        if (!bucket->next)
        {
            bucket->next = new Symbol_Index;
        }
    }
}

static const Symbol *symbol_table_get(Symbol_Table *root_table, String name, bool recursive = true)
{
    auto hash      = murmur3_32(name.data, name.length, HASH_SEED);

    auto pos       = hash & (SYMBOL_TABLE_BUCKET_COUNT - 1);
    auto buk_index = pos >> SYMBOL_INDEX_SHIFT;

    for (auto table = root_table; table; table = table->parent)
    {
        for (auto bucket = &table->lookup.buckets[buk_index]; bucket; bucket = bucket->next)
        {
            uint32_t count = 0;
            for (auto index = pos & SYMBOL_INDEX_MASK; count < SYMBOL_INDEX_BUCKET_SIZE;
                 ++count, index = (index + 1) & SYMBOL_INDEX_MASK)
            {
                auto found_hash = bucket->hash[index];
                if (found_hash == hash)
                {
                    auto sym_index = bucket->index[index];
                    return table->buffer[sym_index];
                }
            }
        }

        if (!recursive)
            break;
    }

    return nullptr;
}

struct Code_Type_Resolver
{
    Symbol_Table                     symbols;

    uint32_t                         virtual_address[2];
    Symbol_Address::Kind             address_kind;

    Array<Code_Type *>               return_stack;

    Bucket_Array<Symbol, 64>         symbols_allocator;
    Bucket_Array<Unary_Operator, 8>  unary_operators[_UNARY_OPERATOR_COUNT];
    Bucket_Array<Binary_Operator, 8> binary_operators[_BINARY_OPERATOR_COUNT];
};

static bool                 code_type_are_same(Code_Type *a, Code_Type *b, bool recurse_pointer_type = true);

static Code_Node_Type_Cast *code_type_cast(Code_Node *node, Code_Type *to_type, bool explicit_cast = false)
{
    bool cast_success     = false;
    bool implicity_casted = true;

    switch (to_type->kind)
    {
    case CODE_TYPE_INTEGER: {
        auto from_type = node->type->kind;
        cast_success   = (from_type == CODE_TYPE_BOOL);

        if (!cast_success && explicit_cast)
        {
            cast_success = (from_type == CODE_TYPE_REAL);
        }
    }
    break;

    case CODE_TYPE_REAL: {
        auto from_type = node->type->kind;
        cast_success   = (from_type == CODE_TYPE_INTEGER);

        if (!cast_success && explicit_cast)
        {
            cast_success = (from_type == CODE_TYPE_BOOL);
        }
    }
    break;

    case CODE_TYPE_BOOL: {
        auto from_type = node->type->kind;
        cast_success   = (from_type == CODE_TYPE_INTEGER || from_type == CODE_TYPE_REAL || from_type == CODE_TYPE_REAL);
    }
    break;

    case CODE_TYPE_POINTER: {
        auto from_type = node->type;
        if (from_type->kind == CODE_TYPE_POINTER)
        {
            auto to_ptr   = (Code_Type_Pointer *)to_type;
            auto from_ptr = (Code_Type_Pointer *)from_type;
            cast_success  = to_ptr->base_type->kind == CODE_TYPE_NULL || from_ptr->base_type->kind == CODE_TYPE_NULL;
        }
    }
    break;

    case CODE_TYPE_ARRAY_VIEW: {
        auto from_type = node->type;
        if (from_type->kind == CODE_TYPE_STATIC_ARRAY)
        {
            auto to_view  = (Code_Type_Array_View *)to_type;
            auto from_arr = (Code_Type_Static_Array *)from_type;
            cast_success  = code_type_are_same(to_view->element_type, from_arr->element_type);
        }
    }
    break;
    }

    if (!cast_success && explicit_cast)
    {
        implicity_casted = false;
        auto from_type   = node->type->kind;
        cast_success     = (to_type->kind == CODE_TYPE_POINTER && from_type == CODE_TYPE_POINTER) ||
                       (to_type->kind == CODE_TYPE_PROCEDURE && from_type == CODE_TYPE_PROCEDURE) ||
                       (to_type->kind == CODE_TYPE_ARRAY_VIEW && from_type == CODE_TYPE_STATIC_ARRAY);
    }

    if (cast_success)
    {
        Code_Node_Expression *expression = nullptr;

        if (node->kind != CODE_NODE_EXPRESSION)
        {
            expression = new Code_Node_Expression;
            expression->flags = node->flags;
            expression->type  = node->type;
            expression->child = node;
        }
        else
        {
            expression = (Code_Node_Expression *)node;
        }


        auto cast      = new Code_Node_Type_Cast;
        cast->child    = expression;
        cast->type     = to_type;
        cast->implicit = implicity_casted;
        return cast;
    }

    return nullptr;
}

static bool code_type_are_same(Code_Type *a, Code_Type *b, bool recurse_pointer_type)
{
    if (a == b)
        return true;

    if (a->kind == b->kind && a->runtime_size == b->runtime_size && a->alignment == b->alignment)
    {
        switch (a->kind)
        {
        case CODE_TYPE_POINTER: {
            if (recurse_pointer_type)
            {
                auto a_ptr = (Code_Type_Pointer *)a;
                auto b_ptr = (Code_Type_Pointer *)b;
                return code_type_are_same(a_ptr->base_type, b_ptr->base_type, recurse_pointer_type);
            }
            return true;
        }
        break;

        case CODE_TYPE_PROCEDURE: {
            auto a_proc = (Code_Type_Procedure *)a;
            auto b_proc = (Code_Type_Procedure *)b;

            if (a_proc->return_type && b_proc->return_type)
            {
                if (!code_type_are_same(a_proc->return_type, b_proc->return_type, recurse_pointer_type))
                    return false;
            }
            else if (!a_proc->return_type && b_proc->return_type)
                return false;
            else if (!b_proc->return_type && a_proc->return_type)
                return false;

            if (a_proc->argument_count != b_proc->argument_count)
                return false;

            auto a_args    = a_proc->arguments;
            auto b_args    = b_proc->arguments;
            auto arg_count = a_proc->argument_count;

            for (uint64_t arg_index = 0; arg_index < arg_count; ++arg_index)
            {
                if (!code_type_are_same(a_args[arg_index], b_args[arg_index], recurse_pointer_type))
                    return false;
            }

            return true;
        }
        break;

        case CODE_TYPE_STRUCT: {
            auto a_struct = (Code_Type_Struct *)a;
            auto b_struct = (Code_Type_Struct *)b;

            return a_struct->id == b_struct->id;
        }
        break;
        }
        return true;
    }

    return false;
}

Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Literal *root);
Code_Node_Address *code_resolve_identifier(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                           Syntax_Node_Identifier *root);
Code_Node *        code_resolve_expression(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node *root);
Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                      Syntax_Node_Unary_Operator *root);
Code_Node *               code_resolve_binary_operator(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                       Syntax_Node_Binary_Operator *root);
Code_Node_Expression *    code_resolve_root_expression(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                       Syntax_Node_Expression *root);

Code_Node_Assignment *    code_resolve_assignment(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                  Syntax_Node_Assignment *root);
Code_Type *               code_resolve_type(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Type *root,
                                            int depth = 0);

Code_Node_Assignment *    code_resolve_declaration(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                   Syntax_Node_Declaration *root, Code_Type **out_type = nullptr);
Code_Node_Statement *     code_resolve_statement(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                 Syntax_Node_Statement *root);
Code_Node_Block *         code_resolve_block(Code_Type_Resolver *resolver, Symbol_Table *parent_symbols,
                                             Syntax_Node_Block *root);

//
//
//

Code_Node_Literal *code_resolve_literal(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Literal *root)
{
    auto node   = new Code_Node_Literal;

    node->flags = SYMBOL_BIT_CONST_EXPR;

    switch (root->value.kind)
    {
    case Literal::INTEGER: {
        auto symbol = symbol_table_get(&resolver->symbols, "int");
        Assert(symbol->flags & SYMBOL_BIT_TYPE);

        node->type               = symbol->type;
        node->data.integer.value = root->value.data.integer;
    }
    break;

    case Literal::REAL: {
        auto symbol = symbol_table_get(&resolver->symbols, "float");
        Assert(symbol->flags & SYMBOL_BIT_TYPE);

        node->type            = symbol->type;
        node->data.real.value = root->value.data.real;
    }
    break;

    case Literal::STRING: {
        auto symbol = symbol_table_get(&resolver->symbols, "string");
        Assert(symbol->flags & SYMBOL_BIT_TYPE);

        node->type              = symbol->type;
        node->data.string.value = root->value.data.string;
    }
    break;

    case Literal::BOOL: {
        auto symbol = symbol_table_get(&resolver->symbols, "bool");
        Assert(symbol->flags & SYMBOL_BIT_TYPE);

        node->type               = symbol->type;
        node->data.boolean.value = root->value.data.boolean;
    }
    break;

    case Literal::NULL_POINTER: {
        auto symbol = symbol_table_get(&resolver->symbols, "*void");
        Assert(symbol->flags & SYMBOL_BIT_TYPE);

        node->type = symbol->type;
    }
    break;

        NoDefaultCase();
    }

    return node;
}

Code_Node_Address *code_resolve_identifier(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                           Syntax_Node_Identifier *root)
{
    auto symbol = symbol_table_get(symbols, root->name);

    if (symbol)
    {
        auto address     = new Code_Node_Address;
        address->address = &symbol->address;
        address->flags   = symbol->flags;

        if (!(symbol->flags & SYMBOL_BIT_CONSTANT))
            address->flags |= SYMBOL_BIT_LVALUE;

        address->type = symbol->type;
        return address;
    }

    Unimplemented();
    return nullptr;
}

Code_Node_Return *code_resolve_return(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Return *root)
{
    auto node = new Code_Node_Return;

    if (root->expression)
    {
        node->expression = code_resolve_expression(resolver, symbols, root->expression);
        node->flags      = node->expression->flags;
        node->type       = node->expression->type;
    }

    if (resolver->return_stack.count)
    {
        auto return_type = *resolver->return_stack.last();
        if (!code_type_are_same(return_type, node->type))
        {
            Unimplemented();
        }
    }
    else
    {
        Unimplemented();
    }

    return node;
}

Code_Node_Procedure_Call *code_resolve_procedure_call(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                      Syntax_Node_Procedure_Call *root)
{
    auto procedure = code_resolve_root_expression(resolver, symbols, root->procedure);

    if (procedure->type->kind == CODE_TYPE_PROCEDURE)
    {
        auto proc = (Code_Type_Procedure *)procedure->type;

        if (proc->argument_count == root->parameter_count && !proc->is_variadic)
        {
            auto node             = new Code_Node_Procedure_Call;
            node->procedure       = procedure;
            node->type            = proc->return_type;

            node->parameter_count = root->parameter_count;
            node->paraments       = new Code_Node_Expression *[node->parameter_count];

            uint32_t param_index  = 0;
            for (auto param = root->parameters; param; param = param->next, ++param_index)
            {
                auto code_param = code_resolve_root_expression(resolver, symbols, param->expression);

                if (!code_type_are_same(proc->arguments[param_index], code_param->type))
                {
                    auto cast = code_type_cast(code_param->child, proc->arguments[param_index]);

                    if (cast)
                    {
                        code_param->child = cast;
                    }
                    else
                    {
                        Unimplemented();
                    }
                }

                node->paraments[param_index] = code_param;
            }

            node->stack_top = resolver->virtual_address[Symbol_Address::STACK];

            return node;
        }
        else if (proc->is_variadic && root->parameter_count >= proc->argument_count - 1)
        {
            auto node             = new Code_Node_Procedure_Call;
            node->procedure       = procedure;
            node->type            = proc->return_type;

            node->parameter_count = proc->argument_count;
            node->paraments       = new Code_Node_Expression *[node->parameter_count];

            uint32_t param_index  = 0;
            auto     param        = root->parameters;
            for (; param_index < proc->argument_count - 1; param = param->next, ++param_index)
            {
                auto code_param = code_resolve_root_expression(resolver, symbols, param->expression);

                if (!code_type_are_same(proc->arguments[param_index], code_param->type))
                {
                    auto cast = code_type_cast(code_param->child, proc->arguments[param_index]);

                    if (cast)
                    {
                        code_param->child = cast;
                    }
                    else
                    {
                        Unimplemented();
                    }
                }

                node->paraments[param_index] = code_param;
            }

            Code_Node *child = nullptr;

            if (root->parameter_count >= proc->argument_count)
            {
                auto stack_top       = resolver->virtual_address[Symbol_Address::STACK];

                auto address         = new Code_Node_Address;
                address->type        = symbol_table_get(&resolver->symbols, "*void")->type;
                address->child       = nullptr;
                address->offset      = stack_top;
                child                = address;

                auto va_arg_count    = root->parameter_count - proc->argument_count + 1;

                node->variadic_count = va_arg_count;
                node->variadics      = new Code_Node_Expression *[va_arg_count];

                uint64_t index       = 0;
                for (; param; param = param->next, ++index)
                {
                    Assert(index < va_arg_count);
                    auto code_param        = code_resolve_root_expression(resolver, symbols, param->expression);
                    node->variadics[index] = code_param;
                }
            }
            else
            {
                auto null_ptr                = new Code_Node_Literal;
                null_ptr->type               = symbol_table_get(&resolver->symbols, "*void")->type;
                null_ptr->data.pointer.value = 0;
                child                        = null_ptr;
            }

            node->stack_top                            = resolver->virtual_address[Symbol_Address::STACK];

            auto va_arg                                = new Code_Node_Expression;
            va_arg->type                               = child->type;
            va_arg->child                              = child;

            node->paraments[node->parameter_count - 1] = va_arg;

            return node;
        }
        else
        {
            Unimplemented();
        }
    }

    Unimplemented();

    return nullptr;
}

Code_Node_Address *code_resolve_subscript(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                          Syntax_Node_Subscript *root)
{
    auto expression = code_resolve_root_expression(resolver, symbols, root->expression);
    auto subscript  = code_resolve_root_expression(resolver, symbols, root->subscript);

    if (expression->type->kind == CODE_TYPE_ARRAY_VIEW || expression->type->kind == CODE_TYPE_STATIC_ARRAY)
    {
        if (subscript->type->kind == CODE_TYPE_INTEGER)
        {
            auto node        = new Code_Node_Subscript;
            node->expression = expression;
            node->subscript  = subscript;

            node->flags      = expression->flags | SYMBOL_BIT_LVALUE;

            if (expression->type->kind == CODE_TYPE_ARRAY_VIEW)
            {
                auto type  = (Code_Type_Array_View *)expression->type;
                node->type = type->element_type;
            }
            else if (expression->type->kind == CODE_TYPE_STATIC_ARRAY)
            {
                auto type  = (Code_Type_Static_Array *)expression->type;
                node->type = type->element_type;
            }

            auto address   = new Code_Node_Address;
            address->type  = node->type;
            address->flags = node->flags;
            address->child = node;

            return address;
        }
        else
        {
            Unimplemented();
        }
    }
    else
    {
        Unimplemented();
    }

    return nullptr;
}

Code_Node_Type_Cast *code_resolve_type_cast(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                            Syntax_Node_Type_Cast *root)
{
    auto expression = code_resolve_root_expression(resolver, symbols, root->expression);
    auto type       = code_resolve_type(resolver, symbols, root->type);

    auto cast       = code_type_cast(expression, type, true);

    if (!cast)
    {
        Unimplemented();
    }

    return cast;
}

Code_Node_Literal *code_resolve_size_of(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Size_Of *root)
{
    auto type                = code_resolve_type(resolver, symbols, root->type);

    auto node                = new Code_Node_Literal;
    node->type               = symbol_table_get(&resolver->symbols, "int")->type;
    node->data.integer.value = type->runtime_size;

    return node;
}

Code_Node *code_resolve_expression(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node *root)
{
    switch (root->kind)
    {
    case SYNTAX_NODE_LITERAL:
        return code_resolve_literal(resolver, symbols, (Syntax_Node_Literal *)root);

    case SYNTAX_NODE_TYPE_CAST:
        return code_resolve_type_cast(resolver, symbols, (Syntax_Node_Type_Cast *)root);

    case SYNTAX_NODE_IDENTIFIER:
        return code_resolve_identifier(resolver, symbols, (Syntax_Node_Identifier *)root);

    case SYNTAX_NODE_UNARY_OPERATOR:
        return code_resolve_unary_operator(resolver, symbols, (Syntax_Node_Unary_Operator *)root);

    case SYNTAX_NODE_BINARY_OPERATOR:
        return code_resolve_binary_operator(resolver, symbols, (Syntax_Node_Binary_Operator *)root);

    case SYNTAX_NODE_ASSIGNMENT:
        return code_resolve_assignment(resolver, symbols, (Syntax_Node_Assignment *)root);

    case SYNTAX_NODE_RETURN:
        return code_resolve_return(resolver, symbols, (Syntax_Node_Return *)root);

    case SYNTAX_NODE_PROCEDURE_CALL:
        return code_resolve_procedure_call(resolver, symbols, (Syntax_Node_Procedure_Call *)root);

    case SYNTAX_NODE_SUBSCRIPT:
        return code_resolve_subscript(resolver, symbols, (Syntax_Node_Subscript *)root);

    case SYNTAX_NODE_SIZE_OF:
        return code_resolve_size_of(resolver, symbols, (Syntax_Node_Size_Of *)root);

        NoDefaultCase();
    }
    return nullptr;
}

Code_Node_Unary_Operator *code_resolve_unary_operator(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                      Syntax_Node_Unary_Operator *root)
{
    auto  child     = code_resolve_expression(resolver, symbols, root->child);

    auto  op_kind   = token_to_unary_operator(root->op);

    auto &operators = resolver->unary_operators[op_kind];
    ForBucketArray(bucket, operators)
    {
        ForBucket(index, bucket, operators)
        {
            auto op    = &bucket->data[index];

            bool found = false;
            if (code_type_are_same(op->parameter, child->type))
            {
                found = true;
            }
            else
            {
                auto cast = code_type_cast(child, op->output);
                if (cast)
                {
                    child = cast;
                    found = true;
                }
            }

            if (found)
            {
                auto node     = new Code_Node_Unary_Operator;
                node->type    = op->output;
                node->child   = child;
                node->op_kind = op_kind;

                if (child->flags & SYMBOL_BIT_CONST_EXPR)
                    node->flags |= SYMBOL_BIT_CONST_EXPR;

                return node;
            }
        }
    }

    if (op_kind == UNARY_OPERATOR_POINTER_TO && (child->flags & SYMBOL_BIT_LVALUE))
    {
        auto node       = new Code_Node_Unary_Operator;
        auto type       = new Code_Type_Pointer;

        type->base_type = child->type;

        node->type      = type;
        node->child     = child;
        node->op_kind   = op_kind;

        if (child->flags & SYMBOL_BIT_CONST_EXPR)
            node->flags |= SYMBOL_BIT_CONST_EXPR;

        return node;
    }

    else if (op_kind == UNARY_OPERATOR_DEREFERENCE && (child->type->kind == CODE_TYPE_POINTER))
    {
        auto pointer_type = (Code_Type_Pointer *)child->type;

        if (pointer_type->base_type->kind != CODE_TYPE_NULL)
        {
            auto node     = new Code_Node_Unary_Operator;
            auto type     = ((Code_Type_Pointer *)child->type)->base_type;

            node->type    = type;
            node->child   = child;
            node->op_kind = op_kind;

            if (child->flags & SYMBOL_BIT_CONST_EXPR)
                node->flags |= SYMBOL_BIT_CONST_EXPR;

            return node;
        }

        Unimplemented();
    }

    Unimplemented();

    return nullptr;
}

Code_Node *code_resolve_binary_operator(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                        Syntax_Node_Binary_Operator *root)
{
    auto left = code_resolve_expression(resolver, symbols, root->left);

    if (root->op == TOKEN_KIND_PERIOD)
    {
        if (left->kind != CODE_NODE_ADDRESS)
        {
            Unimplemented();
        }

        if (root->right->kind != SYNTAX_NODE_IDENTIFIER)
        {
            Unimplemented();
        }

        bool valid_type = false;
        auto base_type  = left->type;

        for (int depth = 0; depth < 2; ++depth)
        {
            if (base_type->kind == CODE_TYPE_STRUCT || base_type->kind == CODE_TYPE_ARRAY_VIEW ||
                base_type->kind == CODE_TYPE_STATIC_ARRAY)
            {
                valid_type = true;
                break;
            }
            else if (base_type->kind == CODE_TYPE_POINTER)
            {
                auto ptr  = (Code_Type_Pointer *)base_type;
                base_type = ptr->base_type;
            }
            else
            {
                Unimplemented();
            }
        }

        if (valid_type)
        {
            auto iden = (Syntax_Node_Identifier *)root->right;

            switch (base_type->kind)
            {
            case CODE_TYPE_STRUCT: {
                auto type   = (Code_Type_Struct *)base_type;
                auto symbol = symbol_table_get(symbols, type->name);
                Assert(symbol && symbol->type->kind == CODE_TYPE_STRUCT &&
                       symbol->address.kind == Symbol_Address::CODE);

                auto block  = (Code_Node_Block *)symbol->address.memory;

                auto member = symbol_table_get(&block->symbols, iden->name, false);

                if (member)
                {
                    Assert(member->address.kind == Symbol_Address::STACK);

                    auto code_node  = (Code_Node_Address *)left;
                    code_node->type = member->type;
                    code_node->offset += (uint64_t)member->address.memory;

                    return code_node;
                }
                else
                {
                    Unimplemented();
                }
            }
            break;

            case CODE_TYPE_STATIC_ARRAY: {
                auto code_node = (Code_Node_Address *)left;

                if (iden->name == "data")
                {
                    auto type           = (Code_Type_Static_Array *)base_type;
                    auto ptr_type       = new Code_Type_Pointer;
                    ptr_type->base_type = type->element_type;
                    code_node->type     = ptr_type;
                    return code_node;
                }
                else if (iden->name == "count")
                {
                    auto type                = (Code_Type_Static_Array *)base_type;
                    auto node                = new Code_Node_Literal;
                    node->type               = symbol_table_get(&resolver->symbols, "int")->type;
                    node->data.integer.value = type->element_count;
                    return node;
                }
                else
                {
                    Unimplemented();
                }
            }
            break;

            case CODE_TYPE_ARRAY_VIEW: {
                auto code_node = (Code_Node_Address *)left;

                if (iden->name == "count")
                {
                    code_node->type = symbol_table_get(&resolver->symbols, "int")->type;
                    code_node->offset += 0;
                }
                else if (iden->name == "data")
                {
                    auto type       = (Code_Type_Array_View *)base_type;
                    code_node->type = type->element_type;
                    code_node->offset += sizeof(int64_t);
                }
                else
                {
                    Unimplemented();
                }

                return code_node;
            }
            break;
            }
        }
        else
        {
            Unimplemented();
        }
    }

    else
    {
        auto  right     = code_resolve_expression(resolver, symbols, root->right);

        auto  op_kind   = token_to_binary_operator(root->op);

        auto &operators = resolver->binary_operators[op_kind];

        ForBucketArray(bucket, operators)
        {
            ForBucket(index, bucket, operators)
            {
                auto op          = &bucket->data[index];

                bool left_match  = false;
                bool right_match = false;

                if (code_type_are_same(op->parameters[0], left->type, false))
                {
                    left_match = true;
                }
                else
                {
                    auto cast_left = code_type_cast(left, op->parameters[0]);
                    if (cast_left)
                    {
                        left       = cast_left;
                        left_match = true;
                    }
                }

                if (code_type_are_same(op->parameters[1], right->type))
                {
                    right_match = true;
                }
                else
                {
                    auto cast_right = code_type_cast(right, op->parameters[1]);
                    if (cast_right)
                    {
                        right       = cast_right;
                        right_match = true;
                    }
                }

                if (left_match && right_match && (!op->compound || (op->compound && (left->flags & SYMBOL_BIT_LVALUE))))
                {
                    auto node     = new Code_Node_Binary_Operator;

                    node->type    = op->parameters[0]->kind == CODE_TYPE_POINTER ? left->type : op->output;
                    node->left    = left;
                    node->right   = right;
                    node->flags   = left->flags & right->flags;
                    node->op_kind = op_kind;

                    return node;
                }
            }
        }
    }

    Unimplemented();

    return nullptr;
}

Code_Node_Expression *code_resolve_root_expression(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                                   Syntax_Node_Expression *root)
{
    auto                  child      = code_resolve_expression(resolver, symbols, root->child);

    Code_Node_Expression *expression = new Code_Node_Expression;
    expression->child                = child;
    expression->flags                = child->flags;
    expression->type                 = child->type;

    return expression;
}

Code_Node_Assignment *code_resolve_assignment(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                              Syntax_Node_Assignment *root)
{
    auto destination = code_resolve_root_expression(resolver, symbols, root->left);
    auto value       = code_resolve_root_expression(resolver, symbols, root->right);

    if (destination->flags & SYMBOL_BIT_LVALUE)
    {
        if (!(destination->flags & SYMBOL_BIT_CONSTANT))
        {
            bool match = false;

            if (code_type_are_same(destination->type, value->type))
            {
                match = true;
            }
            else
            {
                auto cast = code_type_cast(value->child, destination->type);
                if (cast)
                {
                    value->child = cast;
                    match        = true;
                }
            }

            if (match)
            {
                auto node         = new Code_Node_Assignment;
                node->destination = destination;
                node->value       = value;
                node->type        = destination->type;
                return node;
            }
        }
    }

    Unimplemented();
    return nullptr;
}

Code_Type *code_resolve_type(Code_Type_Resolver *resolver, Symbol_Table *symbols, Syntax_Node_Type *root, int depth)
{
    depth += 1;

    switch (root->id)
    {
    case Syntax_Node_Type::INT: {
        auto symbol = symbol_table_get(&resolver->symbols, "int");
        return symbol->type;
    }
    break;

    case Syntax_Node_Type::FLOAT: {
        auto symbol = symbol_table_get(&resolver->symbols, "float");
        return symbol->type;
    }
    break;

    case Syntax_Node_Type::BOOL: {
        auto symbol = symbol_table_get(&resolver->symbols, "bool");
        return symbol->type;
    }
    break;

    case Syntax_Node_Type::VARIADIC_ARGUMENT: {
        if (depth == 1)
        {
            auto symbol = symbol_table_get(&resolver->symbols, "*void");
            return symbol->type;
        }
        else
        {
            Unimplemented();
        }
    }
    break;

    case Syntax_Node_Type::POINTER: {
        auto type = new Code_Type_Pointer;

        auto ptr  = (Syntax_Node_Type *)root->type;

        if (ptr->id == Syntax_Node_Type::VOID)
        {
            type->base_type = new Code_Type;
        }
        else
        {
            type->base_type = code_resolve_type(resolver, symbols, ptr);
        }

        return type;
    }
    break;

    case Syntax_Node_Type::PROCEDURE: {
        auto node            = (Syntax_Node_Procedure_Prototype *)root->type;

        auto type            = new Code_Type_Procedure;

        type->argument_count = node->argument_count;
        type->arguments      = new Code_Type *[type->argument_count];

        auto     last_index  = type->argument_count - 1;

        uint64_t arg_index   = 0;
        for (auto arg = node->arguments_type; arg; arg = arg->next, ++arg_index)
        {
            type->arguments[arg_index] = code_resolve_type(resolver, symbols, arg->type);

            if (arg_index == last_index)
            {
                type->is_variadic = (arg->type->id == Syntax_Node_Type::VARIADIC_ARGUMENT);
            }
            else if (arg->type->id == Syntax_Node_Type::VARIADIC_ARGUMENT)
            {
                Unimplemented();
            }
        }

        if (node->return_type)
        {
            type->return_type = code_resolve_type(resolver, symbols, node->return_type);
        }

        return type;
    }
    break;

    case Syntax_Node_Type::IDENTIFIER: {
        auto node   = (Syntax_Node_Identifier *)root->type;

        auto symbol = symbol_table_get(symbols, node->name);
        if (symbol && symbol->flags & SYMBOL_BIT_TYPE)
        {
            Assert(symbol->type->kind == CODE_TYPE_STRUCT && symbol->address.kind == Symbol_Address::CODE);
            return symbol->type;
        }
        else
        {
            Unimplemented();
        }
    }
    break;

    case Syntax_Node_Type::TYPE_OF: {
        auto node       = (Syntax_Node_Type_Of *)root->type;
        auto expression = code_resolve_root_expression(resolver, symbols, node->expression);
        return expression->type;
    }
    break;

    case Syntax_Node_Type::ARRAY_VIEW: {
        auto node          = (Syntax_Node_Array_View *)root->type;

        auto type          = new Code_Type_Array_View;
        type->element_type = code_resolve_type(resolver, symbols, node->element_type);
        return type;
    }
    break;

    case Syntax_Node_Type::STATIC_ARRAY: {
        auto node          = (Syntax_Node_Static_Array *)root->type;

        auto type          = new Code_Type_Static_Array;
        type->element_type = code_resolve_type(resolver, symbols, node->element_type);
        type->alignment    = type->element_type->alignment;

        auto expr          = code_resolve_root_expression(resolver, symbols, node->expression);

        if (expr->flags & SYMBOL_BIT_CONST_EXPR)
        {
            if (expr->type->kind == CODE_TYPE_INTEGER)
            {
                Assert(expr->child->kind == CODE_NODE_LITERAL);

                auto literal        = (Code_Node_Literal *)expr->child;

                type->element_count = literal->data.integer.value;
                type->runtime_size  = type->element_count * type->element_type->runtime_size;

                return type;
            }
            else
            {
                Unimplemented();
            }
        }
        else
        {
            Unimplemented();
        }
    }
    break;

    default: {
        Unimplemented();
    }
    }

    return nullptr;
}

Code_Node_Assignment *code_resolve_declaration(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                               Syntax_Node_Declaration *root, Code_Type **out_type)
{
    String sym_name = root->identifier;

    Assert(root->type || root->initializer);

    if (!symbol_table_get(symbols, sym_name, false))
    {
        Symbol *symbol   = resolver->symbols_allocator.add();
        symbol->name     = sym_name;
        symbol->type     = root->type ? code_resolve_type(resolver, symbols, root->type) : nullptr;
        symbol->flags    = root->flags;
        symbol->location = root->location;
        symbol_table_put(symbols, symbol);

        if (root->flags & SYMBOL_BIT_CONSTANT && !root->initializer)
        {
            // Error: Constanst expression must be initialized
            Unimplemented();
        }

        bool                  infer_type     = symbol->type == nullptr;

        Code_Node_Block *     procedure_body = nullptr;
        Code_Node_Expression *expression     = nullptr;

        Code_Type *           type           = nullptr;

        auto ResolveProcedure = [&resolver, &type, &procedure_body, symbols, symbol](Syntax_Node_Procedure *proc) {
            auto proc_type            = new Code_Type_Procedure;

            proc_type->argument_count = proc->argument_count;
            proc_type->arguments      = new Code_Type *[proc_type->argument_count];

            if (proc->return_type)
            {
                proc_type->return_type = code_resolve_type(resolver, &resolver->symbols, proc->return_type);
            }

            auto proc_symbols    = new Symbol_Table;
            proc_symbols->parent = symbols;

            auto stack_top       = resolver->virtual_address[Symbol_Address::STACK];
            auto address_kind    = resolver->address_kind;

            if (proc_type->return_type)
                resolver->virtual_address[Symbol_Address::STACK] = proc_type->return_type->runtime_size;
            else
                resolver->virtual_address[Symbol_Address::STACK] = 0;

            resolver->address_kind = Symbol_Address::STACK;

            auto     last_index    = proc_type->argument_count - 1;

            uint64_t arg_index     = 0;
            for (auto arg = proc->arguments; arg; arg = arg->next, ++arg_index)
            {
                auto assign = code_resolve_declaration(resolver, proc_symbols, arg->declaration,
                                                       &proc_type->arguments[arg_index]);
                Assert(assign == nullptr);

                auto decl_type = arg->declaration->type;
                if (arg_index == last_index)
                {
                    proc_type->is_variadic = (decl_type->id == Syntax_Node_Type::VARIADIC_ARGUMENT);
                }
                else if (decl_type->id == Syntax_Node_Type::VARIADIC_ARGUMENT)
                {
                    Unimplemented();
                }
            }

            type = proc_type;
            if (!symbol->type)
                symbol->type = proc_type;

            resolver->return_stack.add(proc_type->return_type);
            procedure_body = code_resolve_block(resolver, proc_symbols, proc->body);
            resolver->return_stack.count -= 1;

            resolver->virtual_address[Symbol_Address::STACK] = stack_top;
            resolver->address_kind                           = address_kind;
        };

        auto ResolveStruct = [&type, resolver, symbols, symbol](Syntax_Node_Struct *struct_node) {
            auto struct_type                                 = new Code_Type_Struct;

            struct_type->name                                = symbol->name;
            struct_type->member_count                        = struct_node->member_count;
            struct_type->members                             = new Code_Type_Struct::Member[struct_type->member_count];

            auto stack_top                                   = resolver->virtual_address[Symbol_Address::STACK];
            auto address_kind                                = resolver->address_kind;
            resolver->address_kind                           = Symbol_Address::STACK;
            resolver->virtual_address[Symbol_Address::STACK] = 0;

            auto block                                       = new Code_Node_Block;
            auto struct_symbols                              = &block->symbols;
            struct_symbols->parent                           = symbols;

            struct_type->id                                  = (uint64_t)block;

            symbol->type                                     = struct_type;
            symbol->address                                  = symbol_address_code(block);

            symbol->flags |= SYMBOL_BIT_TYPE;

            uint32_t alignment  = 0;
            auto     dst_member = struct_type->members;

            for (auto member = struct_node->members; member; member = member->next)
            {
                if (code_resolve_declaration(resolver, struct_symbols, member->declaration, &dst_member->type))
                {
                    Unimplemented();
                }

                dst_member->name   = member->declaration->identifier;
                dst_member->offset = resolver->virtual_address[Symbol_Address::STACK] - dst_member->type->runtime_size;

                if (alignment == 0)
                    alignment = dst_member->type->alignment;

                dst_member += 1;
            }

            auto runtime_size                                = resolver->virtual_address[Symbol_Address::STACK];
            runtime_size                                     = AlignPower2Up(runtime_size, alignment);

            struct_type->runtime_size                        = runtime_size;
            struct_type->alignment                           = alignment;

            resolver->virtual_address[Symbol_Address::STACK] = stack_top;
            resolver->address_kind                           = address_kind;
        };

        if (infer_type)
        {
            Assert(root->initializer);

            if (root->initializer->kind == SYNTAX_NODE_PROCEDURE)
            {
                ResolveProcedure((Syntax_Node_Procedure *)root->initializer);
            }
            else if (root->initializer->kind == SYNTAX_NODE_STRUCT)
            {
                ResolveStruct((Syntax_Node_Struct *)root->initializer);

                if (out_type)
                    *out_type = symbol->type;

                return nullptr;
            }
            else
            {
                Assert(root->initializer->kind == SYNTAX_NODE_EXPRESSION);
                expression =
                    code_resolve_root_expression(resolver, symbols, (Syntax_Node_Expression *)root->initializer);
                type = expression->type;
            }
        }
        else if (root->initializer)
        {
            if (root->initializer->kind == SYNTAX_NODE_PROCEDURE)
            {
                ResolveProcedure((Syntax_Node_Procedure *)root->initializer);
            }
            else
            {
                Assert(root->initializer->kind == SYNTAX_NODE_EXPRESSION);
                expression =
                    code_resolve_root_expression(resolver, symbols, (Syntax_Node_Expression *)root->initializer);
                type = expression->type;
            }
        }

        if (!infer_type && root->initializer)
        {
            if (!code_type_are_same(symbol->type, type))
            {
                if (expression)
                {
                    auto cast = code_type_cast(expression->child, symbol->type);
                    if (cast)
                    {
                        expression->child = cast;
                    }
                    else
                    {
                        Unimplemented();
                    }
                }
                else
                {
                    Unimplemented();
                }
            }
        }

        if (!symbol->type)
            symbol->type = type;

        if (out_type)
            *out_type = symbol->type;

        if (symbol->type->kind != CODE_TYPE_PROCEDURE)
        {
            auto     address = resolver->virtual_address[resolver->address_kind];

            uint32_t size    = symbol->type->runtime_size;
            address          = AlignPower2Up(address, symbol->type->alignment);
            symbol->address  = symbol_address_offset(address, resolver->address_kind);
            address += size;

            resolver->virtual_address[resolver->address_kind] = address;

            if (root->initializer)
            {
                auto address     = new Code_Node_Address;
                address->address = &symbol->address;
                address->flags   = symbol->flags;
                address->flags |= SYMBOL_BIT_LVALUE;
                address->type           = symbol->type;

                auto destination        = new Code_Node_Expression;
                destination->child      = address;
                destination->flags      = address->flags;
                destination->type       = address->type;

                auto assignment         = new Code_Node_Assignment;
                assignment->type        = destination->type;
                assignment->destination = destination;
                assignment->value       = expression;
                assignment->flags |= destination->flags;

                if (root->flags & SYMBOL_BIT_CONSTANT && expression->flags & SYMBOL_BIT_CONST_EXPR)
                {
                    address->flags |= SYMBOL_BIT_CONST_EXPR;
                }

                return assignment;
            }

            return nullptr;
        }
        else
        {
            if (root->flags & SYMBOL_BIT_CONSTANT)
            {
                symbol->address = symbol_address_code(procedure_body);
            }
            else
            {
                auto     address_offset = resolver->virtual_address[resolver->address_kind];

                uint32_t size           = symbol->type->runtime_size;
                address_offset          = AlignPower2Up(address_offset, symbol->type->alignment);
                symbol->address         = symbol_address_offset(address_offset, resolver->address_kind);
                address_offset += size;

                resolver->virtual_address[resolver->address_kind] = address_offset;

                auto address                                      = new Code_Node_Address;
                address->address                                  = &symbol->address;
                address->flags                                    = symbol->flags;
                address->flags |= SYMBOL_BIT_LVALUE;
                address->type           = symbol->type;

                auto destination        = new Code_Node_Expression;
                destination->child      = address;
                destination->flags      = address->flags;
                destination->type       = address->type;

                auto sym_addr           = new Symbol_Address;
                *sym_addr               = symbol_address_code(procedure_body);

                auto source             = new Code_Node_Address;
                source->type            = symbol->type;
                source->flags           = symbol->flags;
                source->address         = sym_addr;

                auto value              = new Code_Node_Expression;
                value->flags            = source->flags;
                value->type             = source->type;
                value->child            = source;

                auto assignment         = new Code_Node_Assignment;
                assignment->type        = destination->type;
                assignment->destination = destination;
                assignment->value       = value;
                assignment->flags |= destination->flags;

                return assignment;
            }

            return nullptr;
        }
    }

    // Already defined in this scope previously
    Unimplemented();

    return nullptr;
}

Code_Node_Statement *code_resolve_statement(Code_Type_Resolver *resolver, Symbol_Table *symbols,
                                            Syntax_Node_Statement *root)
{
    auto node = root->node;

    switch (node->kind)
    {
    case SYNTAX_NODE_EXPRESSION: {
        auto expression = code_resolve_root_expression(resolver, symbols, (Syntax_Node_Expression *)node);
        Code_Node_Statement *statement = new Code_Node_Statement;
        statement->source_row          = node->location.start_row;
        statement->node                = expression;
        statement->type                = expression->type;
        return statement;
    }
    break;

    case SYNTAX_NODE_IF: {
        auto if_node   = (Syntax_Node_If *)node;

        auto condition = code_resolve_root_expression(resolver, symbols, if_node->condition);

        auto boolean   = symbol_table_get(symbols, "bool");
        if (!code_type_are_same(condition->child->type, boolean->type))
        {
            auto cast = code_type_cast(condition->child, boolean->type);
            if (cast)
            {
                condition->child = cast;
            }
            else
            {
                Unimplemented();
            }
        }

        auto if_code            = new Code_Node_If;
        if_code->condition      = condition;
        if_code->true_statement = code_resolve_statement(resolver, symbols, if_node->true_statement);

        if (if_node->false_statement)
        {
            if_code->false_statement = code_resolve_statement(resolver, symbols, if_node->false_statement);
        }

        Code_Node_Statement *statement = new Code_Node_Statement;
        statement->source_row          = node->location.start_row;
        statement->node                = if_code;
        return statement;
    }
    break;

    case SYNTAX_NODE_FOR: {
        auto for_node            = (Syntax_Node_For *)node;

        auto for_code            = new Code_Node_For;
        for_code->symbols.parent = symbols;

        auto stack_top           = resolver->virtual_address[Symbol_Address::STACK];

        for_code->initialization = code_resolve_statement(resolver, &for_code->symbols, for_node->initialization);

        auto condition           = code_resolve_root_expression(resolver, &for_code->symbols, for_node->condition);

        auto boolean             = symbol_table_get(&for_code->symbols, "bool");
        if (!code_type_are_same(condition->child->type, boolean->type))
        {
            auto cast = code_type_cast(condition->child, boolean->type);
            if (cast)
            {
                condition->child = cast;
            }
            else
            {
                Unimplemented();
            }
        }

        for_code->condition = condition;
        for_code->increment = code_resolve_root_expression(resolver, &for_code->symbols, for_node->increment);
        for_code->body      = code_resolve_statement(resolver, &for_code->symbols, for_node->body);

        resolver->virtual_address[Symbol_Address::STACK] = stack_top;

        Code_Node_Statement *statement                   = new Code_Node_Statement;
        statement->source_row                            = node->location.start_row;
        statement->node                                  = for_code;
        return statement;
    }
    break;

    case SYNTAX_NODE_WHILE: {
        auto while_node = (Syntax_Node_While *)node;

        auto condition  = code_resolve_root_expression(resolver, symbols, while_node->condition);

        auto boolean    = symbol_table_get(symbols, "bool");
        if (!code_type_are_same(condition->child->type, boolean->type))
        {
            auto cast = code_type_cast(condition->child, boolean->type);
            if (cast)
            {
                condition->child = cast;
            }
            else
            {
                Unimplemented();
            }
        }

        auto while_code                = new Code_Node_While;
        while_code->condition          = condition;
        while_code->body               = code_resolve_statement(resolver, symbols, while_node->body);

        Code_Node_Statement *statement = new Code_Node_Statement;
        statement->source_row          = node->location.start_row;
        statement->node                = while_code;
        return statement;
    }
    break;

    case SYNTAX_NODE_DO: {
        auto do_node    = (Syntax_Node_Do *)node;

        auto body       = code_resolve_statement(resolver, symbols, do_node->body);

        auto do_symbols = symbols;
        if (body->node->kind == CODE_NODE_BLOCK)
        {
            auto block = (Code_Node_Block *)body->node;
            do_symbols = &block->symbols;
        }

        auto condition = code_resolve_root_expression(resolver, do_symbols, do_node->condition);

        auto boolean   = symbol_table_get(do_symbols, "bool");
        if (!code_type_are_same(condition->child->type, boolean->type))
        {
            auto cast = code_type_cast(condition->child, boolean->type);
            if (cast)
            {
                condition->child = cast;
            }
            else
            {
                Unimplemented();
            }
        }

        auto do_code                   = new Code_Node_Do;
        do_code->body                  = body;
        do_code->condition             = condition;

        Code_Node_Statement *statement = new Code_Node_Statement;
        statement->source_row          = node->location.start_row;
        statement->node                = do_code;
        return statement;
    }
    break;

    case SYNTAX_NODE_DECLARATION: {
        auto initialization = code_resolve_declaration(resolver, symbols, (Syntax_Node_Declaration *)node);

        if (initialization)
        {
            Code_Node_Statement *statement = new Code_Node_Statement;
            statement->source_row          = node->location.start_row;
            statement->node                = initialization;
            return statement;
        }

        return nullptr;
    }
    break;

    case SYNTAX_NODE_STATEMENT: {
        // This happens when a block is added inside another block
        auto statement = (Syntax_Node_Statement *)node;
        return code_resolve_statement(resolver, symbols, statement);
    }
    break;

    case SYNTAX_NODE_BLOCK: {
        auto                 block     = code_resolve_block(resolver, symbols, (Syntax_Node_Block *)node);
        Code_Node_Statement *statement = new Code_Node_Statement;
        statement->source_row          = node->location.start_row;
        statement->node                = block;
        statement->type                = nullptr;
        return statement;
    }
    break;

        NoDefaultCase();
    }

    return nullptr;
}

Code_Node_Block *code_resolve_block(Code_Type_Resolver *resolver, Symbol_Table *parent_symbols, Syntax_Node_Block *root)
{
    Code_Node_Block *block = new Code_Node_Block;

    block->type            = nullptr;
    block->symbols.parent  = parent_symbols;

    Code_Node_Statement  statement_stub_head;
    Code_Node_Statement *parent_statement = &statement_stub_head;

    auto                 stack_top        = resolver->virtual_address[Symbol_Address::STACK];

    uint32_t             statement_count  = 0;
    for (auto statement = root->statements; statement; statement = statement->next)
    {
        auto code_statement = code_resolve_statement(resolver, &block->symbols, statement);
        if (code_statement)
        {
            parent_statement->next = code_statement;
            parent_statement       = code_statement;
            statement_count += 1;
        }
    }

    resolver->virtual_address[Symbol_Address::STACK] = stack_top;

    block->statement_head                            = statement_stub_head.next;
    block->statement_count                           = statement_count;

    return block;
}

Array_View<Code_Node_Assignment *> code_resolve_global_scope(Code_Type_Resolver *resolver, Symbol_Table *parent_symbols,
                                                             Syntax_Node_Global_Scope *global)
{
    Array<Code_Node_Assignment *> global_exe;

    for (auto decl = global->declarations; decl; decl = decl->next)
    {
        auto assign = code_resolve_declaration(resolver, parent_symbols, decl->declaration);
        if (assign)
        {
            if ((assign->value->flags & SYMBOL_BIT_CONST_EXPR))
            {
                global_exe.add(assign);
            }
            else
            {
                Unimplemented();
            }
        }
    }

    return global_exe;
}

int main()
{
    String content = read_entire_file("Simple.kano");

    auto   builder = new String_Builder;

    Parser parser;
    parser_init(&parser, content, builder);

    auto node = parse_global_scope(&parser);

    if (parser.error_count)
    {
        for (auto error = parser.error.first.next; error; error = error->next)
        {
            auto row     = error->location.start_row;
            auto column  = error->location.start_column;
            auto message = error->message.data;
            fprintf(stderr, "%zu,%zu: %s\n", row, column, message);
        }
        return 1;
    }

    {
        auto fp = fopen("syntax.txt", "wb");
        print_syntax(node, fp);
        fclose(fp);
    }

    Code_Type_Resolver resolver;

    Code_Type *        CompilerTypes[_CODE_TYPE_COUNT];

    {
        CompilerTypes[CODE_TYPE_NULL]    = new Code_Type;
        CompilerTypes[CODE_TYPE_INTEGER] = new Code_Type_Integer;
        CompilerTypes[CODE_TYPE_REAL]    = new Code_Type_Real;
        CompilerTypes[CODE_TYPE_BOOL]    = new Code_Type_Bool;
    }

    {
        auto pointer_type       = new Code_Type_Pointer;
        pointer_type->base_type = new Code_Type;

        auto sym                = resolver.symbols_allocator.add();
        sym->name               = "*void";
        sym->type               = pointer_type;
        sym->flags              = SYMBOL_BIT_CONSTANT | SYMBOL_BIT_TYPE;
        symbol_table_put(&resolver.symbols, sym);

        CompilerTypes[CODE_TYPE_POINTER] = pointer_type;
    }

    {
        auto sym   = resolver.symbols_allocator.add();
        sym->name  = "int";
        sym->type  = CompilerTypes[CODE_TYPE_INTEGER];
        sym->flags = SYMBOL_BIT_CONSTANT | SYMBOL_BIT_TYPE;
        symbol_table_put(&resolver.symbols, sym);
    }

    {
        auto sym   = resolver.symbols_allocator.add();
        sym->name  = "float";
        sym->type  = CompilerTypes[CODE_TYPE_REAL];
        sym->flags = SYMBOL_BIT_CONSTANT | SYMBOL_BIT_TYPE;
        symbol_table_put(&resolver.symbols, sym);
    }

    {
        auto sym   = resolver.symbols_allocator.add();
        sym->name  = "bool";
        sym->type  = CompilerTypes[CODE_TYPE_BOOL];
        sym->flags = SYMBOL_BIT_CONSTANT | SYMBOL_BIT_TYPE;
        symbol_table_put(&resolver.symbols, sym);
    }

    {
        auto block             = new Code_Node_Block;
        block->symbols.parent  = &resolver.symbols;

        auto length            = resolver.symbols_allocator.add();
        length->name           = "length";
        length->address.kind   = Symbol_Address::STACK;
        length->address.memory = 0;
        length->type           = symbol_table_get(&resolver.symbols, "int")->type;

        auto data              = resolver.symbols_allocator.add();
        data->name             = "data";
        data->address.kind     = Symbol_Address::STACK;
        data->address.memory   = (uint8_t *)sizeof(int64_t);
        data->type             = symbol_table_get(&resolver.symbols, "*void")->type;

        symbol_table_put(&block->symbols, length);
        symbol_table_put(&block->symbols, data);

        auto type               = new Code_Type_Struct;
        type->alignment         = sizeof(int64_t);
        type->runtime_size      = sizeof(String);
        type->name              = "string";
        type->id                = (uint64_t)type;
        type->member_count      = 2;
        type->members           = new Code_Type_Struct::Member[type->member_count];

        type->members[0].name   = length->name;
        type->members[0].offset = (uint64_t)length->address.memory;
        type->members[0].type   = length->type;

        type->members[1].name   = data->name;
        type->members[1].offset = (uint64_t)length->address.memory;
        type->members[1].type   = data->type;

        auto sym                = resolver.symbols_allocator.add();
        sym->name               = "string";
        sym->type               = type;
        sym->flags              = SYMBOL_BIT_CONSTANT | SYMBOL_BIT_TYPE;
        sym->address            = symbol_address_code(block);
        symbol_table_put(&resolver.symbols, sym);
    }

    {
        Unary_Operator unary_operator_int;
        unary_operator_int.parameter = CompilerTypes[CODE_TYPE_INTEGER];
        unary_operator_int.output    = CompilerTypes[CODE_TYPE_INTEGER];
        resolver.unary_operators[UNARY_OPERATOR_PLUS].add(unary_operator_int);
        resolver.unary_operators[UNARY_OPERATOR_MINUS].add(unary_operator_int);
        resolver.unary_operators[UNARY_OPERATOR_BITWISE_NOT].add(unary_operator_int);
    }

    {
        Unary_Operator unary_operator_real;
        unary_operator_real.parameter = CompilerTypes[CODE_TYPE_REAL];
        unary_operator_real.output    = CompilerTypes[CODE_TYPE_REAL];
        resolver.unary_operators[UNARY_OPERATOR_PLUS].add(unary_operator_real);
        resolver.unary_operators[UNARY_OPERATOR_MINUS].add(unary_operator_real);
    }

    {
        Unary_Operator unary_operator_bool;
        unary_operator_bool.parameter = CompilerTypes[CODE_TYPE_BOOL];
        unary_operator_bool.output    = CompilerTypes[CODE_TYPE_BOOL];
        resolver.unary_operators[UNARY_OPERATOR_LOGICAL_NOT].add(unary_operator_bool);
    }

    {
        Binary_Operator binary_operator_int;
        binary_operator_int.parameters[0] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.parameters[1] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.output        = CompilerTypes[CODE_TYPE_INTEGER];
        resolver.binary_operators[BINARY_OPERATOR_ADDITION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_SUBTRACTION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_MULTIPLICATION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_DIVISION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_REMAINDER].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_BITWISE_SHIFT_RIGHT].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_BITWISE_SHIFT_LEFT].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_BITWISE_AND].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_BITWISE_XOR].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_BITWISE_OR].add(binary_operator_int);
    }

    {
        Binary_Operator binary_operator_int;
        binary_operator_int.parameters[0] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.parameters[1] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.output        = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.compound      = true;
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_ADDITION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_SUBTRACTION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_MULTIPLICATION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_DIVISION].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_REMAINDER].add(binary_operator_int);

        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_RIGHT].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_BITWISE_SHIFT_LEFT].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_BITWISE_AND].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_BITWISE_XOR].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_BITWISE_OR].add(binary_operator_int);
    }

    {
        Binary_Operator binary_operator_pointer;
        binary_operator_pointer.parameters[0] = CompilerTypes[CODE_TYPE_POINTER];
        binary_operator_pointer.parameters[1] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_pointer.output        = CompilerTypes[CODE_TYPE_POINTER];
        binary_operator_pointer.compound      = false;
        resolver.binary_operators[BINARY_OPERATOR_ADDITION].add(binary_operator_pointer);
        resolver.binary_operators[BINARY_OPERATOR_SUBTRACTION].add(binary_operator_pointer);

        binary_operator_pointer.compound = true;
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_ADDITION].add(binary_operator_pointer);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_SUBTRACTION].add(binary_operator_pointer);
    }

    {
        Binary_Operator binary_operator_int;
        binary_operator_int.parameters[0] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.parameters[1] = CompilerTypes[CODE_TYPE_INTEGER];
        binary_operator_int.output        = CompilerTypes[CODE_TYPE_BOOL];
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_GREATER].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_LESS].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_GREATER_EQUAL].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_LESS_EQUAL].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPARE_EQUAL].add(binary_operator_int);
        resolver.binary_operators[BINARY_OPERATOR_COMPARE_NOT_EQUAL].add(binary_operator_int);
    }

    {
        Binary_Operator binary_operator_real;
        binary_operator_real.parameters[0] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.parameters[1] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.output        = CompilerTypes[CODE_TYPE_REAL];
        resolver.binary_operators[BINARY_OPERATOR_ADDITION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_SUBTRACTION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_MULTIPLICATION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_DIVISION].add(binary_operator_real);
    }

    {
        Binary_Operator binary_operator_real;
        binary_operator_real.parameters[0] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.parameters[1] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.output        = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.compound      = true;
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_ADDITION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_SUBTRACTION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_MULTIPLICATION].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_COMPOUND_DIVISION].add(binary_operator_real);
    }

    {
        Binary_Operator binary_operator_real;
        binary_operator_real.parameters[0] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.parameters[1] = CompilerTypes[CODE_TYPE_REAL];
        binary_operator_real.output        = CompilerTypes[CODE_TYPE_BOOL];
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_GREATER].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_LESS].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_GREATER_EQUAL].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_RELATIONAL_LESS_EQUAL].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_COMPARE_EQUAL].add(binary_operator_real);
        resolver.binary_operators[BINARY_OPERATOR_COMPARE_NOT_EQUAL].add(binary_operator_real);
    }

    {
        auto type            = new Code_Type_Procedure;
        type->argument_count = 1;
        type->arguments      = new Code_Type *;
        type->arguments[0]   = symbol_table_get(&resolver.symbols, "int")->type;
        type->return_type    = symbol_table_get(&resolver.symbols, "*void")->type;

        auto sym             = resolver.symbols_allocator.add();
        sym->name            = "allocate";
        sym->type            = type;
        sym->address.kind    = Symbol_Address::CCALL;
        sym->address.memory  = nullptr;

        symbol_table_put(&resolver.symbols, sym);
    }

    {
        auto type            = new Code_Type_Procedure;
        type->argument_count = 1;
        type->arguments      = new Code_Type *;
        type->arguments[0]   = symbol_table_get(&resolver.symbols, "*void")->type;
        type->return_type    = nullptr;

        auto sym             = resolver.symbols_allocator.add();
        sym->name            = "free";
        sym->type            = type;
        sym->address.kind    = Symbol_Address::CCALL;
        sym->address.memory  = nullptr;

        symbol_table_put(&resolver.symbols, sym);
    }

    {
        auto type            = new Code_Type_Procedure;
        type->argument_count = 2;
        type->arguments      = new Code_Type *[type->argument_count];
        type->arguments[0]   = symbol_table_get(&resolver.symbols, "string")->type;
        type->arguments[1]   = symbol_table_get(&resolver.symbols, "*void")->type;
        type->return_type    = nullptr;
        type->is_variadic    = true;

        auto sym             = resolver.symbols_allocator.add();
        sym->name            = "print";
        sym->type            = type;
        sym->address.kind    = Symbol_Address::CCALL;
        sym->address.memory  = nullptr;

        symbol_table_put(&resolver.symbols, sym);
    }

    auto exprs = code_resolve_global_scope(&resolver, &resolver.symbols, node);

    {
        auto fp = fopen("code.txt", "wb");

        for (auto expr : exprs)
            print_code(expr, fp);

        fclose(fp);
    }

    Interp interp;
    interp_init(&interp, 1024 * 1024 * 4);

    for (auto expr : exprs)
        evaluate_code_node_assignment(expr, &interp, 0);

    auto main_proc = symbol_table_get(&resolver.symbols, "main", false);
    if (main_proc)
    {
        if (main_proc->flags & SYMBOL_BIT_CONSTANT && main_proc->address.kind == Symbol_Address::CODE)
        {
            auto type = main_proc->type;
            if (type->kind == CODE_TYPE_PROCEDURE)
            {
                auto proc_type = (Code_Type_Procedure *)type;
                if (proc_type->argument_count == 0 && !proc_type->return_type)
                {
                    auto proc = (Code_Node_Block *)main_proc->address.memory;

                    {
                        auto fp = fopen("code.txt", "ab");
                        print_code(proc, fp);
                        fclose(fp);
                    }

                    evaluate_node_block(proc, &interp, 0);
                }
                else
                {
                    fprintf(stderr, "The \"main\" procedure must not take any arguments and should return nothing!\n");
                }
            }
            else
            {
                fprintf(stderr, "The \"main\" symbol must be a procedure!\n");
            }
        }
        else
        {
            fprintf(stderr, "The \"main\" procedure must be constant!\n");
        }
    }
    else
    {
        fprintf(stderr, "\"main\" procedure not defined!\n");
    }

    return 0;
}
