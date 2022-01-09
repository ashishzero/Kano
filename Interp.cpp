#include "Interp.h"
#include "CodeNode.h"
#include <stdlib.h>

template <typename T> T to_type(Find_Type_Value &val)
{
    if (val.address)
        return *(T *)val.address;
    return *(T *)&val.imm;
}
#define TypeValue(val, type) to_type<type>(val)

template <typename T> T *to_type_ptr(Find_Type_Value &val)
{
    if (val.address)
        return (T *)val.address;
    return (T *)&val.imm;
}
#define TypeValueRef(val, type) to_type_ptr<type>(val)

Find_Type_Value evaluate_expression(Code_Node *root, Interp *interp, uint64_t top);
Find_Type_Value evaluate_node_expression(Code_Node_Expression *root, Interp *interp, uint64_t top);
void            evaluate_node_statement(Code_Node_Statement *root, Interp *interp, uint64_t top);

void            interp_init(Interp *interp, size_t size)
{
    interp->stack = new uint8_t[size];
}

void print_values(Find_Type_Value result)
{
    if (result.type == NULL)
        return;
    switch (result.type->kind)
    {
    case CODE_TYPE_BOOL: {
        printf("statement executes:: %d\n", (int)TypeValue(result, Kano_Bool));
    }
    break;
    case CODE_TYPE_REAL: {
        printf("statement executes:: %f\n", TypeValue(result, Kano_Real));
    }
    break;
    case CODE_TYPE_INTEGER: {
        printf("statement executes:: %d\n", TypeValue(result, Kano_Int));
    }
    break;
    case CODE_TYPE_POINTER: {
        printf("statement executes:: %p\n", TypeValue(result, void *));
    }
    break;
    }
}

uint64_t push_into_interp_stack(Find_Type_Value var, Interp *interp, uint64_t top)
{
    memcpy(interp->stack + top, TypeValueRef(var, void *), var.type->runtime_size);
    top += var.type->runtime_size;
    return top;
}

Find_Type_Value evaluate_procedure(Code_Node_Procedure_Call *root, Interp *interp, uint64_t prev_top)
{
    auto top = root->stack_top + prev_top;
    for (int i = 0; i < root->variadic_count; i++)
    {
        auto var = evaluate_node_expression((Code_Node_Expression *)root->variadics[i], interp, prev_top);
        top      = push_into_interp_stack(var, interp, top);
    }
    // allocate for return type
    auto proc_top = top;
    if (root->type)
        top += root->type->runtime_size;
    for (int i = 0; i < root->parameter_count; i++)
    {
        auto var  = evaluate_node_expression((Code_Node_Expression *)root->paraments[i], interp, prev_top);
        auto node = (Code_Type_Procedure *)root->paraments[i];
        top       = push_into_interp_stack(var, interp, top);
    }
    //interp->call_stack_count += 1;
    //interp->call_stack_index += 1;
    auto result = evaluate_expression((Code_Node *)root->procedure, interp, proc_top);
    //interp->call_stack_count -= 1;
    return result;
}
void evaluate_do_block(Code_Node_Do *root, Interp *interp, uint64_t top)
{
    Find_Type_Value cond;
    do
    {
        evaluate_node_statement((Code_Node_Statement *)root->body, interp, top);
        cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
    } while (TypeValue(cond, bool));
}

void evaluate_while_block(Code_Node_While *root, Interp *interp, uint64_t top)
{
    auto cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
    while (TypeValue(cond, bool))
    {
        evaluate_node_statement((Code_Node_Statement *)root->body, interp, top);
        cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
        printf("statement executes:: %d\n", (int)TypeValue(cond, bool));
    }
}

void evaluate_if_block(Code_Node_If *root, Interp *interp, uint64_t top)
{
    auto cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
    if (TypeValue(cond, bool))
        evaluate_node_statement((Code_Node_Statement *)root->true_statement, interp, top);
    else
    {
        if (root->false_statement)
            evaluate_node_statement((Code_Node_Statement *)root->false_statement, interp, top);
    }
}

void evaluate_for_block(Code_Node_For *root, Interp *interp, uint64_t top)
{
    evaluate_node_statement((Code_Node_Statement *)root->initialization, interp, top);
    auto cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
    printf("statement executes:: %d\n", (int)TypeValue(cond, bool));
    while (TypeValue(cond, bool))
    {
        evaluate_node_statement((Code_Node_Statement *)root->body, interp, top);
        auto incre = evaluate_node_expression((Code_Node_Expression *)root->increment, interp, top);
        print_values(incre);
        cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
    }
}

Find_Type_Value evaluate_code_node_assignment(Code_Node_Assignment *node, Interp *interp, uint64_t top)
{
    auto value = evaluate_node_expression((Code_Node_Expression *)node->value, interp, top);
    Assert(node->destination->child->kind == CODE_NODE_ADDRESS);

    auto destiny = (Code_Node_Address *)node->destination->child;

    auto offset  = destiny->offset;
    if (destiny->address)
    {
        Assert(destiny->address->kind == Symbol_Address::STACK);
        offset += (uint64_t)destiny->address->offset;
    }

    memcpy(interp->stack + top + offset, TypeValueRef(value, void *), value.type->runtime_size);

    return value;
}

Find_Type_Value evaluate_node_literal(Code_Node_Literal *root, Interp *interp, uint64_t top)
{
    auto            node       = root;
    auto            check_kind = (Code_Node *)root;
    Find_Type_Value type_value;

    type_value.address = (uint8_t *)&node->data.boolean.value;
    type_value.type    = node->type;

    return type_value;
}

Find_Type_Value evaluate_unary_operator(Code_Node_Unary_Operator *root, Interp *interp, uint64_t top)
{
    switch (root->op_kind)
    {
    case UNARY_OPERATOR_LOGICAL_NOT: {
        auto type_value = evaluate_expression(root->child, interp, top);
        auto ref        = TypeValueRef(type_value, bool);
        *ref            = !*ref;
        return type_value;
    }
    break;

    case UNARY_OPERATOR_BITWISE_NOT: {
        auto type_value = evaluate_expression(root->child, interp, top);
        auto ref        = TypeValueRef(type_value, int64_t);
        *ref            = ~*ref;
        return type_value;
    }
    break;

    case UNARY_OPERATOR_PLUS: {
        auto type_value = evaluate_expression(root->child, interp, top);
        return type_value;
    }
    break;

    case UNARY_OPERATOR_MINUS: {
        auto value = evaluate_expression(root->child, interp, top);

        if (value.type->kind == CODE_TYPE_INTEGER)
        {

            Find_Type_Value r;
            r.imm.int_value = -TypeValue(value, Kano_Int);
            r.type          = root->type;
            return r;
        }
        else if (value.type->kind == CODE_TYPE_REAL)
        {
            Find_Type_Value r;
            r.imm.real_value = -TypeValue(value, Kano_Real);
            r.type           = root->type;
            return r;
        }

        Unreachable();
    }
    break;

    case UNARY_OPERATOR_DEREFERENCE: {
        Assert(root->child->kind == CODE_NODE_ADDRESS);

        auto     address = (Code_Node_Address *)root->child;

        uint64_t offset  = address->offset;
        if (address->address)
        {
            Assert(address->address->kind == Symbol_Address::STACK);
            offset += address->address->offset;
        }

        auto            address_value = (uint8_t *)(interp->stack + top + offset);

        Find_Type_Value type_value;
        type_value.address = address_value;
        type_value.type    = root->type;
        return type_value;
    }
    break;

    case UNARY_OPERATOR_POINTER_TO: {
        Assert(root->child->kind == CODE_NODE_ADDRESS);

        auto            address = (Code_Node_Address *)root->child;

        Find_Type_Value type_value;
        type_value.type = root->type;

        auto offset     = address->offset;
        if (address->address)
        {
            offset += address->address->offset;
        }

        type_value.address = (uint8_t *)(interp->stack + offset + top);
        return type_value;
    }
    break;
    }

    Unreachable();
    return Find_Type_Value{};
}

typedef Find_Type_Value (*BinaryOperatorProc)(Find_Type_Value a, Find_Type_Value b, Code_Type *type);

typedef Kano_Int (*BinaryOperatorIntProc)(Kano_Int a, Kano_Int b);

Find_Type_Value binary_add(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.int_value = TypeValue(a, Kano_Int) + TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        r.imm.real_value = TypeValue(a, Kano_Real) + TypeValue(b, Kano_Real);
        return r;
    }
    break;
    case CODE_TYPE_POINTER: {
        Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.pointer_value = TypeValue(a, uint8_t *) + TypeValue(b, Kano_Int);
        return r;
    }
    break;
    }


    Unreachable();
    return r;
}

Find_Type_Value binary_sub(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.int_value = TypeValue(a, Kano_Int) - TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        r.imm.real_value = TypeValue(a, Kano_Real) - TypeValue(b, Kano_Real);
        return r;
    }
    break;
    case CODE_TYPE_POINTER: {
        Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.pointer_value = TypeValue(a, uint8_t *) - TypeValue(b, Kano_Int);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_mul(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.int_value = TypeValue(a, Kano_Int) * TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        r.imm.real_value = TypeValue(a, Kano_Real) * TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_div(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        r.imm.int_value = TypeValue(a, Kano_Int) / TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        r.imm.real_value = TypeValue(a, Kano_Real) / TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_mod(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) % TypeValue(b, Kano_Int);
    return r;
}

Find_Type_Value binary_rs(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) >> TypeValue(b, Kano_Int);
    return r;
}

Find_Type_Value binary_ls(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) << TypeValue(b, Kano_Int);
    return r;
}

Find_Type_Value binary_and(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) & TypeValue(b, Kano_Int);
    return r;
}
Find_Type_Value binary_xor(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) ^ TypeValue(b, Kano_Int);
    return r;
}

Find_Type_Value binary_or(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    r.imm.int_value = TypeValue(a, Kano_Int) | TypeValue(b, Kano_Int);
    return r;
}

Find_Type_Value binary_gt(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) > TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) > TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_lt(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) < TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) < TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_ge(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) >= TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) >= TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_le(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) <= TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) <= TypeValue(b, Kano_Real);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_cmp(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) == TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) == TypeValue(b, Kano_Real);
        return r;
    }
    break;

    case CODE_TYPE_BOOL: {
        Assert(b.type->kind == CODE_TYPE_BOOL);
        r.imm.bool_value = TypeValue(a, Kano_Bool) == TypeValue(b, Kano_Bool);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_ncmp(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    Find_Type_Value r;
    r.type = type;

    switch (a.type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(b.type->kind == CODE_TYPE_INTEGER);
        r.imm.bool_value = TypeValue(a, Kano_Int) != TypeValue(b, Kano_Int);
        return r;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(b.type->kind == CODE_TYPE_REAL);
        r.imm.bool_value = TypeValue(a, Kano_Real) != TypeValue(b, Kano_Real);
        return r;
    }
    break;

    case CODE_TYPE_BOOL: {
        Assert(b.type->kind == CODE_TYPE_BOOL);
        r.imm.bool_value = TypeValue(a, Kano_Bool) != TypeValue(b, Kano_Bool);
        return r;
    }
    break;
    }

    Unreachable();
    return r;
}

Find_Type_Value binary_cadd(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, Kano_Int);
        *ref += TypeValue(b, Kano_Int);
        return a;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        auto ref = TypeValueRef(a, Kano_Real);
        *ref += TypeValue(b, Kano_Real);
        return a;
    }
    break;

    case CODE_TYPE_POINTER: {
        Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, uint8_t *);
        *ref += TypeValue(b, Kano_Int);
        return a;
    }
    break;
    }

    Unreachable();
    return Find_Type_Value{};
}

Find_Type_Value binary_csub(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, Kano_Int);
        *ref -= TypeValue(b, Kano_Int);
        return a;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        auto ref = TypeValueRef(a, Kano_Real);
        *ref -= TypeValue(b, Kano_Real);
        return a;
    }
    break;
    case CODE_TYPE_POINTER: {
        Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, uint8_t *);
        *ref -= TypeValue(b, Kano_Int);
        return a;
    }
    break;
    }

    Unreachable();
    return Find_Type_Value{};
}

Find_Type_Value binary_cmul(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, Kano_Int);
        *ref *= TypeValue(b, Kano_Int);
        return a;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        auto ref = TypeValueRef(a, Kano_Real);
        *ref *= TypeValue(b, Kano_Real);
        return a;
    }
    break;
    }

    Unreachable();
    return Find_Type_Value{};
}

Find_Type_Value binary_cdiv(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;

    switch (type->kind)
    {
    case CODE_TYPE_INTEGER: {
        Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
        auto ref = TypeValueRef(a, Kano_Int);
        *ref /= TypeValue(b, Kano_Int);
        return a;
    }
    break;

    case CODE_TYPE_REAL: {
        Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
        auto ref = TypeValueRef(a, Kano_Real);
        *ref /= TypeValue(b, Kano_Real);
        return a;
    }
    break;
    }

    Unreachable();
    return Find_Type_Value{};
}

Find_Type_Value binary_cmod(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref %= TypeValue(b, Kano_Int);
    return a;
}

Find_Type_Value binary_crs(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref >>= TypeValue(b, Kano_Int);
    return a;
}

Find_Type_Value binary_cls(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref <<= TypeValue(b, Kano_Int);
    return a;
}

Find_Type_Value binary_cand(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref &= TypeValue(b, Kano_Int);
    return a;
}

Find_Type_Value binary_cxor(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref ^= TypeValue(b, Kano_Int);
    return a;
}

Find_Type_Value binary_cor(Find_Type_Value a, Find_Type_Value b, Code_Type *type)
{
    a.type = type;
    Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
    auto ref = TypeValueRef(a, Kano_Int);
    *ref |= TypeValue(b, Kano_Int);
    return a;
}

static BinaryOperatorProc BinaryOperators[] = {
    binary_add,  binary_sub,  binary_mul,  binary_div, binary_mod, binary_rs,   binary_ls,   binary_add,  binary_xor,
    binary_or,   binary_gt,   binary_lt,   binary_ge,  binary_le,  binary_cmp,  binary_ncmp, binary_cadd, binary_csub,
    binary_cmul, binary_cdiv, binary_cmod, binary_crs, binary_cls, binary_cadd, binary_cxor, binary_cor};

Find_Type_Value evaluate_binary_operator(Code_Node_Binary_Operator *node, Interp *interp, uint64_t top)
{
    auto a = evaluate_expression(node->left, interp, top);
    auto b = evaluate_expression(node->right, interp, top);

    Assert(node->op_kind < ArrayCount(BinaryOperators));

    return BinaryOperators[node->op_kind](a, b, node->type);
}

Find_Type_Value evaluate_expression(Code_Node *root, Interp *interp, uint64_t top)
{
    switch (root->kind)
    {
    case CODE_NODE_LITERAL: {
        return evaluate_node_literal((Code_Node_Literal *)root, interp, top);
    }
    break;

    case CODE_NODE_UNARY_OPERATOR: {
        return evaluate_unary_operator((Code_Node_Unary_Operator *)root, interp, top);
    }
    break;

    case CODE_NODE_BINARY_OPERATOR: {
        return evaluate_binary_operator((Code_Node_Binary_Operator *)root, interp, top);
    }
    break;

    case CODE_NODE_ADDRESS: {
        auto node = (Code_Node_Address *)root;

        if (node->type->kind != CODE_TYPE_PROCEDURE)
        {
            auto offset = node->offset;

            if (node->address)
            {
                // TODO: SUPPORT!!
                Assert(!node->child && node->address->kind == Symbol_Address::STACK);
                offset += node->address->offset;
            }

            Find_Type_Value type_value;
            type_value.type    = root->type;
            type_value.address = (uint8_t *)(interp->stack + top + offset);
            return type_value;
        }
        else
        {
            //Assert(!node->child && node->address->kind == Symbol_Address::CODE);

            switch (node->address->kind)
            {
            case Symbol_Address::CODE: {

                evaluate_node_block(node->address->code, interp, top, true);

                Find_Type_Value type_value;

                auto            proc = (Code_Type_Procedure *)node->type;

                if (proc->return_type)
                {
                    type_value.address = (uint8_t *)(interp->stack + top);
                    type_value.type    = proc->return_type;
                }
                else
                {
                    type_value.address = nullptr;
                    type_value.type    = nullptr;
                }
                return type_value;
            }
            break;
            case Symbol_Address::CCALL:{
                node->address->ccall(interp, top);     ////passing to main
              //  interp->call_stack_index -= 1;

                Find_Type_Value type_value;

                auto            proc = (Code_Type_Procedure *)node->type;

                if (proc->return_type)
                {
                    type_value.address = (uint8_t *)(interp->stack + top);
                    type_value.type    = proc->return_type;
                }
                else
                {
                    type_value.address = nullptr;
                    type_value.type    = nullptr;
                }
                return type_value;

            }break;
            }

        }
    }
    break;

    case CODE_NODE_ASSIGNMENT: {
        return evaluate_code_node_assignment((Code_Node_Assignment *)root, interp, top);
    }
    break;

    case CODE_NODE_TYPE_CAST: {
        auto            cast  = (Code_Node_Type_Cast *)root;
        auto            value = evaluate_node_expression(cast->child, interp, top);
        Find_Type_Value type_value;
        type_value.type = cast->type;

        switch (cast->type->kind)
        {
        case CODE_TYPE_REAL: {
            Assert(value.type->kind == CODE_TYPE_INTEGER);
            type_value.imm.real_value = (Kano_Real)TypeValue(value, Kano_Int);
        }
        break;

        case CODE_TYPE_INTEGER: {
            if (value.type->kind == CODE_TYPE_BOOL)
            {
                type_value.imm.int_value = TypeValue(value, Kano_Bool);
            }
            else if (value.type->kind == CODE_TYPE_REAL)
            {
                type_value.imm.int_value = (Kano_Int)TypeValue(value, Kano_Real);
            }
            else
            {
                Unreachable();
            }
        }
        break;

        case CODE_TYPE_BOOL: {
            if (value.type->kind == CODE_TYPE_INTEGER)
            {
                type_value.imm.bool_value = TypeValue(value, Kano_Int) != 0;
            }
            else if (value.type->kind == CODE_TYPE_REAL)
            {
                type_value.imm.bool_value = TypeValue(value, Kano_Real) != 0.0;
            }
            else
            {
                Unreachable();
            }
        }
        break;

        case CODE_TYPE_POINTER: {
            Assert(value.type->kind == CODE_TYPE_POINTER);
            type_value.imm.pointer_value = TypeValue(value, uint8_t *);
        }
        break;

        case CODE_TYPE_ARRAY_VIEW: {
            Assert(value.type->kind == CODE_TYPE_STATIC_ARRAY);
            type_value.imm.array_value.data   = TypeValue(value, uint8_t *);
            auto type                         = (Code_Type_Static_Array *)value.type;
            type_value.imm.array_value.length = type->element_count;
        }
        break;

            NoDefaultCase();
        }

        return type_value;
    }
    break;
    case CODE_NODE_IF: {
        return evaluate_expression((Code_Node *)root, interp, top);
    }
    break;
    case CODE_NODE_PROCEDURE_CALL: {
        return evaluate_procedure((Code_Node_Procedure_Call *)root, interp, top);
    }
    break;
    case CODE_NODE_EXPRESSION: {
        auto result = evaluate_node_expression((Code_Node_Expression *)root, interp, top);
        print_values(result);
        return result;
    }
    break;
    case CODE_NODE_RETURN: {
        auto node   = (Code_Node_Return *)root;
        auto result = evaluate_expression(node->expression, interp, top);
        push_into_interp_stack(result, interp, top);
 
        interp->return_count += 1;
       // interp->call_stack_index -= 1;
        return result;
    }
    break;

        NoDefaultCase();
    }

    Unreachable();
    return Find_Type_Value{};
}

Find_Type_Value evaluate_node_expression(Code_Node_Expression *root, Interp *interp, uint64_t top)
{
    return evaluate_expression(root->child, interp, top);
}

void evaluate_node_statement(Code_Node_Statement *root, Interp *interp, uint64_t top)
{
    switch (root->node->kind)
    {
    case CODE_NODE_EXPRESSION: {
        auto result = evaluate_node_expression((Code_Node_Expression *)root->node, interp, top);
        print_values(result);
    }
    break;
    case CODE_NODE_ASSIGNMENT: {
        auto result = evaluate_code_node_assignment((Code_Node_Assignment *)root->node, interp, top);
        print_values(result);
        // Unimplemented();
    }
    break;
    case CODE_NODE_BLOCK: {
        evaluate_node_block((Code_Node_Block *)root->node, interp, top, false);
    }
    break;
    case CODE_NODE_IF: {
        evaluate_if_block((Code_Node_If *)root->node, interp, top);
    }
    break;
    case CODE_NODE_FOR: {
        evaluate_for_block((Code_Node_For *)root->node, interp, top);
    }
    break;
    case CODE_NODE_WHILE: {
        evaluate_while_block((Code_Node_While *)root->node, interp, top);
    }
    break;
    case CODE_NODE_DO: {
        evaluate_do_block((Code_Node_Do *)root->node, interp, top);
    }
    break;
        NoDefaultCase();
    }
}

void evaluate_node_block(Code_Node_Block *root, Interp *interp, uint64_t top, bool isproc)
{
    auto return_index = interp->return_count;
    for (auto statement = root->statement_head; statement; statement = statement->next)
    {
        evaluate_node_statement(statement, interp, top);
        printf("STATEMENT  ::  %zu \n", statement->source_row);
        if (return_index != interp->return_count)
        {
            if (isproc)
                interp->return_count -= 1;
            break;
        }
    }
   // interp->call_stack_index -= 1;
}