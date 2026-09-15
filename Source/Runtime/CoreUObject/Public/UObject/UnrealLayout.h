#pragma once

#include "Runtime/Core/Public/CoreMinimal.h"

namespace FUnrealLayout
{
    inline constexpr int32 UObject_VirtualTable = 0x00;
    inline constexpr int32 UObject_ObjectFlags = 0x08;
    inline constexpr int32 UObject_InternalIndex = 0x0C;
    inline constexpr int32 UObject_Class = 0x10;
    inline constexpr int32 UObject_Name = 0x18;
    inline constexpr int32 UObject_Outer = 0x20;
    inline constexpr int32 UObject_Size = 0x28;

    inline constexpr int32 UField_Next = 0x28;

    inline constexpr int32 UStruct_SuperStruct = 0x30;
    inline constexpr int32 UStruct_Children = 0x38;
    inline constexpr int32 UStruct_PropertiesSize = 0x40;
    inline constexpr int32 UStruct_MinAlignment = 0x44;
    inline constexpr int32 UStruct_Size = 0x88;

    inline constexpr int32 UFunction_FunctionFlags = 0x88;
    inline constexpr int32 UFunction_NumParms = 0x8E;
    inline constexpr int32 UFunction_ParmsSize = 0x90;
    inline constexpr int32 UFunction_ReturnValueOffset = 0x92;
    inline constexpr int32 UFunction_ExecFunction = 0xB0;

    inline constexpr int32 UProperty_ArrayDim = 0x30;
    inline constexpr int32 UProperty_ElementSize = 0x34;
    inline constexpr int32 UProperty_PropertyFlags = 0x38;
    inline constexpr int32 UProperty_Offset_Internal = 0x44;

    inline constexpr int32 UBoolProperty_FieldSize = 0x70;
    inline constexpr int32 UBoolProperty_ByteOffset = 0x71;
    inline constexpr int32 UBoolProperty_ByteMask = 0x72;
    inline constexpr int32 UBoolProperty_FieldMask = 0x73;

    inline constexpr int32 UObjectPropertyBase_PropertyClass = 0x70;
    inline constexpr int32 UEnumProperty_UnderlyingProperty = 0x78;
    inline constexpr int32 UStructProperty_Struct = 0x70;
    inline constexpr int32 UArrayProperty_Inner = 0x70;

    inline constexpr int32 FUObjectItem_Object = 0x00;
    inline constexpr int32 FUObjectItem_Flags = 0x08;
    inline constexpr int32 FUObjectItem_ClusterIndex = 0x0C;
    inline constexpr int32 FUObjectItem_SerialNumber = 0x10;
    inline constexpr int32 FUObjectItem_Stride = 0x18;

    inline constexpr int32 TUObjectArray_Objects = 0x00;
    inline constexpr int32 TUObjectArray_MaxElements = 0x08;
    inline constexpr int32 TUObjectArray_NumElements = 0x0C;

    inline constexpr int32 FChunkedObjectArray_Objects = 0x00;
    inline constexpr int32 FChunkedObjectArray_PreAllocated = 0x08;
    inline constexpr int32 FChunkedObjectArray_MaxElements = 0x10;
    inline constexpr int32 FChunkedObjectArray_NumElements = 0x14;
    inline constexpr int32 FChunkedObjectArray_MaxChunks = 0x18;
    inline constexpr int32 FChunkedObjectArray_NumChunks = 0x1C;
    inline constexpr int32 FChunkedObjectArray_ChunkBytes = 64 * 1024;

    inline constexpr int32 FScriptArray_Data = 0x00;
    inline constexpr int32 FScriptArray_ArrayNum = 0x08;
    inline constexpr int32 FScriptArray_ArrayMax = 0x0C;
    inline constexpr int32 FScriptArray_Size = 0x10;

    inline constexpr uint32 FunctionFlag_Native = 0x00000400;
    inline constexpr uint32 FunctionFlag_Net = 0x00000040;
    inline constexpr uint32 FunctionFlag_NetRequest = 0x00080000;
    inline constexpr uint32 FunctionFlag_NetResponse = 0x00200000;

    inline constexpr uint64 PropertyFlag_ReturnParm = 0x0000000000000400ull;
    inline constexpr uint64 PropertyFlag_OutParm = 0x0000000000000100ull;
    inline constexpr uint64 PropertyFlag_Parm = 0x0000000000000080ull;

    inline constexpr int32 FindNameMode = 1;
    inline constexpr int32 AddNameMode = 0;

    inline constexpr int32 AnyPackageSentinel = -1;
}
