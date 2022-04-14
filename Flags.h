#pragma once
#include <stdint.h>

constexpr uint32_t SYMBOL_BIT_LVALUE       = 0x1;
constexpr uint32_t SYMBOL_BIT_CONSTANT     = 0x2;
constexpr uint32_t SYMBOL_BIT_TYPE         = 0x4;
constexpr uint32_t SYMBOL_BIT_CONST_EXPR   = 0x8;
constexpr uint32_t SYMBOL_BIT_COMPILER_DEF = 0x10;
