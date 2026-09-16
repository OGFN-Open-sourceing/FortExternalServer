param(
    [string]$BinaryPath = "D:\FortniteBuilds\ch1\fn-3.6\3.6-CL-4019403\Fortnite\dump\FortniteClient-Win64-Shipping_dump.exe"
)

Write-Host "Reading binary: $BinaryPath"
$bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
Write-Host "Binary size: $($bytes.Length) bytes"

$baseAddress = 0x140000000  # Typical UE4 shipping base

function Find-WideString {
    param([string]$Text)
    $chars = [System.Text.Encoding]::Unicode.GetBytes($Text + "`0")
    for ($i = 0; $i -le $bytes.Length - $chars.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $chars.Length; $j++) {
            if ($bytes[$i + $j] -ne $chars[$j]) { $match = $false; break }
        }
        if ($match) { return $i }
    }
    return -1
}

function Find-ReferencesTo {
    param([long]$TargetOffset)
    $results = @()
    $targetAddr = [uint64]($baseAddress + $targetAddr_int64)
    for ($i = 0; $i -le $bytes.Length - 4; $i++) {
        $disp = [BitConverter]::ToInt32($bytes, $i)
        $refAddr = [uint64]($baseAddress + $i + 4 + [int64]$disp)
        if ($refAddr -eq $targetAddr) {
            $results += [uint64]($baseAddress + $i)
            if ($results.Count -ge 10) { break }
        }
    }
    return $results
}

function Find-InstructionBefore {
    param([long]$Offset, [int]$Range)
    $start = [Math]::Max(0, $Offset - $Range)
    for ($i = $Offset; $i -ge $start; $i--) {
        # Look for '48 8D' (lea) followed by 'E8' (call) or 'E9' (jmp) 7 bytes later
        if ($i + 8 -lt $bytes.Length) {
            if ($bytes[$i] -eq 0x48 -and $bytes[$i+1] -eq 0x8D -and ($bytes[$i+7] -eq 0xE8 -or $bytes[$i+7] -eq 0xE9)) {
                $disp = [BitConverter]::ToInt32($bytes, $i + 8)
                $target = $baseAddress + $i + 13 + [int64]$disp
                Write-Host "  Found lea+call/jmp at RVA 0x$($i.ToString('X8')) -> target 0x$($target.ToString('X16'))"
                return @{RVA=[uint64]$i; Target=[uint64]$target}
            }
        }
    }
    return $null
}

# ===== Find NameConstructor =====
Write-Host "`n=== Finding NameConstructor ==="
$nameStrOffset = Find-WideString "ClientIgnoreLookInput"
if ($nameStrOffset -ge 0) {
    $nameStrRva = [uint64]$nameStrOffset
    Write-Host "Found 'ClientIgnoreLookInput' at offset $nameStrOffset (RVA 0x$($nameStrRva.ToString('X8')))"
    
    # Scan for references
    $refs = @()
    $targetAddr_int64 = [int64]$nameStrRva
    $targetAddr = [uint64]($baseAddress + $nameStrOffset)
    Write-Host "Target address: 0x$($targetAddr.ToString('X16'))"
    
    # Scan .text section only (0x1000 to 0x305E000)
    $textStart = 0x1000
    $textEnd = [Math]::Min($bytes.Length, 0x3060000)
    Write-Host "Scanning .text (0x$($textStart.ToString('X')) to 0x$($textEnd.ToString('X')))..."
    
    $found = $false
    for ($i = $textStart; $i -le $textEnd - 4; $i++) {
        $disp = [BitConverter]::ToInt32($bytes, $i)
        $checkAddr = [uint64]($baseAddress + $i + 4 + [int64]$disp)
        if ($checkAddr -eq $targetAddr) {
            Write-Host "Found reference at RVA 0x$($i.ToString('X8')) (file offset 0x$($i.ToString('X8')))"
            $refs += $i
            $found = $true
            if ($refs.Count -ge 5) { break }
        }
    }
    
    if ($found) {
        # For each reference, look backward for lea+call pattern
        foreach ($ref in $refs) {
            $result = Find-InstructionBefore -Offset $ref -Range 0x100
            if ($null -ne $result) {
                Write-Host "  NameConstructor likely at RVA 0x$($result.Target.ToString('X8'))"
            }
        }
    } else {
        Write-Host "No code references found to 'ClientIgnoreLookInput'!"
        Write-Host "Trying broader scan (all sections)..."
        for ($i = 0; $i -le $bytes.Length - 4; $i++) {
            $disp = [BitConverter]::ToInt32($bytes, $i)
            $checkAddr = [uint64]($baseAddress + $i + 4 + [int64]$disp)
            if ($checkAddr -eq $targetAddr) {
                Write-Host "Found reference at offset 0x$($i.ToString('X8'))"
                $refs += $i
                if ($refs.Count -ge 5) { break }
            }
        }
        if ($refs.Count -eq 0) {
            Write-Host "STILL no references found. String may not be code-referenced in this build."
        }
    }
} else {
    Write-Host "String 'ClientIgnoreLookInput' not found in binary!"
}

# ===== Find ProcessEvent anchor =====
Write-Host "`n=== Finding ProcessEvent anchor ==="
$peStrOffset = Find-WideString "AccessNoneNoContext"
if ($peStrOffset -ge 0) {
    $peStrRva = [uint64]$peStrOffset
    Write-Host "Found 'AccessNoneNoContext' at RVA 0x$($peStrRva.ToString('X8'))"
} else {
    Write-Host "String 'AccessNoneNoContext' not found!"
}

# ===== Find StaticLoadObject anchor =====
Write-Host "`n=== Finding StaticLoadObject anchor ==="
$sloStrOffset = Find-WideString "STAT_LoadObject"
if ($sloStrOffset -ge 0) {
    $sloStrRva = [uint64]$sloStrOffset
    Write-Host "Found 'STAT_LoadObject' at RVA 0x$($sloStrRva.ToString('X8'))"
} else {
    Write-Host "String 'STAT_LoadObject' not found!"
}

# ===== Find SpawnActor anchor =====
Write-Host "`n=== Finding SpawnActor anchor ==="
$saStrOffset = Find-WideString "SpawnActor failed because no class was specified"
if ($saStrOffset -ge 0) {
    $saStrRva = [uint64]$saStrOffset
    Write-Host "Found 'SpawnActor failed...' at RVA 0x$($saStrRva.ToString('X8'))"
} else {
    Write-Host "String 'SpawnActor failed...' not found!"
}

# ===== Known offsets verification =====
Write-Host "`n=== Known SDK Offsets (verification) ==="
Write-Host "GObjects RVA: 0x04BA7768"
Write-Host "GNames RVA: 0x04B9E370"
Write-Host "ProcessEvent RVA: 0x014C04C0"
Write-Host "FMemory::Realloc RVA: 0x01280550"
Write-Host "FName::AppendString (NameToString) RVA: 0x013298E0"

# ===== Try to find StaticFindObject by pattern =====
Write-Host "`n=== Searching for StaticFindObject pattern ==="
# StaticFindObject typically starts with '48 89 5C 24' and references FindObject-related strings
# Let's look for common UE4 function prologues near where we expect it
# Search for '48 89 5C 24 ? 48 89 74 24 ? 55 57 41 54 41 56 41 57 48 8B EC 48 83 EC 60 80 3D'
# which is the original signature
$sigPattern = @(0x48, 0x89, 0x5C, 0x24, -1, 0x48, 0x89, 0x74, 0x24, -1, 0x55, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41, 0x57, 0x48, 0x8B, 0xEC, 0x48, 0x83, 0xEC, 0x60, 0x80, 0x3D)
$textStart2 = 0x1000
$textEnd2 = [Math]::Min($bytes.Length, 0x3060000)
$foundSig = $false
for ($i = $textStart2; $i -le $textEnd2 - $sigPattern.Length; $i++) {
    $match = $true
    for ($j = 0; $j -lt $sigPattern.Length; $j++) {
        if ($sigPattern[$j] -ge 0 -and $bytes[$i + $j] -ne $sigPattern[$j]) { $match = $false; break }
    }
    if ($match) {
        Write-Host "Found StaticFindObject-like pattern at RVA 0x$($i.ToString('X8'))"
        $foundSig = $true
    }
}
if (-not $foundSig) {
    Write-Host "Primary StaticFindObject pattern not found, trying alternate..."
    $altSig = @(0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08, 0x49, 0x89, 0x6B, 0x18, 0x49, 0x89, 0x73, 0x20, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x80, 0x3D)
    for ($i = $textStart2; $i -le $textEnd2 - $altSig.Length; $i++) {
        $match = $true
        for ($j = 0; $j -lt $altSig.Length; $j++) {
            if ($altSig[$j] -ge 0 -and $bytes[$i + $j] -ne $altSig[$j]) { $match = $false; break }
        }
        if ($match) {
            Write-Host "Found StaticFindObject alternate pattern at RVA 0x$($i.ToString('X8'))"
            $foundSig = $true
        }
    }
}

# ===== Search for name-related patterns near AppendString =====
Write-Host "`n=== Searching near FName::AppendString (0x013298E0) for NameConstructor ==="
$nameToStringRva = 0x013298E0
$searchStart = [Math]::Max(0x1000, $nameToStringRva - 0x20000)
$searchEnd = [Math]::Min($textEnd2, $nameToStringRva + 0x20000)
Write-Host "Searching range: 0x$($searchStart.ToString('X8')) to 0x$($searchEnd.ToString('X8'))"

# Look for function prologues in this range
for ($i = $searchStart; $i -le $searchEnd - 4; $i++) {
    if ($bytes[$i] -eq 0x48 -and $bytes[$i+1] -eq 0x8D) {
        # Check if followed by call/jmp within a few bytes
        for ($k = 3; $k -le 12; $k++) {
            if ($i + $k + 5 -lt $bytes.Length -and ($bytes[$i+$k] -eq 0xE8 -or $bytes[$i+$k] -eq 0xE9)) {
                # It's a lea followed by call/jmp - print it
                Write-Host "  Potential pattern at RVA 0x$($i.ToString('X8')): lea ... call/jmp at +$k"
            }
        }
    }
}

Write-Host "`n=== Summary ==="
Write-Host "If patterns not found, offsets need to come from IDA/Ghidra analysis of the binary."
Write-Host "You can also try loading the binary in IDA and searching for xrefs to the anchor strings."
