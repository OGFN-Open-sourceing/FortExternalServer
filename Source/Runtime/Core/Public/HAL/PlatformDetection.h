#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define FORT_PLATFORM_WINDOWS 1
#else
#define FORT_PLATFORM_WINDOWS 0
#endif

#if defined(__APPLE__)
#define FORT_PLATFORM_MAC 1
#else
#define FORT_PLATFORM_MAC 0
#endif

#if !FORT_PLATFORM_WINDOWS && !FORT_PLATFORM_MAC
#error FortExternalServer supports Windows and macOS hosts only.
#endif

#if FORT_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <objbase.h>

#endif
