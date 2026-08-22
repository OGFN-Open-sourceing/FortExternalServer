#pragma once

#include "Runtime/Core/Public/CoreTypes.h"

#include <string>

namespace FPaths
{
    std::wstring GetExecutableDirectory();

    std::wstring GetParentDirectory(const std::wstring& Path);

    std::wstring Combine(const std::wstring& Left, const std::wstring& Right);

    std::wstring NormalizeSeparators(const std::wstring& Path);

    bool FileExists(const std::wstring& Path);

    bool DirectoryExists(const std::wstring& Path);

    bool CreateDirectoryTree(const std::wstring& Path);

    std::wstring FindBuildRoot(const std::wstring& StartDirectory);
}
