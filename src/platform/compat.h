//! @file compat.h
//! @brief [OpenXOPS-web] Portable replacements for the few Windows/MSVC facilities
//!        that the game logic uses. Included from main.h on non-_WIN32 targets only.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#ifndef OPENXOPS_PLATFORM_COMPAT_H
#define OPENXOPS_PLATFORM_COMPAT_H

#ifdef _WIN32
 #error "platform/compat.h is for non-Windows targets only"
#endif

#include <unistd.h>		// chdir(), getcwd()  (replaces <direct.h>)
#include <strings.h>	// strcasecmp(), strncasecmp()
#include <limits.h>
#include <stdint.h>

//! MAX_PATH / _MAX_PATH  (main.h also defines these if missing; kept here so
//! platform code that includes only this header gets them too)
#ifndef _MAX_PATH
 #define _MAX_PATH 260
#endif
#ifndef MAX_PATH
 #define MAX_PATH 260
#endif

//! MSVC case-insensitive string compare
#ifndef _stricmp
 #define _stricmp strcasecmp
#endif
#ifndef _strnicmp
 #define _strnicmp strncasecmp
#endif

//! Unused-parameter marker normally provided by <windows.h> (winnt.h).
//! main.h has an equivalent block, but it sits behind `#ifndef MAIN_H` inside
//! main.h itself, so it never fires.
#ifndef UNREFERENCED_PARAMETER
 #define UNREFERENCED_PARAMETER(P) ((void)(P))
#endif

//! The binary loaders read 4-byte IEEE floats straight into float arrays
//! (datafile.cpp: fread(bdata_main, 4, ...), fread(pdata_mainf, 4, ...)).
static_assert(sizeof(float) == 4, "OpenXOPS file formats need 32-bit float");
static_assert(sizeof(int) == 4, "OpenXOPS assumes 32-bit int (as on MSVC)");
static_assert(sizeof(short) == 2, "OpenXOPS assumes 16-bit short");

#endif	// OPENXOPS_PLATFORM_COMPAT_H
