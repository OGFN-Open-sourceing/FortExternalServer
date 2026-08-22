#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;

using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using FRemoteAddress = std::uint64_t;

inline constexpr FRemoteAddress InvalidRemoteAddress = 0;

inline constexpr int32 InvalidIndex = -1;

template<typename EnumType>
constexpr auto ToUnderlying(EnumType Value)
{
    return static_cast<std::underlying_type_t<EnumType>>(Value);
}

template<typename ValueType>
constexpr ValueType AlignUp(ValueType Value, ValueType Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

template<typename ValueType>
constexpr ValueType AlignDown(ValueType Value, ValueType Alignment)
{
    return Value & ~(Alignment - 1);
}
