#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

struct FObjectLayout
{
    int32 UObject_VirtualTable = 0x00;
    int32 UObject_ObjectFlags = 0x08;
    int32 UObject_InternalIndex = 0x0C;
    int32 UObject_Class = 0x10;
    int32 UObject_Name = 0x18;
    int32 UObject_Outer = 0x20;

    int32 UField_Next = 0x28;

    int32 UStruct_SuperStruct = 0x30;
    int32 UStruct_Children = 0x38;
    int32 UStruct_ChildProperties = InvalidIndex;
    int32 UStruct_PropertiesSize = 0x40;

    int32 UFunction_FunctionFlags = 0x88;
    int32 UFunction_ExecFunction = 0xB0;

    int32 UProperty_ArrayDim = 0x30;
    int32 UProperty_ElementSize = 0x34;
    int32 UProperty_PropertyFlags = 0x38;
    int32 UProperty_Offset_Internal = 0x44;
    int32 UBoolProperty_FieldMask = 0x73;

    int32 FField_Next = 0x20;
    int32 FField_Name = 0x28;

    int32 ObjectItemStride = 0x18;
    int32 ObjectsPerChunk = 64 * 1024 / 0x18;
    bool bChunkedObjectArray = false;

    bool UsesFieldProperties() const
    {
        return UStruct_ChildProperties >= 0;
    }

    int32 GetPropertyListOffset() const
    {
        return UsesFieldProperties() ? UStruct_ChildProperties : UStruct_Children;
    }

    int32 GetPropertyNameOffset() const
    {
        return UsesFieldProperties() ? FField_Name : UObject_Name;
    }

    int32 GetPropertyNextOffset() const
    {
        return UsesFieldProperties() ? FField_Next : UField_Next;
    }

    int32 GetObjectCountOffset() const
    {
        return bChunkedObjectArray ? 0x14 : 0x0C;
    }
};

namespace FUnrealLayout
{
    inline constexpr int32 FUObjectItem_Object = 0x00;

    inline constexpr int32 TUObjectArray_Objects = 0x00;

    inline constexpr int32 FScriptArray_Data = 0x00;
    inline constexpr int32 FScriptArray_ArrayNum = 0x08;
    inline constexpr int32 FScriptArray_ArrayMax = 0x0C;
    inline constexpr int32 FScriptArray_Size = 0x10;

    inline constexpr uint32 FunctionFlag_Native = 0x00000400;
    inline constexpr uint32 FunctionFlag_Net = 0x00000040;

    inline constexpr uint64 PropertyFlag_ReturnParm = 0x0000000000000400ull;
    inline constexpr uint64 PropertyFlag_OutParm = 0x0000000000000100ull;
    inline constexpr uint64 PropertyFlag_Parm = 0x0000000000000080ull;

    inline constexpr int32 FindNameMode = 1;
    inline constexpr int32 AddNameMode = 0;

    inline constexpr int32 AnyPackageSentinel = -1;
}
