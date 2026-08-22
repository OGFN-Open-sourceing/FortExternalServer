#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

enum class EX64Register : uint8
{
    Rax = 0,
    Rcx = 1,
    Rdx = 2,
    Rbx = 3,
    Rsp = 4,
    Rbp = 5,
    Rsi = 6,
    Rdi = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

enum class EX64CallingConvention : uint8
{
    MicrosoftX64,
    SystemV
};

struct FX64AbiLayout
{
    EX64CallingConvention Convention = EX64CallingConvention::MicrosoftX64;
    EX64Register IntegerArgumentRegisters[6] = {};
    int32 IntegerArgumentCount = 4;
    int32 ShadowSpaceBytes = 32;

    static FX64AbiLayout GetHostAbi();

    static FX64AbiLayout GetMicrosoftX64();

    static FX64AbiLayout GetSystemV();
};

enum class EX64Condition : uint8
{
    Zero = 0x84,
    NotZero = 0x85,
    Below = 0x82,
    AboveOrEqual = 0x83,
    Equal = 0x84,
    NotEqual = 0x85
};

class FX64Emitter
{
public:
    using FLabelId = size_t;

    void Reset();

    size_t GetSize() const;

    const std::vector<uint8>& GetBytes() const;

    FLabelId CreateLabel();

    void BindLabel(FLabelId Label);

    void EmitBytes(std::initializer_list<uint8> Bytes);

    void EmitRaw(const uint8* Bytes, size_t Count);

    void EmitU8(uint8 Value);

    void EmitU32(uint32 Value);

    void EmitU64(uint64 Value);

    void PushRegister(EX64Register Register);

    void PopRegister(EX64Register Register);

    void MoveRegisterImmediate(EX64Register Destination, uint64 Value);

    void MoveRegisterRegister(EX64Register Destination, EX64Register Source);

    void LoadRegisterFromMemory(EX64Register Destination, EX64Register Base, int32 Displacement);

    void StoreRegisterToMemory(EX64Register Base, int32 Displacement, EX64Register Source);

    void StoreImmediateToMemory(EX64Register Base, int32 Displacement, uint32 Value);

    void AddRegisterImmediate(EX64Register Destination, int32 Value);

    void SubRegisterImmediate(EX64Register Destination, int32 Value);

    void CompareMemoryImmediate(EX64Register Base, int32 Displacement, uint32 Value);

    void TestRegisterRegister(EX64Register Left, EX64Register Right);

    void CompareRegisterRegister(EX64Register Left, EX64Register Right);

    void SaveXmmToMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement);

    void LoadXmmFromMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement);

    void LoadXmmQwordFromMemory(uint8 XmmIndex, EX64Register Base, int32 Displacement);

    void StoreXmmQwordToMemory(EX64Register Base, int32 Displacement, uint8 XmmIndex);

    void XorRegisterRegister(EX64Register Destination, EX64Register Source);

    void IncrementMemory(EX64Register Base, int32 Displacement);

    void CallRegister(EX64Register Register);

    void JumpToLabel(FLabelId Label);

    void JumpConditionalToLabel(EX64Condition Condition, FLabelId Label);

    void JumpAbsolute(uint64 TargetAddress);

    void EmitPause();

    void EmitReturn();

    void EmitBreakpoint();

    bool Finalize();

    static void WriteRelativeJump(uint8* Destination, int32 Displacement);

    static size_t GetAbsoluteJumpSize();

    static void WriteAbsoluteJump(std::vector<uint8>& Destination, uint64 TargetAddress);

private:
    struct FLabelFixup
    {
        FLabelId Label = 0;
        size_t PatchOffset = 0;
        size_t InstructionEnd = 0;
    };

    void EmitRexForRegisterMemory(EX64Register Register, EX64Register Base);

    void EmitModRmWithDisplacement(EX64Register Register, EX64Register Base, int32 Displacement);

    std::vector<uint8> Bytes;
    std::vector<size_t> Labels;
    std::vector<FLabelFixup> Fixups;
};
