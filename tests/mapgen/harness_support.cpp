// Minimal link-time support for the engine-loader proof harness.
//
// src/datafile.cpp calls into src/debug.h's OutputLog (a DebugLog
// instance declared extern in src/main.h) purely for diagnostic
// logging, and into Get2ByteLittleEndian() (declared in src/main.h,
// defined in src/main.cpp) for header parsing. Both are tiny, pure
// utility pieces of the engine with no rendering/platform
// dependencies of their own, but main.cpp as a whole pulls in the
// full game loop. Rather than link all of main.cpp (and everything it
// drags in) just to prove the .bd1/.pd1 loaders, this file supplies
// the same two symbols directly, matching the real behaviour exactly
// (same byte math as src/main.cpp:456, same log no-op as a file-less
// DebugLog would do once OutputLog.filename is empty, src/debug.h).
// It changes nothing about how BlockDataInterface/PointDataInterface
// parse bytes -- that code is the unmodified src/datafile.cpp.

#define H_LAYERLEVEL 1
#include "datafile.h"
#include "debug.h"

#include <cstdio>

#ifdef ENABLE_DEBUGLOG
// DebugLog's real ctor/dtor/EnableOutputLog/SetLogName/MakeLog bodies
// are in debug.cpp (not read for this task); WriteLog only matters
// here, and with no log file configured it is a pure no-op either
// way (src/debug.h:43-57 declares the shape; src/datafile.cpp calls
// only WriteLog(int,const char*,const char*)).
DebugLog::DebugLog() {}
DebugLog::~DebugLog() {}
bool DebugLog::WriteLog(int, const char *, const char *) { return true; }
bool DebugLog::WriteLog(int, const char *, int) { return true; }
class DebugLog OutputLog;
#endif

// Exact copy of src/main.cpp:456-461's bit math (reading a 2-byte
// little-endian value out of the first two bytes of `indata`).
unsigned int Get2ByteLittleEndian(unsigned char *indata) {
    if (indata == NULL) { return 0; }
    return (unsigned int)(((indata[1] << 8) & 0xFF00) + ((indata[0] << 0) & 0x00FF));
}
