#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace FStringConv
{
    std::wstring ToWide(std::string_view Narrow);

    std::string ToNarrow(std::wstring_view Wide);

    std::string TrimWhitespace(std::string_view Value);

    std::string ToLower(std::string_view Value);

    bool EqualsIgnoreCase(std::string_view Left, std::string_view Right);

    bool StartsWithIgnoreCase(std::string_view Value, std::string_view Prefix);

    std::vector<std::string> SplitBy(std::string_view Value, char Delimiter);

    std::string ToHex(uint64 Value);

    std::string ToHexBytes(const uint8* Bytes, size_t Count);

    bool ParseBool(std::string_view Value, bool DefaultValue);

    int64 ParseInt(std::string_view Value, int64 DefaultValue);

    double ParseDouble(std::string_view Value, double DefaultValue);
}
