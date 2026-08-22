#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>
#include <string_view>
#include <vector>

class FCommandLine
{
public:
    static void Initialize(int ArgumentCount, char** Arguments);

    static bool HasSwitch(std::string_view Name);

    static std::string GetValue(std::string_view Name, std::string_view DefaultValue);

    static const std::vector<std::string>& GetTokens();

private:
    static std::vector<std::string> Tokens;
};
