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

static inline uint64_t interp_push_into_stack(Interp *interp, Evaluation_Value var, uint64_t offset)
{
	memcpy(interp->stack + interp->stack_top + offset, EvaluationTypePointer(var, void *), var.type->runtime_size);
	offset += var.type->runtime_size;
	return offset;
}

//
//
//

static inline uint8_t *interp_eval_data_address(Interp *interp, Code_Node_Address *node)
{
	Assert(node->type->kind != CODE_TYPE_PROCEDURE);
	
	auto offset = node->offset;
	auto memory = interp->stack;
	
	Assert(node->child == nullptr);
	
	if (node->address)
	{
		offset += node->address->offset;
		auto address_kind = node->address->kind;

		if (address_kind == Symbol_Address::STACK)
		{
			offset += interp->stack_top;
		}
		else if (address_kind == Symbol_Address::GLOBAL)
		{
			memory = interp->global;
		}
		else
		{
			Unreachable();
		}
	}

	return memory + offset;
}

static void interp_eval_block(Code_Node_Block *root, Interp *interp, bool isproc);

static Evaluation_Value interp_eval_address(Code_Node_Address *node, Interp *interp)
{
	if (node->type->kind != CODE_TYPE_PROCEDURE)
	{
		Evaluation_Value type_value;
		type_value.type = node->type;
		type_value.address = interp_eval_data_address(interp, node);
		return type_value;
	}

	else
	{
		switch (node->address->kind)
		{
			case Symbol_Address::CODE: 
			{
				interp_eval_block(node->address->code, interp, true);
				
				auto proc = (Code_Type_Procedure *)node->type;
				
				Evaluation_Value type_value;

				if (proc->return_type)
				{
					type_value.address = (uint8_t *)(interp->stack + interp->stack_top);
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

			case Symbol_Address::CCALL:
			{
				node->address->ccall(interp);
				
				auto proc = (Code_Type_Procedure *)node->type;
				
				Evaluation_Value type_value;
				
				if (proc->return_type)
				{
					type_value.address = (uint8_t *)(interp->stack + interp->stack_top);
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

			NoDefaultCase();
		}

		Unreachable();
		return Evaluation_Value{};
	}
}

static Evaluation_Value interp_eval_root_expression(Code_Node_Expression *root, Interp *interp);

static Evaluation_Value interp_eval_type_cast(Code_Node_Type_Cast *cast, Interp *interp)
{
	auto value = interp_eval_root_expression(cast->child, interp);

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

static Evaluation_Value interp_eval_expression(Code_Node *root, Interp *interp);

static Evaluation_Value interp_eval_return(Code_Node_Return *node, Interp *interp)
{
	auto result = interp_eval_expression(node->expression, interp);
	interp_push_into_stack(interp, result, 0);
	interp->return_count += 1;
	return result;
}

static Evaluation_Value interp_eval_literal(Code_Node_Literal *node, Interp *interp)
{
	Evaluation_Value type_value;
	type_value.address = (uint8_t *)&node->data;
	type_value.type    = node->type;
	return type_value;
}

static Evaluation_Value interp_eval_expression(Code_Node *root, Interp *interp);

static Evaluation_Value interp_eval_unary_operator(Code_Node_Unary_Operator *root, Interp *interp)
{
	switch (root->op_kind)
	{
		case UNARY_OPERATOR_LOGICAL_NOT: {
			auto type_value = interp_eval_expression(root->child, interp);
			auto ref        = EvaluationTypePointer(type_value, bool);
			*ref            = !*ref;
			return type_value;
		}
		break;
		
		case UNARY_OPERATOR_BITWISE_NOT: {
			auto type_value = interp_eval_expression(root->child, interp);
			auto ref        = EvaluationTypePointer(type_value, int64_t);
			*ref            = ~*ref;
			return type_value;
		}
		break;
		
		case UNARY_OPERATOR_PLUS: {
			auto type_value = interp_eval_expression(root->child, interp);
			return type_value;
		}
		break;
		
		case UNARY_OPERATOR_MINUS: {
			auto value = interp_eval_expression(root->child, interp);
			
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
		
		case UNARY_OPERATOR_DEREFERENCE: 
		case UNARY_OPERATOR_POINTER_TO: {
			Assert(root->child->kind == CODE_NODE_ADDRESS);
			
			auto address = (Code_Node_Address *)root->child;

			Evaluation_Value type_value;
			type_value.type    = root->type;
			type_value.address = interp_eval_data_address(interp, address);
			
			return type_value;
		}
		break;

		NoDefaultCase();
	}
	
	Unreachable();
	return Evaluation_Value{};
}

typedef Evaluation_Value (*BinaryOperatorProc)(Evaluation_Value a, Evaluation_Value b, Code_Type *type);

typedef Kano_Int (*BinaryOperatorIntProc)(Kano_Int a, Kano_Int b);

static Evaluation_Value binary_add(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_sub(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_mul(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_div(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_mod(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) % EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_rs(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) >> EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_ls(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) << EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_and(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) & EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_xor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) ^ EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_or(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	Evaluation_Value r;
	r.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	r.imm.int_value = EvaluationTypeValue(a, Kano_Int) | EvaluationTypeValue(b, Kano_Int);
	return r;
}

static Evaluation_Value binary_gt(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_lt(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_ge(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_le(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_cmp(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_ncmp(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_cadd(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_csub(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_cmul(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_cdiv(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value binary_cmod(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref %= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static Evaluation_Value binary_crs(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref >>= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static Evaluation_Value binary_cls(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref <<= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static Evaluation_Value binary_cand(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref &= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static Evaluation_Value binary_cxor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
{
	a.type = type;
	Assert(a.type->kind == CODE_TYPE_INTEGER && b.type->kind == CODE_TYPE_INTEGER);
	auto ref = EvaluationTypePointer(a, Kano_Int);
	*ref ^= EvaluationTypeValue(b, Kano_Int);
	return a;
}

static Evaluation_Value binary_cor(Evaluation_Value a, Evaluation_Value b, Code_Type *type)
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

static Evaluation_Value interp_eval_expression(Code_Node *root, Interp *interp);
static Evaluation_Value interp_eval_root_expression(Code_Node_Expression *root, Interp *interp);

static Evaluation_Value evaluate_binary_operator(Code_Node_Binary_Operator *node, Interp *interp)
{
	auto a = interp_eval_expression(node->left, interp);
	auto b = interp_eval_expression(node->right, interp);
	
	Assert(node->op_kind < ArrayCount(BinaryOperators));
	
	return BinaryOperators[node->op_kind](a, b, node->type);
}

static Evaluation_Value interp_eval_assignment(Code_Node_Assignment *node, Interp *interp)
{
	auto value = interp_eval_root_expression((Code_Node_Expression *)node->value, interp);
	
	Assert(node->destination->child->kind == CODE_NODE_ADDRESS);
	
	auto dst = interp_eval_root_expression(node->destination, interp);
	
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

static Evaluation_Value interp_eval_procedure_call(Code_Node_Procedure_Call *root, Interp *interp)
{
	auto prev_top = interp->stack_top;
	auto new_top = root->stack_top + prev_top;

	uint64_t offset = 0;
	for (int i = 0; i < root->variadic_count; i++)
	{
		interp->stack_top = prev_top;
		auto var = interp_eval_root_expression((Code_Node_Expression *)root->variadics[i], interp);

		interp->stack_top = new_top;
		offset   = interp_push_into_stack(interp, var, offset);
	}
	
	new_top += offset;
	offset = 0;

	if (root->type)
		offset += root->type->runtime_size;
	
	for (int i = 0; i < root->parameter_count; i++)
	{
		interp->stack_top = prev_top;
		auto var   = interp_eval_root_expression((Code_Node_Expression *)root->paraments[i], interp);

		interp->stack_top = new_top;
		offset     = interp_push_into_stack(interp, var, offset);
	}
	
	auto result = interp_eval_root_expression(root->procedure, interp);

	interp->stack_top = prev_top;
	
	return result;
}

static Evaluation_Value interp_eval_expression(Code_Node *root, Interp *interp)
{
	switch (root->kind)
	{
		case CODE_NODE_LITERAL: return interp_eval_literal((Code_Node_Literal *)root, interp);
		case CODE_NODE_UNARY_OPERATOR: return interp_eval_unary_operator((Code_Node_Unary_Operator *)root, interp);
		case CODE_NODE_BINARY_OPERATOR: return evaluate_binary_operator((Code_Node_Binary_Operator *)root, interp);
		case CODE_NODE_ADDRESS: return interp_eval_address((Code_Node_Address *)root, interp);
		case CODE_NODE_ASSIGNMENT: return interp_eval_assignment((Code_Node_Assignment *)root, interp);
		case CODE_NODE_TYPE_CAST: return interp_eval_type_cast((Code_Node_Type_Cast *)root, interp);
		case CODE_NODE_IF: return interp_eval_expression((Code_Node *)root, interp);
		case CODE_NODE_PROCEDURE_CALL: return interp_eval_procedure_call((Code_Node_Procedure_Call *)root, interp);
		case CODE_NODE_RETURN: return interp_eval_return((Code_Node_Return *)root, interp);
		
		NoDefaultCase();
	}
	
	Unreachable();
	return Evaluation_Value{};
}

static Evaluation_Value interp_eval_root_expression(Code_Node_Expression *root, Interp *interp)
{
	return interp_eval_expression(root->child, interp);
}

static bool interp_eval_statement(Code_Node_Statement *root, Interp *interp, Evaluation_Value *value);

static void interp_eval_do(Code_Node_Do *root, Interp *interp)
{
	auto do_cond = root->condition;
	auto do_body = root->body;
	
	Evaluation_Value cond;
	
	do
	{
		interp_eval_statement(do_body, interp, nullptr);
		Assert(interp_eval_statement(do_cond, interp, &cond));
	} while (EvaluationTypeValue(cond, bool));
}

static void interp_eval_if(Code_Node_While *root, Interp *interp)
{
	auto while_cond = root->condition;
	auto while_body = root->body;
	
	Evaluation_Value cond;
	Assert(interp_eval_statement(while_cond, interp, &cond));
	
	while (EvaluationTypeValue(cond, bool))
	{
		interp_eval_statement(while_body, interp, nullptr);
		Assert(interp_eval_statement(while_cond, interp, &cond));
	}
}

static void interp_eval_if_block(Code_Node_If *root, Interp *interp)
{
	auto cond = interp_eval_root_expression((Code_Node_Expression *)root->condition, interp);
	if (EvaluationTypeValue(cond, bool))
		interp_eval_statement((Code_Node_Statement *)root->true_statement, interp, nullptr);
	else
	{
		if (root->false_statement)
			interp_eval_statement((Code_Node_Statement *)root->false_statement, interp, nullptr);
	}
}

static void interp_eval_for_block(Code_Node_For *root, Interp *interp)
{
	auto for_init = root->initialization;
	auto for_cond = root->condition;
	auto for_incr = root->increment;
	auto for_body = root->body;
	
	Evaluation_Value cond;
	
	interp_eval_statement(for_init, interp, nullptr);
	Assert(interp_eval_statement(for_cond, interp, &cond));
	
	while (EvaluationTypeValue(cond, bool))
	{
		interp_eval_statement(for_body, interp, nullptr);
		Assert(interp_eval_statement(for_incr, interp, nullptr));
		Assert(interp_eval_statement(for_cond, interp, &cond));
	}
}

static bool interp_eval_statement(Code_Node_Statement *root, Interp *interp, Evaluation_Value *out_value)
{
	Assert(root->symbol_table);

	//interp->intercept(interp, top, root);
	//printf("Executing line: %zu\n", root->source_row);

	Evaluation_Value value;
	Evaluation_Value *dst = out_value ? out_value : &value;

	switch (root->node->kind)
	{
		case CODE_NODE_EXPRESSION: 
			*dst = interp_eval_root_expression((Code_Node_Expression *)root->node, interp);
			return true;
		
		case CODE_NODE_ASSIGNMENT:
			*dst = interp_eval_assignment((Code_Node_Assignment *)root->node, interp);
			return true;
		
		case CODE_NODE_BLOCK:			
			interp_eval_block((Code_Node_Block *)root->node, interp, false);
			return false;
		
		case CODE_NODE_IF:
			interp_eval_if_block((Code_Node_If *)root->node, interp);
			return false;
		
		case CODE_NODE_FOR:
			interp_eval_for_block((Code_Node_For *)root->node, interp);
			return false;
		
		case CODE_NODE_WHILE:
			interp_eval_if((Code_Node_While *)root->node, interp);
			return false;
		
		case CODE_NODE_DO:
			interp_eval_do((Code_Node_Do *)root->node, interp);
			return false;
		
		NoDefaultCase();
	}

	Unreachable();
	
	return false;
}

static void interp_eval_block(Code_Node_Block *root, Interp *interp, bool isproc)
{
	auto return_index = interp->return_count;
	for (auto statement = root->statement_head; statement; statement = statement->next)
	{
		interp_eval_statement(statement, interp, nullptr);
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
		interp_eval_assignment(expr, interp);
}

void interp_eval_procedure_call(Interp *interp, Code_Node_Block *proc)
{
	interp_eval_block(proc, interp, true);
}
