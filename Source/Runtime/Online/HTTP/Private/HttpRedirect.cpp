#include "Runtime/Online/HTTP/Public/HttpRedirect.h"
#include "Runtime/RemoteProcess/Public/X64Emitter.h"

#include <cstring>

namespace
{
    constexpr std::u16string_view RedirectedDomains[] = { u"epicgames.com", u"epicgames.net" };
    constexpr int32 DomainCharacters = 13;
    constexpr int32 DomainBytes = DomainCharacters * static_cast<int32>(sizeof(char16_t));
    constexpr int32 DomainStride = 32;
    constexpr int32 CharacterBytes = static_cast<int32>(sizeof(char16_t));

    constexpr int32 ShadowSpace = 0x20;
    constexpr int32 RedirectedStringOffset = ShadowSpace;
    constexpr int32 RedirectedStringNumOffset = RedirectedStringOffset + 0x08;
    constexpr int32 RedirectedStringMaxOffset = RedirectedStringOffset + 0x0C;
    constexpr int32 RedirectedTextOffset = RedirectedStringOffset + 0x10;
    constexpr int32 RedirectedTextBytes = 0x800;
    constexpr int32 FrameSize = RedirectedTextOffset + RedirectedTextBytes;

    constexpr EX64Register SavedRegisters[] = { EX64Register::Rbx, EX64Register::Rsi, EX64Register::Rdi, EX64Register::R12, EX64Register::R13 };

    std::vector<uint8> BuildDomainTable()
    {
        std::vector<uint8> Table(std::size(RedirectedDomains) * DomainStride, 0);
        for (size_t Index = 0; Index < std::size(RedirectedDomains); ++Index)
        {
            std::memcpy(Table.data() + Index * DomainStride, RedirectedDomains[Index].data(), DomainBytes);
        }
        return Table;
    }

    void EmitScanUntil(FX64Emitter& Emitter, EX64Register Cursor, std::initializer_list<char16_t> Terminators, FX64Emitter::FLabelId Found)
    {
        const FX64Emitter::FLabelId Loop = Emitter.CreateLabel();
        Emitter.BindLabel(Loop);
        Emitter.LoadWordZeroExtended(EX64Register::Rax, Cursor, 0);
        Emitter.TestRegisterRegister(EX64Register::Rax, EX64Register::Rax);
        Emitter.JumpConditionalToLabel(EX64Condition::Zero, Found);
        for (const char16_t Terminator : Terminators)
        {
            Emitter.CompareRegisterImmediate(EX64Register::Rax, Terminator);
            Emitter.JumpConditionalToLabel(EX64Condition::Equal, Found);
        }
        Emitter.AddRegisterImmediate(Cursor, CharacterBytes);
        Emitter.JumpToLabel(Loop);
    }
}

FRemoteAddress FHttpRedirect::EmitSetURLOverride(FRemoteAddress DomainTable, FRemoteAddress BackendText) const
{
    FX64Emitter Emitter;

    const FX64Emitter::FLabelId PassThrough = Emitter.CreateLabel();
    const FX64Emitter::FLabelId CallOriginal = Emitter.CreateLabel();
    const FX64Emitter::FLabelId SchemeLoop = Emitter.CreateLabel();
    const FX64Emitter::FLabelId SchemeNext = Emitter.CreateLabel();
    const FX64Emitter::FLabelId HostStart = Emitter.CreateLabel();
    const FX64Emitter::FLabelId HostEnd = Emitter.CreateLabel();
    const FX64Emitter::FLabelId Matched = Emitter.CreateLabel();
    const FX64Emitter::FLabelId PathStart = Emitter.CreateLabel();
    const FX64Emitter::FLabelId CopyBackend = Emitter.CreateLabel();
    const FX64Emitter::FLabelId CopyPath = Emitter.CreateLabel();
    const FX64Emitter::FLabelId BuildString = Emitter.CreateLabel();

    for (const EX64Register Register : SavedRegisters)
    {
        Emitter.PushRegister(Register);
    }
    Emitter.SubRegisterImmediate(EX64Register::Rsp, FrameSize);

    Emitter.MoveRegisterRegister(EX64Register::R12, EX64Register::Rcx);
    Emitter.MoveRegisterRegister(EX64Register::R13, EX64Register::Rdx);
    Emitter.LoadRegisterFromMemory(EX64Register::Rsi, EX64Register::R13, 0);
    Emitter.TestRegisterRegister(EX64Register::Rsi, EX64Register::Rsi);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, PassThrough);

    Emitter.BindLabel(SchemeLoop);
    Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rsi, 0);
    Emitter.TestRegisterRegister(EX64Register::Rax, EX64Register::Rax);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, PassThrough);
    Emitter.CompareRegisterImmediate(EX64Register::Rax, u':');
    Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, SchemeNext);
    Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rsi, CharacterBytes);
    Emitter.CompareRegisterImmediate(EX64Register::Rax, u'/');
    Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, SchemeNext);
    Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rsi, 2 * CharacterBytes);
    Emitter.CompareRegisterImmediate(EX64Register::Rax, u'/');
    Emitter.JumpConditionalToLabel(EX64Condition::Equal, HostStart);
    Emitter.BindLabel(SchemeNext);
    Emitter.AddRegisterImmediate(EX64Register::Rsi, CharacterBytes);
    Emitter.JumpToLabel(SchemeLoop);

    Emitter.BindLabel(HostStart);
    Emitter.AddRegisterImmediate(EX64Register::Rsi, 3 * CharacterBytes);
    Emitter.MoveRegisterRegister(EX64Register::Rbx, EX64Register::Rsi);
    EmitScanUntil(Emitter, EX64Register::Rsi, { u'/', u'?', u'#', u':' }, HostEnd);

    Emitter.BindLabel(HostEnd);
    Emitter.MoveRegisterRegister(EX64Register::Rcx, EX64Register::Rbx);
    Emitter.AddRegisterImmediate(EX64Register::Rcx, DomainBytes);
    Emitter.CompareRegisterRegister(EX64Register::Rsi, EX64Register::Rcx);
    Emitter.JumpConditionalToLabel(EX64Condition::Below, PassThrough);

    for (size_t Index = 0; Index < std::size(RedirectedDomains); ++Index)
    {
        const FX64Emitter::FLabelId CompareLoop = Emitter.CreateLabel();
        const FX64Emitter::FLabelId Mismatch = Emitter.CreateLabel();

        Emitter.MoveRegisterRegister(EX64Register::Rdi, EX64Register::Rsi);
        Emitter.SubRegisterImmediate(EX64Register::Rdi, DomainBytes);
        Emitter.MoveRegisterImmediate(EX64Register::R8, DomainTable + Index * DomainStride);
        Emitter.MoveRegisterImmediate(EX64Register::R9, DomainCharacters);

        Emitter.BindLabel(CompareLoop);
        Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rdi, 0);
        Emitter.OrRegisterImmediate(EX64Register::Rax, 0x20);
        Emitter.LoadWordZeroExtended(EX64Register::Rcx, EX64Register::R8, 0);
        Emitter.CompareRegisterRegister(EX64Register::Rax, EX64Register::Rcx);
        Emitter.JumpConditionalToLabel(EX64Condition::NotEqual, Mismatch);
        Emitter.AddRegisterImmediate(EX64Register::Rdi, CharacterBytes);
        Emitter.AddRegisterImmediate(EX64Register::R8, CharacterBytes);
        Emitter.SubRegisterImmediate(EX64Register::R9, 1);
        Emitter.JumpConditionalToLabel(EX64Condition::NotZero, CompareLoop);

        Emitter.MoveRegisterRegister(EX64Register::Rcx, EX64Register::Rbx);
        Emitter.AddRegisterImmediate(EX64Register::Rcx, DomainBytes);
        Emitter.CompareRegisterRegister(EX64Register::Rsi, EX64Register::Rcx);
        Emitter.JumpConditionalToLabel(EX64Condition::Equal, Matched);
        Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rsi, -(DomainBytes + CharacterBytes));
        Emitter.CompareRegisterImmediate(EX64Register::Rax, u'.');
        Emitter.JumpConditionalToLabel(EX64Condition::Equal, Matched);

        Emitter.BindLabel(Mismatch);
    }
    Emitter.JumpToLabel(PassThrough);

    Emitter.BindLabel(Matched);
    Emitter.MoveRegisterRegister(EX64Register::R8, EX64Register::Rsi);
    EmitScanUntil(Emitter, EX64Register::R8, { u'/', u'?', u'#' }, PathStart);

    Emitter.BindLabel(PathStart);
    Emitter.MoveRegisterRegister(EX64Register::Rdi, EX64Register::Rsp);
    Emitter.AddRegisterImmediate(EX64Register::Rdi, RedirectedTextOffset);
    Emitter.MoveRegisterRegister(EX64Register::R9, EX64Register::Rdi);
    Emitter.AddRegisterImmediate(EX64Register::R9, RedirectedTextBytes - CharacterBytes);
    Emitter.MoveRegisterImmediate(EX64Register::Rdx, BackendText);
    Emitter.MoveRegisterImmediate(EX64Register::R10, 1);

    Emitter.BindLabel(CopyBackend);
    Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::Rdx, 0);
    Emitter.TestRegisterRegister(EX64Register::Rax, EX64Register::Rax);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, CopyPath);
    Emitter.StoreWordToMemory(EX64Register::Rdi, 0, EX64Register::Rax);
    Emitter.AddRegisterImmediate(EX64Register::Rdi, CharacterBytes);
    Emitter.AddRegisterImmediate(EX64Register::Rdx, CharacterBytes);
    Emitter.AddRegisterImmediate(EX64Register::R10, 1);
    Emitter.JumpToLabel(CopyBackend);

    Emitter.BindLabel(CopyPath);
    Emitter.LoadWordZeroExtended(EX64Register::Rax, EX64Register::R8, 0);
    Emitter.StoreWordToMemory(EX64Register::Rdi, 0, EX64Register::Rax);
    Emitter.TestRegisterRegister(EX64Register::Rax, EX64Register::Rax);
    Emitter.JumpConditionalToLabel(EX64Condition::Zero, BuildString);
    Emitter.AddRegisterImmediate(EX64Register::Rdi, CharacterBytes);
    Emitter.AddRegisterImmediate(EX64Register::R8, CharacterBytes);
    Emitter.AddRegisterImmediate(EX64Register::R10, 1);
    Emitter.CompareRegisterRegister(EX64Register::Rdi, EX64Register::R9);
    Emitter.JumpConditionalToLabel(EX64Condition::AboveOrEqual, PassThrough);
    Emitter.JumpToLabel(CopyPath);

    Emitter.BindLabel(BuildString);
    Emitter.MoveRegisterRegister(EX64Register::Rax, EX64Register::Rsp);
    Emitter.AddRegisterImmediate(EX64Register::Rax, RedirectedTextOffset);
    Emitter.StoreRegisterToMemory(EX64Register::Rsp, RedirectedStringOffset, EX64Register::Rax);
    Emitter.StoreDwordToMemory(EX64Register::Rsp, RedirectedStringNumOffset, EX64Register::R10);
    Emitter.StoreDwordToMemory(EX64Register::Rsp, RedirectedStringMaxOffset, EX64Register::R10);
    Emitter.MoveRegisterRegister(EX64Register::Rcx, EX64Register::R12);
    Emitter.MoveRegisterRegister(EX64Register::Rdx, EX64Register::Rsp);
    Emitter.AddRegisterImmediate(EX64Register::Rdx, RedirectedStringOffset);
    Emitter.JumpToLabel(CallOriginal);

    Emitter.BindLabel(PassThrough);
    Emitter.MoveRegisterRegister(EX64Register::Rcx, EX64Register::R12);
    Emitter.MoveRegisterRegister(EX64Register::Rdx, EX64Register::R13);

    Emitter.BindLabel(CallOriginal);
    Emitter.MoveRegisterImmediate(EX64Register::Rax, OriginalSetURL);
    Emitter.CallRegister(EX64Register::Rax);
    Emitter.AddRegisterImmediate(EX64Register::Rsp, FrameSize);
    for (auto Register = std::rbegin(SavedRegisters); Register != std::rend(SavedRegisters); ++Register)
    {
        Emitter.PopRegister(*Register);
    }
    Emitter.EmitReturn();

    return Bridge->EmitRoutine(Emitter);
}

bool FHttpRedirect::WriteVirtualSlot(FRemoteAddress Value) const
{
    const FRemoteMemory& Memory = Bridge->GetMemory();
    const FRemoteAddress Slot = RequestVirtualTable + SetURLVirtualIndex * sizeof(FRemoteAddress);

    uint32 PreviousProtection = 0;
    if (!Memory.ProtectRange(Slot, sizeof(FRemoteAddress), PAGE_READWRITE, PreviousProtection))
    {
        return false;
    }

    const bool bWritten = Memory.Write<FRemoteAddress>(Slot, Value);

    uint32 RestoredProtection = 0;
    Memory.ProtectRange(Slot, sizeof(FRemoteAddress), PreviousProtection, RestoredProtection);
    return bWritten;
}

bool FHttpRedirect::Install(FGameThreadBridge& InBridge, FRemoteAddress InRequestVirtualTable, FRemoteAddress InSetURL, const std::wstring& BackendUrl, uint64 TimeoutMilliseconds)
{
    Bridge = &InBridge;
    RequestVirtualTable = InRequestVirtualTable;
    OriginalSetURL = InSetURL;

    if (RequestVirtualTable == InvalidRemoteAddress || OriginalSetURL == InvalidRemoteAddress || BackendUrl.empty())
    {
        UE_LOG_ERROR("Http", "The HTTP request virtual table, SetURL or the backend URL is missing");
        return false;
    }

    const FRemoteAddress Slot = RequestVirtualTable + SetURLVirtualIndex * sizeof(FRemoteAddress);
    const uint64 Deadline = FWindowsPlatform::GetTimeMilliseconds() + TimeoutMilliseconds;

    FRemoteAddress CurrentSetURL = Bridge->GetMemory().ReadPointer(Slot);
    while (CurrentSetURL != OriginalSetURL && FWindowsPlatform::GetTimeMilliseconds() < Deadline)
    {
        FWindowsPlatform::SleepMilliseconds(100);
        CurrentSetURL = Bridge->GetMemory().ReadPointer(Slot);
    }

    if (CurrentSetURL != OriginalSetURL)
    {
        UE_LOG_ERROR("Http", "The HTTP request virtual table holds " + FStringConv::ToHex(CurrentSetURL) + " instead of IHttpRequest::SetURL");
        return false;
    }

    std::u16string BackendText(BackendUrl.begin(), BackendUrl.end());
    while (!BackendText.empty() && BackendText.back() == u'/')
    {
        BackendText.pop_back();
    }

    FRemoteAllocation& Arena = Bridge->GetArena();
    const std::vector<uint8> Domains = BuildDomainTable();
    const FRemoteAddress DomainTable = Arena.AllocateAndWrite(Domains.data(), Domains.size());
    const FRemoteAddress Backend = Arena.AllocateAndWrite(BackendText.c_str(), (BackendText.size() + 1) * sizeof(char16_t));

    if (DomainTable == InvalidRemoteAddress || Backend == InvalidRemoteAddress)
    {
        UE_LOG_ERROR("Http", "Failed to allocate the redirect tables");
        return false;
    }

    const FRemoteAddress Override = EmitSetURLOverride(DomainTable, Backend);
    if (Override == InvalidRemoteAddress || !WriteVirtualSlot(Override))
    {
        UE_LOG_ERROR("Http", "Failed to redirect IHttpRequest::SetURL");
        return false;
    }

    bInstalled = true;
    UE_LOG_DISPLAY("Http", "Epic HTTP requests now go to " + FStringConv::ToNarrow(BackendUrl));
    return true;
}

void FHttpRedirect::Uninstall()
{
    if (bInstalled && WriteVirtualSlot(OriginalSetURL))
    {
        bInstalled = false;
    }
}
