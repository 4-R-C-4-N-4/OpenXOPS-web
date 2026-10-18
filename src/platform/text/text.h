#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// Decode one Windows-31J/CP932 character. On failure, emits U+FFFD and
// consumes one byte (unless input is empty, or a truncated/invalid lead byte
// consumes exactly the bytes available so callers never read past the end).
// Returns true only if the decoded codepoint is valid.
bool cp932_decode(const unsigned char* input, size_t length, uint16_t* codepoint, size_t* consumed);

// Measure the pixel size a string would take if rendered by text_bitmap,
// without allocating or rendering any pixels. Half-width glyphs are px/2
// wide, full-width glyphs are px wide; lines are separated by LF. Returns
// false for invalid arguments (null bytes/width/height pointers, bad px).
bool text_measure(const char* bytes, int px, int* width, int* height);

// Render CP932 bytes to tightly packed RGBA8, transparent background, opaque
// monochrome glyphs. px selects 12 or 16 pixel source font, other sizes scale
// nearest-neighbor. Width is measured in pixels; lines separated by LF.
// Invalid byte sequences render U+FFFD. Returns false for invalid arguments.
bool text_bitmap(const char* bytes, int px, std::vector<unsigned char>& rgba, int* width, int* height);
