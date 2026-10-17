#pragma once
#include <cstdio>
#include <vector>
#include <AL/al.h>
namespace openal_pcm {
struct Pcm { ALenum format; ALsizei rate; std::vector<unsigned char> bytes; };
// Compatibility decoder: stereo intentionally selects the left channel.
bool decode_wav(FILE* fp, Pcm& out);
int distance_volume(int volume, float distance);
}
