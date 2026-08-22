#include "Runtime/Core/Public/Misc/ConfigCacheIni.h"
#include "Runtime/Core/Public/Containers/StringConv.h"

#include <filesystem>
#include <fstream>

bool FConfigFile::Load(const std::wstring& FilePath)
{
    std::ifstream Stream{ std::filesystem::path(FilePath) };
    if (!Stream.is_open())
    {
        return false;
    }

    std::string CurrentSection;
    std::string Line;

    while (std::getline(Stream, Line))
    {
        const std::string Trimmed = FStringConv::TrimWhitespace(Line);
        if (Trimmed.empty() || Trimmed[0] == ';' || Trimmed[0] == '#')
        {
            continue;
        }

        if (Trimmed.front() == '[' && Trimmed.back() == ']')
        {
            CurrentSection = FStringConv::TrimWhitespace(Trimmed.substr(1, Trimmed.size() - 2));
            Sections.try_emplace(CurrentSection);
            continue;
        }

        const size_t Assignment = Trimmed.find('=');
        if (Assignment == std::string::npos)
        {
            continue;
        }

        const std::string Key = FStringConv::TrimWhitespace(Trimmed.substr(0, Assignment));
        const std::string Value = FStringConv::TrimWhitespace(Trimmed.substr(Assignment + 1));

        Sections[CurrentSection][Key] = Value;
    }

    bLoaded = true;
    return true;
}

bool FConfigFile::IsLoaded() const
{
    return bLoaded;
}

const std::string* FConfigFile::FindValue(std::string_view Section, std::string_view Key) const
{
    const auto SectionIterator = Sections.find(Section);
    if (SectionIterator == Sections.end())
    {
        return nullptr;
    }

    const auto KeyIterator = SectionIterator->second.find(Key);
    if (KeyIterator == SectionIterator->second.end())
    {
        return nullptr;
    }

    return &KeyIterator->second;
}

std::string FConfigFile::GetString(std::string_view Section, std::string_view Key, std::string_view DefaultValue) const
{
    const std::string* Value = FindValue(Section, Key);
    return Value != nullptr ? *Value : std::string(DefaultValue);
}

bool FConfigFile::GetBool(std::string_view Section, std::string_view Key, bool DefaultValue) const
{
    const std::string* Value = FindValue(Section, Key);
    return Value != nullptr ? FStringConv::ParseBool(*Value, DefaultValue) : DefaultValue;
}

int64 FConfigFile::GetInt(std::string_view Section, std::string_view Key, int64 DefaultValue) const
{
    const std::string* Value = FindValue(Section, Key);
    return Value != nullptr ? FStringConv::ParseInt(*Value, DefaultValue) : DefaultValue;
}

double FConfigFile::GetDouble(std::string_view Section, std::string_view Key, double DefaultValue) const
{
    const std::string* Value = FindValue(Section, Key);
    return Value != nullptr ? FStringConv::ParseDouble(*Value, DefaultValue) : DefaultValue;
}

float FConfigFile::GetFloat(std::string_view Section, std::string_view Key, float DefaultValue) const
{
    return static_cast<float>(GetDouble(Section, Key, static_cast<double>(DefaultValue)));
}
