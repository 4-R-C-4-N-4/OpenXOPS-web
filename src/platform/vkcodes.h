//! @file vkcodes.h
//! @brief [OpenXOPS-web] Windows virtual-key codes (values from winuser.h) for
//!        non-Windows targets. Key ids in OpenXOPS stay in VK space, so
//!        OriginalkeycodeToDinputdef() is reused unchanged; a backend only has
//!        to fill keys[256] indexed by these codes.

// Copyright (c) 2026, OpenXOPS-web contributors. BSD-3-Clause, same terms as OpenXOPS.

#ifndef OPENXOPS_PLATFORM_VKCODES_H
#define OPENXOPS_PLATFORM_VKCODES_H

#ifndef _WIN32

#define VK_LBUTTON    0x01
#define VK_RBUTTON    0x02
#define VK_BACK       0x08
#define VK_TAB        0x09
#define VK_RETURN     0x0D
#define VK_SHIFT      0x10
#define VK_CONTROL    0x11
#define VK_ESCAPE     0x1B
#define VK_SPACE      0x20
#define VK_HOME       0x24
#define VK_LEFT       0x25
#define VK_UP         0x26
#define VK_RIGHT      0x27
#define VK_DOWN       0x28
// '0'-'9' = 0x30-0x39, 'A'-'Z' = 0x41-0x5A (same as ASCII)
#define VK_NUMPAD0    0x60
#define VK_NUMPAD1    0x61
#define VK_NUMPAD2    0x62
#define VK_NUMPAD3    0x63
#define VK_NUMPAD4    0x64
#define VK_NUMPAD5    0x65
#define VK_NUMPAD6    0x66
#define VK_NUMPAD7    0x67
#define VK_NUMPAD8    0x68
#define VK_NUMPAD9    0x69
#define VK_MULTIPLY   0x6A
#define VK_ADD        0x6B
#define VK_SUBTRACT   0x6D
#define VK_DECIMAL    0x6E
#define VK_DIVIDE     0x6F
#define VK_F1         0x70
#define VK_F2         0x71
#define VK_F3         0x72
#define VK_F4         0x73
#define VK_F5         0x74
#define VK_F6         0x75
#define VK_F7         0x76
#define VK_F8         0x77
#define VK_F9         0x78
#define VK_F10        0x79
#define VK_F11        0x7A
#define VK_F12        0x7B
#define VK_LSHIFT     0xA0
#define VK_RSHIFT     0xA1
#define VK_LCONTROL   0xA2
#define VK_RCONTROL   0xA3
#define VK_OEM_1      0xBA
#define VK_OEM_PLUS   0xBB
#define VK_OEM_COMMA  0xBC
#define VK_OEM_MINUS  0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2      0xBF
#define VK_OEM_3      0xC0
#define VK_OEM_4      0xDB
#define VK_OEM_5      0xDC
#define VK_OEM_6      0xDD
#define VK_OEM_7      0xDE
#define VK_OEM_102    0xE2

#endif	// !_WIN32

#endif	// OPENXOPS_PLATFORM_VKCODES_H
