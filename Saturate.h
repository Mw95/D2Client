#pragma once

//  Saturate.h -- 64-bit arithmetic for life and damage. Hand-written.
//
//  The game keeps life, mana, stamina and damage as signed 32-bit values, most of them 24.8 fixed point (the value << 8). The original
//  multiplies them in 32 bits for display: the globes take `80 * life / max`, a hit flash `100 * life / max`, a health bar `life << 7`.
//  These wrap long before the values themselves do, from about 65,536 life for `<< 7`. Every place this rewrite fixes such a wrap does the
//  product in 64 bits through one of these helpers and says `DIFFERENCE: overflow fix` where it does. For an input whose original result did
//  not wrap, each helper gives exactly the original result (the same truncation toward zero); see OVERFLOW_FIXES.md.

#include <cstdint>

inline constexpr int32_t SATURATE_ToInt32(int64_t nValue)
{
    return nValue > INT32_MAX ? INT32_MAX : (nValue < INT32_MIN ? INT32_MIN : (int32_t)nValue);
}

//  nValue * nMultiplier / nDivisor, the product in 64 bits; 0 when nDivisor is 0.
inline constexpr int32_t SATURATE_MulDiv(int32_t nValue, int32_t nMultiplier, int32_t nDivisor)
{
    return nDivisor ? SATURATE_ToInt32((int64_t)nValue * nMultiplier / nDivisor) : 0;
}

inline constexpr int32_t SATURATE_Add(int32_t a, int32_t b)
{
    return SATURATE_ToInt32((int64_t)a + b);
}

inline constexpr int32_t SATURATE_Mul(int32_t a, int32_t b)
{
    return SATURATE_ToInt32((int64_t)a * b);
}

//  nValue << nShift, for a whole value made fixed point.
inline constexpr int32_t SATURATE_Shl(int32_t nValue, int32_t nShift)
{
    return SATURATE_ToInt32((int64_t)nValue * ((int64_t)1 << nShift));
}
