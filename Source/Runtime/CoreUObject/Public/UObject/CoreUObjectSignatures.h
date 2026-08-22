#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

namespace FCoreUObjectSignatures
{
    inline constexpr std::string_view ObjectArrayReference = "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8B 04 D1";
    inline constexpr std::string_view ObjectArrayReferenceAlternate = "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8D 04 D1";

    inline constexpr std::string_view StaticFindObject = "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 45 33 F6";
    inline constexpr std::string_view StaticFindObjectAlternate = "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 60";

    inline constexpr std::wstring_view StaticFindObjectAnchor = L"Illegal call to StaticFindObject() while serializing object data!";

    inline constexpr std::string_view MemoryRealloc = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC ? 48 8B F1 41 8B D8 48 8B 0D ? ? ? ?";

    inline constexpr std::string_view NameToString = "48 89 5C 24 ? 57 48 83 EC 40 83 79 04 00 48 8B DA 48 8B F9";

    inline constexpr std::wstring_view NameConstructorAnchor = L"ClientIgnoreLookInput";
    inline constexpr std::wstring_view ProcessEventAnchor = L"AccessNoneNoContext";
    inline constexpr std::wstring_view StaticLoadObjectAnchor = L"STAT_LoadObject";
    inline constexpr std::wstring_view SpawnActorAnchor = L"SpawnActor failed because no class was specified";

    inline constexpr size_t NameConstructorScanRange = 0x400;
    inline constexpr size_t FunctionStartBacktrack = 0x1000;
    inline constexpr size_t AnchorScanRange = 0xC00;
    inline constexpr int32 VirtualTableProbeCount = 256;
}
