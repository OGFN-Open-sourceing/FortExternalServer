#include "Runtime/RemoteProcess/Public/ProcessAttachment.h"

#include <algorithm>
#include <cwctype>

namespace
{
    bool EqualsIgnoreCaseWide(const std::wstring& Left, const std::wstring& Right)
    {
        if (Left.size() != Right.size())
        {
            return false;
        }

        for (size_t Index = 0; Index < Left.size(); ++Index)
        {
            if (std::towlower(static_cast<wint_t>(Left[Index])) != std::towlower(static_cast<wint_t>(Right[Index])))
            {
                return false;
            }
        }

        return true;
    }
}

const std::vector<FRemoteModuleInfo>& FProcessAttachment::GetModules() const
{
    return Modules;
}

const FRemoteModuleInfo* FProcessAttachment::FindModule(std::wstring_view ModuleName) const
{
    const std::wstring Target(ModuleName);

    const auto Found = std::find_if(Modules.begin(), Modules.end(), [&Target](const FRemoteModuleInfo& Entry) {
        return EqualsIgnoreCaseWide(Entry.Name, Target);
    });

    return Found != Modules.end() ? &(*Found) : nullptr;
}

const FRemoteModuleInfo* FProcessAttachment::GetPrimaryModule() const
{
    const FRemoteModuleInfo* Exact = FindModule(PrimaryModuleName);
    if (Exact != nullptr)
    {
        return Exact;
    }

    const auto Found = std::find_if(Modules.begin(), Modules.end(), [this](const FRemoteModuleInfo& Entry) {
        return Entry.Name.find(PrimaryModuleName) != std::wstring::npos || PrimaryModuleName.find(Entry.Name) != std::wstring::npos;
    });

    return Found != Modules.end() ? &(*Found) : nullptr;
}
