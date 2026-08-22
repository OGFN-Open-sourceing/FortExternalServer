#include "Runtime/RemoteProcess/Public/InstructionDecoder.h"

#include <array>

namespace
{
    constexpr uint8 FlagModRm = 0x01;
    constexpr uint8 FlagImmediate8 = 0x02;
    constexpr uint8 FlagImmediate16 = 0x04;
    constexpr uint8 FlagImmediateZ = 0x08;
    constexpr uint8 FlagRelative8 = 0x10;
    constexpr uint8 FlagRelative32 = 0x20;
    constexpr uint8 FlagMoveImmediate = 0x40;
    constexpr uint8 FlagSupported = 0x80;

    using FOpcodeTable = std::array<uint8, 256>;

    FOpcodeTable BuildOneByteTable()
    {
        FOpcodeTable Table = {};

        for (uint32 ArithmeticBase = 0x00; ArithmeticBase <= 0x38; ArithmeticBase += 0x08)
        {
            Table[ArithmeticBase + 0] = FlagSupported | FlagModRm;
            Table[ArithmeticBase + 1] = FlagSupported | FlagModRm;
            Table[ArithmeticBase + 2] = FlagSupported | FlagModRm;
            Table[ArithmeticBase + 3] = FlagSupported | FlagModRm;
            Table[ArithmeticBase + 4] = FlagSupported | FlagImmediate8;
            Table[ArithmeticBase + 5] = FlagSupported | FlagImmediateZ;
        }

        for (uint32 Opcode = 0x50; Opcode <= 0x5F; ++Opcode)
        {
            Table[Opcode] = FlagSupported;
        }

        Table[0x63] = FlagSupported | FlagModRm;
        Table[0x68] = FlagSupported | FlagImmediateZ;
        Table[0x69] = FlagSupported | FlagModRm | FlagImmediateZ;
        Table[0x6A] = FlagSupported | FlagImmediate8;
        Table[0x6B] = FlagSupported | FlagModRm | FlagImmediate8;

        for (uint32 Opcode = 0x70; Opcode <= 0x7F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagRelative8;
        }

        Table[0x80] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0x81] = FlagSupported | FlagModRm | FlagImmediateZ;
        Table[0x83] = FlagSupported | FlagModRm | FlagImmediate8;

        for (uint32 Opcode = 0x84; Opcode <= 0x8B; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        Table[0x8D] = FlagSupported | FlagModRm;
        Table[0x8F] = FlagSupported | FlagModRm;

        for (uint32 Opcode = 0x90; Opcode <= 0x99; ++Opcode)
        {
            Table[Opcode] = FlagSupported;
        }

        Table[0x9C] = FlagSupported;
        Table[0x9D] = FlagSupported;
        Table[0xA8] = FlagSupported | FlagImmediate8;
        Table[0xA9] = FlagSupported | FlagImmediateZ;

        for (uint32 Opcode = 0xB0; Opcode <= 0xB7; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagImmediate8;
        }

        for (uint32 Opcode = 0xB8; Opcode <= 0xBF; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagMoveImmediate;
        }

        Table[0xC0] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xC1] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xC2] = FlagSupported | FlagImmediate16;
        Table[0xC3] = FlagSupported;
        Table[0xC6] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xC7] = FlagSupported | FlagModRm | FlagImmediateZ;
        Table[0xC9] = FlagSupported;
        Table[0xCC] = FlagSupported;
        Table[0xD0] = FlagSupported | FlagModRm;
        Table[0xD1] = FlagSupported | FlagModRm;
        Table[0xD2] = FlagSupported | FlagModRm;
        Table[0xD3] = FlagSupported | FlagModRm;
        Table[0xE8] = FlagSupported | FlagRelative32;
        Table[0xE9] = FlagSupported | FlagRelative32;
        Table[0xEB] = FlagSupported | FlagRelative8;
        Table[0xF5] = FlagSupported;
        Table[0xF6] = FlagSupported | FlagModRm;
        Table[0xF7] = FlagSupported | FlagModRm;
        Table[0xF8] = FlagSupported;
        Table[0xF9] = FlagSupported;
        Table[0xFC] = FlagSupported;
        Table[0xFD] = FlagSupported;
        Table[0xFE] = FlagSupported | FlagModRm;
        Table[0xFF] = FlagSupported | FlagModRm;

        return Table;
    }

    FOpcodeTable BuildTwoByteTable()
    {
        FOpcodeTable Table = {};

        Table[0x05] = FlagSupported;
        Table[0x0B] = FlagSupported;

        for (uint32 Opcode = 0x10; Opcode <= 0x17; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        for (uint32 Opcode = 0x18; Opcode <= 0x1F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        for (uint32 Opcode = 0x28; Opcode <= 0x2F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        for (uint32 Opcode = 0x40; Opcode <= 0x4F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        for (uint32 Opcode = 0x51; Opcode <= 0x6F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        Table[0x70] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0x71] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0x72] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0x73] = FlagSupported | FlagModRm | FlagImmediate8;

        for (uint32 Opcode = 0x74; Opcode <= 0x7F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        for (uint32 Opcode = 0x80; Opcode <= 0x8F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagRelative32;
        }

        for (uint32 Opcode = 0x90; Opcode <= 0x9F; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        Table[0xA2] = FlagSupported;
        Table[0xA3] = FlagSupported | FlagModRm;
        Table[0xA4] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xA5] = FlagSupported | FlagModRm;
        Table[0xAB] = FlagSupported | FlagModRm;
        Table[0xAC] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xAD] = FlagSupported | FlagModRm;
        Table[0xAF] = FlagSupported | FlagModRm;
        Table[0xB0] = FlagSupported | FlagModRm;
        Table[0xB1] = FlagSupported | FlagModRm;
        Table[0xB3] = FlagSupported | FlagModRm;
        Table[0xB6] = FlagSupported | FlagModRm;
        Table[0xB7] = FlagSupported | FlagModRm;
        Table[0xBA] = FlagSupported | FlagModRm | FlagImmediate8;
        Table[0xBB] = FlagSupported | FlagModRm;
        Table[0xBC] = FlagSupported | FlagModRm;
        Table[0xBD] = FlagSupported | FlagModRm;
        Table[0xBE] = FlagSupported | FlagModRm;
        Table[0xBF] = FlagSupported | FlagModRm;
        Table[0xC0] = FlagSupported | FlagModRm;
        Table[0xC1] = FlagSupported | FlagModRm;
        Table[0xC6] = FlagSupported | FlagModRm | FlagImmediate8;

        for (uint32 Opcode = 0xD0; Opcode <= 0xFE; ++Opcode)
        {
            Table[Opcode] = FlagSupported | FlagModRm;
        }

        return Table;
    }

    const FOpcodeTable& GetOneByteTable()
    {
        static const FOpcodeTable Table = BuildOneByteTable();
        return Table;
    }

    const FOpcodeTable& GetTwoByteTable()
    {
        static const FOpcodeTable Table = BuildTwoByteTable();
        return Table;
    }

    bool IsLegacyPrefix(uint8 Byte)
    {
        switch (Byte)
        {
        case 0x26:
        case 0x2E:
        case 0x36:
        case 0x3E:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67:
        case 0xF0:
        case 0xF2:
        case 0xF3:
            return true;
        default:
            return false;
        }
    }
}

FDecodedInstruction FInstructionDecoder::Decode(const uint8* Code, size_t AvailableBytes)
{
    FDecodedInstruction Result;

    if (Code == nullptr || AvailableBytes == 0)
    {
        return Result;
    }

    size_t Cursor = 0;
    bool bOperandSizeOverride = false;
    bool bAddressSizeOverride = false;
    uint8 RexByte = 0;

    while (Cursor < AvailableBytes && IsLegacyPrefix(Code[Cursor]))
    {
        if (Code[Cursor] == 0x66)
        {
            bOperandSizeOverride = true;
        }
        else if (Code[Cursor] == 0x67)
        {
            bAddressSizeOverride = true;
        }

        ++Cursor;
    }

    if (Cursor < AvailableBytes && Code[Cursor] >= 0x40 && Code[Cursor] <= 0x4F)
    {
        RexByte = Code[Cursor];
        ++Cursor;
    }

    if (Cursor >= AvailableBytes)
    {
        return Result;
    }

    uint8 Flags = 0;
    bool bTwoByteOpcode = false;

    if (Code[Cursor] == 0x0F)
    {
        ++Cursor;
        if (Cursor >= AvailableBytes)
        {
            return Result;
        }

        bTwoByteOpcode = true;
        Flags = GetTwoByteTable()[Code[Cursor]];
    }
    else
    {
        Flags = GetOneByteTable()[Code[Cursor]];
        Result.bIsReturn = Code[Cursor] == 0xC3 || Code[Cursor] == 0xC2;
    }

    if ((Flags & FlagSupported) == 0)
    {
        return Result;
    }

    const uint8 Opcode = Code[Cursor];
    ++Cursor;

    if ((Flags & FlagModRm) != 0)
    {
        if (Cursor >= AvailableBytes)
        {
            return Result;
        }

        const uint8 ModRm = Code[Cursor];
        ++Cursor;

        const uint8 Mode = static_cast<uint8>(ModRm >> 6);
        const uint8 RegisterField = static_cast<uint8>((ModRm >> 3) & 0x7);
        const uint8 MemoryField = static_cast<uint8>(ModRm & 0x7);

        if (Mode != 0x3 && MemoryField == 0x4)
        {
            if (Cursor >= AvailableBytes)
            {
                return Result;
            }

            ++Cursor;
        }

        if (Mode == 0x0 && MemoryField == 0x5)
        {
            Result.bHasRipRelativeOperand = !bAddressSizeOverride;
            Result.RipDisplacementOffset = static_cast<uint32>(Cursor);
            Cursor += sizeof(int32);
        }
        else if (Mode == 0x1)
        {
            Cursor += sizeof(int8);
        }
        else if (Mode == 0x2)
        {
            Cursor += sizeof(int32);
        }

        if (!bTwoByteOpcode && (Opcode == 0xF6 || Opcode == 0xF7) && RegisterField <= 1)
        {
            Flags |= Opcode == 0xF6 ? FlagImmediate8 : FlagImmediateZ;
        }
    }

    if ((Flags & FlagMoveImmediate) != 0)
    {
        Cursor += (RexByte & 0x8) != 0 ? sizeof(uint64) : (bOperandSizeOverride ? sizeof(uint16) : sizeof(uint32));
    }

    if ((Flags & FlagImmediate8) != 0)
    {
        Cursor += sizeof(uint8);
    }

    if ((Flags & FlagImmediate16) != 0)
    {
        Cursor += sizeof(uint16);
    }

    if ((Flags & FlagImmediateZ) != 0)
    {
        Cursor += bOperandSizeOverride ? sizeof(uint16) : sizeof(uint32);
    }

    if ((Flags & FlagRelative8) != 0)
    {
        Result.bHasRelativeBranch = true;
        Result.RelativeOperandOffset = static_cast<uint32>(Cursor);
        Result.RelativeOperandSize = sizeof(int8);
        Cursor += sizeof(int8);
    }

    if ((Flags & FlagRelative32) != 0)
    {
        Result.bHasRelativeBranch = true;
        Result.RelativeOperandOffset = static_cast<uint32>(Cursor);
        Result.RelativeOperandSize = sizeof(int32);
        Cursor += sizeof(int32);
    }

    if (Cursor > AvailableBytes)
    {
        return Result;
    }

    Result.Length = static_cast<uint32>(Cursor);
    Result.bValid = Result.Length > 0;
    return Result;
}

uint32 FInstructionDecoder::MeasurePrologue(const uint8* Code, size_t AvailableBytes, uint32 MinimumLength)
{
    uint32 Consumed = 0;

    while (Consumed < MinimumLength)
    {
        const FDecodedInstruction Instruction = Decode(Code + Consumed, AvailableBytes - Consumed);
        if (!Instruction.bValid)
        {
            return 0;
        }

        Consumed += Instruction.Length;

        if (Instruction.bIsReturn)
        {
            break;
        }
    }

    return Consumed >= MinimumLength ? Consumed : 0;
}
