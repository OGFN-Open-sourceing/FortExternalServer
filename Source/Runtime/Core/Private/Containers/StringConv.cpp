#include "Runtime/Core/Public/Containers/StringConv.h"
#include "Runtime/Core/Public/HAL/WindowsPlatform.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace FStringConv
{
    std::wstring ToWide(std::string_view Narrow)
    {
        if (Narrow.empty())
        {
            return std::wstring();
        }

        const int Required = ::MultiByteToWideChar(CP_UTF8, 0, Narrow.data(), static_cast<int>(Narrow.size()), nullptr, 0);
        if (Required <= 0)
        {
            return std::wstring();
        }

        std::wstring Result(static_cast<size_t>(Required), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, Narrow.data(), static_cast<int>(Narrow.size()), Result.data(), Required);
        return Result;
    }

    std::string ToNarrow(std::wstring_view Wide)
    {
        if (Wide.empty())
        {
            return std::string();
        }

        const int Required = ::WideCharToMultiByte(CP_UTF8, 0, Wide.data(), static_cast<int>(Wide.size()), nullptr, 0, nullptr, nullptr);
        if (Required <= 0)
        {
            return std::string();
        }

        std::string Result(static_cast<size_t>(Required), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, Wide.data(), static_cast<int>(Wide.size()), Result.data(), Required, nullptr, nullptr);
        return Result;
    }

    std::string TrimWhitespace(std::string_view Value)
    {
        size_t First = 0;
        while (First < Value.size() && std::isspace(static_cast<unsigned char>(Value[First])) != 0)
        {
            ++First;
        }

        size_t Last = Value.size();
        while (Last > First && std::isspace(static_cast<unsigned char>(Value[Last - 1])) != 0)
        {
            --Last;
        }

        return std::string(Value.substr(First, Last - First));
    }

    std::string ToLower(std::string_view Value)
    {
        std::string Result(Value);
        std::transform(Result.begin(), Result.end(), Result.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
        return Result;
    }

    bool EqualsIgnoreCase(std::string_view Left, std::string_view Right)
    {
        if (Left.size() != Right.size())
        {
            return false;
        }

        for (size_t Index = 0; Index < Left.size(); ++Index)
        {
            if (std::tolower(static_cast<unsigned char>(Left[Index])) != std::tolower(static_cast<unsigned char>(Right[Index])))
            {
                return false;
            }
        }

        return true;
    }

    bool StartsWithIgnoreCase(std::string_view Value, std::string_view Prefix)
    {
        return Value.size() >= Prefix.size() && EqualsIgnoreCase(Value.substr(0, Prefix.size()), Prefix);
    }

    std::vector<std::string> SplitBy(std::string_view Value, char Delimiter)
    {
        std::vector<std::string> Result;
        size_t Start = 0;

        while (Start <= Value.size())
        {
            const size_t Found = Value.find(Delimiter, Start);
            if (Found == std::string_view::npos)
            {
                Result.emplace_back(Value.substr(Start));
                break;
            }

            Result.emplace_back(Value.substr(Start, Found - Start));
            Start = Found + 1;
        }

        return Result;
    }

    std::string ToHex(uint64 Value)
    {
        static constexpr char Digits[] = "0123456789ABCDEF";

        char Buffer[19] = { '0', 'x' };
        int Position = 2;

        bool bLeading = true;
        for (int Shift = 60; Shift >= 0; Shift -= 4)
        {
            const uint8 Nibble = static_cast<uint8>((Value >> Shift) & 0xF);
            if (bLeading && Nibble == 0 && Shift != 0)
            {
                continue;
            }

            bLeading = false;
            Buffer[Position++] = Digits[Nibble];
        }

        return std::string(Buffer, static_cast<size_t>(Position));
    }

    std::string ToHexBytes(const uint8* Bytes, size_t Count)
    {
        static constexpr char Digits[] = "0123456789ABCDEF";

        std::string Result;
        Result.reserve(Count * 3);

        for (size_t Index = 0; Index < Count; ++Index)
        {
            if (Index != 0)
            {
                Result.push_back(' ');
            }

            Result.push_back(Digits[Bytes[Index] >> 4]);
            Result.push_back(Digits[Bytes[Index] & 0xF]);
        }

        return Result;
    }

    bool ParseBool(std::string_view Value, bool DefaultValue)
    {
        const std::string Trimmed = TrimWhitespace(Value);
        if (Trimmed.empty())
        {
            return DefaultValue;
        }

        if (EqualsIgnoreCase(Trimmed, "true") || Trimmed == "1" || EqualsIgnoreCase(Trimmed, "yes") || EqualsIgnoreCase(Trimmed, "on"))
        {
            return true;
        }

        if (EqualsIgnoreCase(Trimmed, "false") || Trimmed == "0" || EqualsIgnoreCase(Trimmed, "no") || EqualsIgnoreCase(Trimmed, "off"))
        {
            return false;
        }

        return DefaultValue;
    }

    int64 ParseInt(std::string_view Value, int64 DefaultValue)
    {
        const std::string Trimmed = TrimWhitespace(Value);
        if (Trimmed.empty())
        {
            return DefaultValue;
        }

        char* End = nullptr;
        const long long Parsed = std::strtoll(Trimmed.c_str(), &End, 0);
        if (End == Trimmed.c_str())
        {
            return DefaultValue;
        }

        return static_cast<int64>(Parsed);
    }

    double ParseDouble(std::string_view Value, double DefaultValue)
    {
        const std::string Trimmed = TrimWhitespace(Value);
        if (Trimmed.empty())
        {
            return DefaultValue;
        }

        char* End = nullptr;
        const double Parsed = std::strtod(Trimmed.c_str(), &End);
        if (End == Trimmed.c_str())
        {
            return DefaultValue;
        }

        return Parsed;
    }
}
