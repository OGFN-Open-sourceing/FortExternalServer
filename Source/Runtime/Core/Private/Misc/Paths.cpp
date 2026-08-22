#include "Runtime/Core/Public/Misc/Paths.h"
#include "Runtime/Core/Public/HAL/WindowsPlatform.h"

#include <algorithm>

namespace FPaths
{
    std::wstring GetExecutableDirectory()
    {
        wchar_t Buffer[MAX_PATH] = {};
        const DWORD Length = ::GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
        if (Length == 0)
        {
            return std::wstring();
        }

        return GetParentDirectory(std::wstring(Buffer, Length));
    }

    std::wstring GetParentDirectory(const std::wstring& Path)
    {
        const std::wstring Normalized = NormalizeSeparators(Path);
        const size_t Separator = Normalized.find_last_of(L'\\');
        if (Separator == std::wstring::npos)
        {
            return std::wstring();
        }

        return Normalized.substr(0, Separator);
    }

    std::wstring Combine(const std::wstring& Left, const std::wstring& Right)
    {
        if (Left.empty())
        {
            return NormalizeSeparators(Right);
        }

        if (Right.empty())
        {
            return NormalizeSeparators(Left);
        }

        std::wstring Result = NormalizeSeparators(Left);
        if (Result.back() != L'\\')
        {
            Result.push_back(L'\\');
        }

        std::wstring Suffix = NormalizeSeparators(Right);
        if (!Suffix.empty() && Suffix.front() == L'\\')
        {
            Suffix.erase(Suffix.begin());
        }

        Result.append(Suffix);
        return Result;
    }

    std::wstring NormalizeSeparators(const std::wstring& Path)
    {
        std::wstring Result = Path;
        std::replace(Result.begin(), Result.end(), L'/', L'\\');
        return Result;
    }

    bool FileExists(const std::wstring& Path)
    {
        const DWORD Attributes = ::GetFileAttributesW(Path.c_str());
        return Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExists(const std::wstring& Path)
    {
        const DWORD Attributes = ::GetFileAttributesW(Path.c_str());
        return Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool CreateDirectoryTree(const std::wstring& Path)
    {
        if (Path.empty() || DirectoryExists(Path))
        {
            return true;
        }

        const std::wstring Parent = GetParentDirectory(Path);
        if (!Parent.empty() && Parent != Path)
        {
            CreateDirectoryTree(Parent);
        }

        return ::CreateDirectoryW(Path.c_str(), nullptr) != FALSE || ::GetLastError() == ERROR_ALREADY_EXISTS;
    }

    std::wstring FindBuildRoot(const std::wstring& StartDirectory)
    {
        std::wstring Current = NormalizeSeparators(StartDirectory);

        while (!Current.empty())
        {
            if (DirectoryExists(Combine(Current, L"FortniteGame")) && DirectoryExists(Combine(Current, L"Engine")))
            {
                return Current;
            }

            const std::wstring Parent = GetParentDirectory(Current);
            if (Parent == Current)
            {
                break;
            }

            Current = Parent;
        }

        return std::wstring();
    }
}
