#include "Runtime/Core/Public/Misc/CommandLine.h"
#include "Runtime/Core/Public/Containers/StringConv.h"

std::vector<std::string> FCommandLine::Tokens;

void FCommandLine::Initialize(int ArgumentCount, char** Arguments)
{
    Tokens.clear();
    Tokens.reserve(static_cast<size_t>(ArgumentCount));

    for (int Index = 1; Index < ArgumentCount; ++Index)
    {
        Tokens.emplace_back(Arguments[Index]);
    }
}

bool FCommandLine::HasSwitch(std::string_view Name)
{
    for (const std::string& Token : Tokens)
    {
        if (Token.size() < 2 || Token[0] != '-')
        {
            continue;
        }

        if (FStringConv::EqualsIgnoreCase(std::string_view(Token).substr(1), Name))
        {
            return true;
        }
    }

    return false;
}

std::string FCommandLine::GetValue(std::string_view Name, std::string_view DefaultValue)
{
    for (const std::string& Token : Tokens)
    {
        std::string_view Candidate(Token);
        if (!Candidate.empty() && Candidate[0] == '-')
        {
            Candidate.remove_prefix(1);
        }

        const size_t Assignment = Candidate.find('=');
        if (Assignment == std::string_view::npos)
        {
            continue;
        }

        if (FStringConv::EqualsIgnoreCase(Candidate.substr(0, Assignment), Name))
        {
            return std::string(Candidate.substr(Assignment + 1));
        }
    }

    return std::string(DefaultValue);
}

const std::vector<std::string>& FCommandLine::GetTokens()
{
    return Tokens;
}
