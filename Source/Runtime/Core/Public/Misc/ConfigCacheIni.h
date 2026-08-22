#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <map>
#include <string>
#include <string_view>

class FConfigFile
{
public:
    bool Load(const std::wstring& FilePath);

    bool IsLoaded() const;

    std::string GetString(std::string_view Section, std::string_view Key, std::string_view DefaultValue) const;

    bool GetBool(std::string_view Section, std::string_view Key, bool DefaultValue) const;

    int64 GetInt(std::string_view Section, std::string_view Key, int64 DefaultValue) const;

    double GetDouble(std::string_view Section, std::string_view Key, double DefaultValue) const;

    float GetFloat(std::string_view Section, std::string_view Key, float DefaultValue) const;

private:
    using FSection = std::map<std::string, std::string, std::less<>>;

    const std::string* FindValue(std::string_view Section, std::string_view Key) const;

    std::map<std::string, FSection, std::less<>> Sections;
    bool bLoaded = false;
};
