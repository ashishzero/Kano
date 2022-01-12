#include "Interp.h"
#include "CodeNode.h"
#include <stdlib.h>

struct Evaluation_Value
{
	struct Kano_Array
	{
		Kano_Int length;
		uint8_t *data;
	};
	
	union {
		Kano_Int   int_value;
		Kano_Real  real_value;
		Kano_Bool  bool_value;
		uint8_t *  pointer_value;
		Kano_Array array_value;
		
	} imm;
	
	uint8_t *  address = nullptr;
	Code_Type *type    = nullptr;
};

template <typename T> T evaluation_value_get(Evaluation_Value &val)
{
	if (val.address)
		return *(T *)val.address;
	return *(T *)&val.imm;
}
#define EvaluationTypeValue(val, type) evaluation_value_get<type>(val)

template <typename T> T *evaluation_value_pointer(Evaluation_Value &val)
{
	if (val.address)
		return (T *)val.address;
	return (T *)&val.imm;
}
#define EvaluationTypePointer(val, type) evaluation_value_pointer<type>(val)

Evaluation_Value evaluate_expression(Code_Node *root, Interp *interp, uint64_t top);
Evaluation_Value evaluate_node_expression(Code_Node_Expression *root, Interp *interp, uint64_t top);
bool            evaluate_node_statement(Code_Node_Statement *root, Interp *interp, uint64_t top, Evaluation_Value *result = nullptr);
Evaluation_Value evaluate_code_node_assignment(Code_Node_Assignment *node, Interp *interp, uint64_t top);
void            evaluate_node_block(Code_Node_Block *root, Interp *stack, uint64_t top, bool isproc);

uint64_t push_into_interp_stack(Evaluation_Value var, Interp *interp, uint64_t top)
{
	memcpy(interp->stack + top, EvaluationTypePointer(var, void *), var.type->runtime_size);
	top += var.type->runtime_size;
	return top;
}

Evaluation_Value evaluate_procedure(Code_Node_Procedure_Call *root, Interp *interp, uint64_t prev_top)
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
		auto param = (Code_Node_Expression *)root->paraments[i];
		auto var   = evaluate_node_expression(param, interp, prev_top);
		top        = push_into_interp_stack(var, interp, top);
	}

	auto result = evaluate_expression((Code_Node *)root->procedure, interp, proc_top);

	return result;
}

void evaluate_do_block(Code_Node_Do *root, Interp *interp, uint64_t top)
{
	auto do_cond = root->condition;
	auto do_body = root->body;

	Evaluation_Value cond;

	do
	{
		evaluate_node_statement(do_body, interp, top);
		Assert(evaluate_node_statement(do_cond, interp, top, &cond));
	} while (EvaluationTypeValue(cond, bool));
}

void evaluate_while_block(Code_Node_While *root, Interp *interp, uint64_t top)
{
	auto while_cond = root->condition;
	auto while_body = root->body;
	
	Evaluation_Value cond;
	Assert(evaluate_node_statement(while_cond, interp, top, &cond));

	while (EvaluationTypeValue(cond, bool))
	{
		evaluate_node_statement(while_body, interp, top);
		Assert(evaluate_node_statement(while_cond, interp, top, &cond));
	}
}

void evaluate_if_block(Code_Node_If *root, Interp *interp, uint64_t top)
{
	auto cond = evaluate_node_expression((Code_Node_Expression *)root->condition, interp, top);
	if (EvaluationTypeValue(cond, bool))
		evaluate_node_statement((Code_Node_Statement *)root->true_statement, interp, top);
	else
	{
		if (root->false_statement)
			evaluate_node_statement((Code_Node_Statement *)root->false_statement, interp, top);
	}
}

void evaluate_for_block(Code_Node_For *root, Interp *interp, uint64_t top)
{
	auto for_init = root->initialization;
	auto for_cond = root->condition;
	auto for_incr = root->increment;
	auto for_body = root->body;

	Evaluation_Value cond;

	evaluate_node_statement(for_init, interp, top);
	Assert(evaluate_node_statement(for_cond, interp, top, &cond));

	while (EvaluationTypeValue(cond, bool))
	{
		evaluate_node_statement(for_body, interp, top);
		Assert(evaluate_node_statement(for_incr, interp, top));
		Assert(evaluate_node_statement(for_cond, interp, top, &cond));
	}
}

Evaluation_Value evaluate_code_node_assignment(Code_Node_Assignment *node, Interp *interp, uint64_t top)
{
	auto value = evaluate_node_expression((Code_Node_Expression *)node->value, interp, top);

	Assert(node->destination->child->kind == CODE_NODE_ADDRESS);

	auto dst = evaluate_node_expression(node->destination, interp, top);

	if (dst.address)
	{
		memcpy(dst.address, EvaluationTypePointer(value, void *), value.type->runtime_size);
	}
	else if (dst.type->kind == CODE_TYPE_POINTER)
	{
		memcpy(dst.imm.pointer_value, EvaluationTypePointer(value, void *), value.type->runtime_size);
	}
	else
	{
		Unreachable();
	}

	return value;
}

Evaluation_Value evaluate_node_literal(Code_Node_Literal *root, Interp *interp, uint64_t top)
{
	auto            node       = root;
	auto            check_kind = (Code_Node *)root;
	Evaluation_Value type_value;

	type_value.address = (uint8_t *)&node->data.boolean.value;
	type_value.type    = node->type;

	return type_value;
}

Evaluation_Value evaluate_unary_operator(Code_Node_Unary_Operator *root, Interp *interp, uint64_t top)
{
	switch (root->op_kind)
	{
	case UNARY_OPERATOR_LOGICAL_NOT: {
		auto type_value = evaluate_expression(root->child, interp, top);
		auto ref        = EvaluationTypePointer(type_value, bool);
		*ref            = !*ref;
		return type_value;
	}
	break;

	case UNARY_OPERATOR_BITWISE_NOT: {
		auto type_value = evaluate_expression(root->child, interp, top);
		auto ref        = EvaluationTypePointer(type_value, int64_t);
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

			Evaluation_Value r;
			r.imm.int_value = -EvaluationTypeValue(value, Kano_Int);
			r.type          = root->type;
			return r;
		}
		else if (value.type->kind == CODE_TYPE_REAL)
		{
			Evaluation_Value r;
			r.imm.real_value = -EvaluationTypeValue(value, Kano_Real);
			r.type           = root->type;
			return r;
		}

		Unreachable();
	}
	break;

	case UNARY_OPERATOR_DEREFERENCE: {
		Assert(root->child->kind == CODE_NODE_ADDRESS);

		auto            address = (Code_Node_Address *)root->child;
		Evaluation_Value type_value;

		Assert(address->child == nullptr);

		uint64_t offset = address->offset;
		auto     memory = interp->stack;

		if (address->address)
		{
			offset += address->address->offset;

			auto address_kind = address->address->kind;
			Assert(address_kind == Symbol_Address::GLOBAL || address_kind == Symbol_Address::STACK);

			if (address->address->kind == Symbol_Address::GLOBAL)
			{
				memory = interp->global;
			}
		}

		type_value.address = memory + top + offset;
		type_value.type    = root->type;

		return type_value;
	}
	break;

	case UNARY_OPERATOR_POINTER_TO: {
		Assert(root->child->kind == CODE_NODE_ADDRESS);

		auto            address = (Code_Node_Address *)root->child;

		Evaluation_Value type_value;
		type_value.type = root->type;

		Assert(address->child == nullptr);

		auto offset = address->offset;
		auto memory = interp->stack;

		if (address->address)
		{
			offset += address->address->offset;

			auto address_kind = address->address->kind;
			Assert(address_kind == Symbol_Address::GLOBAL || address_kind == Symbol_Address::STACK);

			if (address->address->kind == Symbol_Address::GLOBAL)
			{
				memory = interp->global;
			}
		}

		type_value.address = memory + top + offset;
		return type_value;
	}
	break;
	}

	Unreachable();
	return Evaluation_Value{};
}

typedef Evaluation_Value (*BinaryOperatorProc)(Evaluation_Value a, Evaluation_Value b, Code_Type *type);

typedef Kano_Int (*BinaryOperatorIntProc)(Kano_Int a, Kano_Int b);

Evaluation_Value binary_add(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.int_value = EvaluationTypeValue(a, Kano_Int) + EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		r.imm.real_value = EvaluationTypeValue(a, Kano_Real) + EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	case CODE_TYPE_POINTER: {
		Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.pointer_value = EvaluationTypeValue(a, uint8_t *) + EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_sub(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.int_value = EvaluationTypeValue(a, Kano_Int) - EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		r.imm.real_value = EvaluationTypeValue(a, Kano_Real) - EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	case CODE_TYPE_POINTER: {
		Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.pointer_value = EvaluationTypeValue(a, uint8_t *) - EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_mul(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.int_value = EvaluationTypeValue(a, Kano_Int) * EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		r.imm.real_value = EvaluationTypeValue(a, Kano_Real) * EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_div(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		r.imm.int_value = EvaluationTypeValue(a, Kano_Int) / EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		r.imm.real_value = EvaluationTypeValue(a, Kano_Real) / EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_mod(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) % EvaluationTypeValue(b, Kano_Int);
	return r;
}

Evaluation_Value binary_rs(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) >> EvaluationTypeValue(b, Kano_Int);
	return r;
}

Evaluation_Value binary_ls(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) << EvaluationTypeValue(b, Kano_Int);
	return r;
}

Evaluation_Value binary_and(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) & EvaluationTypeValue(b, Kano_Int);
	return r;
}
Evaluation_Value binary_xor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) ^ EvaluationTypeValue(b, Kano_Int);
	return r;
}

Evaluation_Value binary_or(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) | EvaluationTypeValue(b, Kano_Int);
	return r;
}

Evaluation_Value binary_gt(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) > EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) > EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_lt(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) < EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) < EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_ge(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) >= EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) >= EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_le(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) <= EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) <= EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_cmp(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) == EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) == EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;

	case CODE_TYPE_BOOL: {
		Assert(b.type->kind == CODE_TYPE_BOOL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Bool) == EvaluationTypeValue(b, Kano_Bool);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_ncmp(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;

	switch (a.type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(b.type->kind == CODE_TYPE_INTEGER);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Int) != EvaluationTypeValue(b, Kano_Int);
		return r;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(b.type->kind == CODE_TYPE_REAL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Real) != EvaluationTypeValue(b, Kano_Real);
		return r;
	}
	break;

	case CODE_TYPE_BOOL: {
		Assert(b.type->kind == CODE_TYPE_BOOL);
		r.imm.bool_value = EvaluationTypeValue(a, Kano_Bool) != EvaluationTypeValue(b, Kano_Bool);
		return r;
	}
	break;
	}

	Unreachable();
	return r;
}

Evaluation_Value binary_cadd(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, Kano_Int);
		*ref += EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		auto ref = EvaluationTypePointer(a, Kano_Real);
		*ref += EvaluationTypeValue(b, Kano_Real);
		return a;
	}
	break;

	case CODE_TYPE_POINTER: {
		Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, uint8_t *);
		*ref += EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;
	}

	Unreachable();
	return Evaluation_Value{};
}

Evaluation_Value binary_csub(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, Kano_Int);
		*ref -= EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		auto ref = EvaluationTypePointer(a, Kano_Real);
		*ref -= EvaluationTypeValue(b, Kano_Real);
		return a;
	}
	break;
	case CODE_TYPE_POINTER: {
		Assert(a.type->kind == CODE_TYPE_POINTER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, uint8_t *);
		*ref -= EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;
	}

	Unreachable();
	return Evaluation_Value{};
}

Evaluation_Value binary_cmul(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, Kano_Int);
		*ref *= EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		auto ref = EvaluationTypePointer(a, Kano_Real);
		*ref *= EvaluationTypeValue(b, Kano_Real);
		return a;
	}
	break;
	}

	Unreachable();
	return Evaluation_Value{};
}

Evaluation_Value binary_cdiv(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;

	switch (type->kind)
	{
	case CODE_TYPE_INTEGER: {
		Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
		auto ref = EvaluationTypePointer(a, Kano_Int);
		*ref /= EvaluationTypeValue(b, Kano_Int);
		return a;
	}
	break;

	case CODE_TYPE_REAL: {
		Assert(a.type->kind == CODE_TYPE_REAL && b.type->kind == CODE_TYPE_REAL);
		auto ref = EvaluationTypePointer(a, Kano_Real);
		*ref /= EvaluationTypeValue(b, Kano_Real);
		return a;
	}
	break;
	}

	Unreachable();
	return Evaluation_Value{};
}

Evaluation_Value binary_cmod(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref %= EvaluationTypeValue(b, Kano_Int);
	return a;
}

Evaluation_Value binary_crs(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref >>= EvaluationTypeValue(b, Kano_Int);
	return a;
}

Evaluation_Value binary_cls(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref <<= EvaluationTypeValue(b, Kano_Int);
	return a;
}

Evaluation_Value binary_cand(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref &= EvaluationTypeValue(b, Kano_Int);
	return a;
}

Evaluation_Value binary_cxor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref ^= EvaluationTypeValue(b, Kano_Int);
	return a;
}

Evaluation_Value binary_cor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref |= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static BinaryOperatorProc BinaryOperators[] = {
    binary_add,  binary_sub,  binary_mul,  binary_div, binary_mod, binary_rs,   binary_ls,   binary_add,  binary_xor,
    binary_or,   binary_gt,   binary_lt,   binary_ge,  binary_le,  binary_cmp,  binary_ncmp, binary_cadd, binary_csub,
    binary_cmul, binary_cdiv, binary_cmod, binary_crs, binary_cls, binary_cadd, binary_cxor, binary_cor};

Evaluation_Value evaluate_binary_operator(Code_Node_Binary_Operator *node, Interp *interp, uint64_t top)
{
	auto a = evaluate_expression(node->left, interp, top);
	auto b = evaluate_expression(node->right, interp, top);

	Assert(node->op_kind < ArrayCount(BinaryOperators));

	return BinaryOperators[node->op_kind](a, b, node->type);
}

Evaluation_Value evaluate_expression(Code_Node *root, Interp *interp, uint64_t top)
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
			Evaluation_Value type_value;
			type_value.type = root->type;

			auto offset     = node->offset;
			auto memory     = interp->stack;

			Assert(node->child == nullptr);

			if (node->address)
			{
				offset += node->address->offset;

				auto address_kind = node->address->kind;
				Assert(address_kind == Symbol_Address::GLOBAL || address_kind == Symbol_Address::STACK);

				if (node->address->kind == Symbol_Address::GLOBAL)
				{
					memory = interp->global;
				}
			}

			type_value.address = memory + top + offset;

			return type_value;
		}
		else
		{
			switch (node->address->kind)
			{
			case Symbol_Address::CODE: {

				evaluate_node_block(node->address->code, interp, top, true);

				Evaluation_Value type_value;

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
			case Symbol_Address::CCALL: {
				node->address->ccall(interp, top);

				Evaluation_Value type_value;

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
		Evaluation_Value type_value;
		type_value.type = cast->type;

		switch (cast->type->kind)
		{
		case CODE_TYPE_REAL: {
			Assert(value.type->kind == CODE_TYPE_INTEGER);
			type_value.imm.real_value = (Kano_Real)EvaluationTypeValue(value, Kano_Int);
		}
		break;

		case CODE_TYPE_INTEGER: {
			if (value.type->kind == CODE_TYPE_BOOL)
			{
				type_value.imm.int_value = EvaluationTypeValue(value, Kano_Bool);
			}
			else if (value.type->kind == CODE_TYPE_REAL)
			{
				type_value.imm.int_value = (Kano_Int)EvaluationTypeValue(value, Kano_Real);
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
				type_value.imm.bool_value = EvaluationTypeValue(value, Kano_Int) != 0;
			}
			else if (value.type->kind == CODE_TYPE_REAL)
			{
				type_value.imm.bool_value = EvaluationTypeValue(value, Kano_Real) != 0.0;
			}
			else
			{
				Unreachable();
			}
		}
		break;

		case CODE_TYPE_POINTER: {
			Assert(value.type->kind == CODE_TYPE_POINTER);
			type_value.imm.pointer_value = EvaluationTypeValue(value, uint8_t *);
		}
		break;

		case CODE_TYPE_ARRAY_VIEW: {
			Assert(value.type->kind == CODE_TYPE_STATIC_ARRAY);
			type_value.imm.array_value.data   = EvaluationTypeValue(value, uint8_t *);
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
		return result;
	}
	break;
	case CODE_NODE_RETURN: {
		auto node   = (Code_Node_Return *)root;
		auto result = evaluate_expression(node->expression, interp, top);
		push_into_interp_stack(result, interp, top);

		interp->return_count += 1;
		return result;
	}
	break;

		NoDefaultCase();
	}

	Unreachable();
	return Evaluation_Value{};
}

Evaluation_Value evaluate_node_expression(Code_Node_Expression *root, Interp *interp, uint64_t top)
{
	return evaluate_expression(root->child, interp, top);
}

bool evaluate_node_statement(Code_Node_Statement *root, Interp *interp, uint64_t top, Evaluation_Value *value)
{
	//interp->intercept(interp, top, root);
	//printf("Executing line: %zu\n", root->source_row);
	Assert(root->symbol_table);

	switch (root->node->kind)
	{
	case CODE_NODE_EXPRESSION: {
		auto result = evaluate_node_expression((Code_Node_Expression *)root->node, interp, top);
		if (value) *value = result;
		return true;
	}
	break;

	case CODE_NODE_ASSIGNMENT: {
		auto result = evaluate_code_node_assignment((Code_Node_Assignment *)root->node, interp, top);
		if (value) *value = result;
		return true;
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

	return false;
}

void evaluate_node_block(Code_Node_Block *root, Interp *interp, uint64_t top, bool isproc)
{
	auto return_index = interp->return_count;
	for (auto statement = root->statement_head; statement; statement = statement->next)
	{
		evaluate_node_statement(statement, interp, top);
		if (return_index != interp->return_count)
		{
			if (isproc)
				interp->return_count -= 1;
			break;
		}
	}
}

//
//
//

void interp_init(Interp *interp, size_t stack_size, size_t bss_size)
{
	interp->stack  = new uint8_t[stack_size];
	interp->global = new uint8_t[bss_size];
}

void interp_eval_globals(Interp *interp, Array_View<Code_Node_Assignment *> exprs)
{
	for (auto expr : exprs)
		evaluate_code_node_assignment(expr, interp, 0);
}

void interp_eval_procedure(Interp *interp, Code_Node_Block *proc)
{
	evaluate_node_block(proc, interp, 0, true);
}
