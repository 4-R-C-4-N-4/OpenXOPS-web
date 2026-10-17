//! @file sound-openal.cpp
//! @brief OpenAL SoundControl backend with bounded PCM WAV decoding.

#include "../../sound.h"
#include "../vfs.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <climits>
#include <vector>

#include "pcm.h"

namespace openal_pcm {
static uint16_t u16(const unsigned char* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
static uint32_t u32(const unsigned char* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
bool decode_wav(FILE* fp, Pcm& out)
{
    if (!fp || fseek(fp, 0, SEEK_END) != 0) return false;
    long end = ftell(fp);
    if (end < 12 || end > 64 * 1024 * 1024 || fseek(fp, 0, SEEK_SET) != 0) return false;
    std::vector<unsigned char> b(static_cast<size_t>(end));
    if (fread(b.data(), 1, b.size(), fp) != b.size() || memcmp(b.data(), "RIFF", 4) || memcmp(b.data()+8, "WAVE", 4)) return false;
    uint64_t riff_end = std::min<uint64_t>(b.size(), uint64_t(u32(&b[4])) + 8);
    uint16_t channels=0, bits=0, encoding=0, align=0; uint32_t rate=0;
    const unsigned char* data=nullptr; size_t data_size=0; bool have_fmt=false;
    for (uint64_t pos=12; pos+8<=riff_end;) {
        uint32_t size=u32(&b[pos+4]); uint64_t payload=pos+8, next=payload+size+(size&1);
        if (next>riff_end || next<payload) return false;
        if (!memcmp(&b[pos], "fmt ", 4)) {
            if(size<16) return false;
            encoding=u16(&b[payload]); channels=u16(&b[payload+2]); rate=u32(&b[payload+4]); align=u16(&b[payload+12]); bits=u16(&b[payload+14]);
            if(encoding==0xfffe) { if(size<40 || u16(&b[payload+16])<22) return false; encoding=u16(&b[payload+24]); }
            have_fmt=true;
        } else if(!memcmp(&b[pos], "data", 4) && !data) { data=&b[payload]; data_size=size; }
        pos=next;
    }
    if(!have_fmt || !data || encoding!=1 || (channels!=1 && channels!=2) || (bits!=8 && bits!=16) || !rate || rate>INT_MAX || align!=channels*(bits/8) || data_size%align) return false;
    size_t frames=data_size/align;
    if(!frames || frames>size_t(INT_MAX)/(bits/8)) return false;
    out.format=bits==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16; out.rate=static_cast<ALsizei>(rate); out.bytes.resize(frames*(bits/8));
    for(size_t i=0;i<frames;i++) {
        const unsigned char* sample=data+i*align;
        if(bits==8) out.bytes[i]=sample[0];
        else { out.bytes[i*2]=sample[0]; out.bytes[i*2+1]=sample[1]; }
    }
    return true;
}
int distance_volume(int volume, float distance) {
    volume=std::clamp(volume,-100,100);
    return static_cast<int>(-float(volume)/MAX_SOUNDDIST*distance+volume);
}
static float gain_for(int volume, float master) {
    volume=std::clamp(volume,-100,100);
    return std::clamp((float(volume+100)/200.0f)*master,0.0f,1.0f);
}
static float normalized_volume(float v) { if(!std::isfinite(v)) return 0; return std::clamp(v,0.0f,1.0f); }
}

using openal_pcm::Pcm;
using openal_pcm::gain_for;
using openal_pcm::normalized_volume;

SoundControl::SoundControl() : device(nullptr), context(nullptr), camera_x(0), camera_y(0), camera_z(0), camera_rx(0), mastervolume(1)
{
    for(int i=0;i<MAX_LOADSOUND;i++){ useflag[i]=false; buffers[i]=0; } for(int j=0;j<MAX_SOUNDLISTS;j++) sources[0][j]=0;
}
SoundControl::~SoundControl(){ DestroySound(); }
int SoundControl::InitSound(WindowControl* window)
{
    (void)window; // OpenAL needs no window handle; allow headless tests.
    if(context) return 0;
    ALCdevice* d=alcOpenDevice(nullptr);
    if(!d) return 0; // Audio is optional: retain silent operation without a device.
    ALCcontext* c=alcCreateContext(d,nullptr);
    if(!c){ alcCloseDevice(d); return 0; }
    if(!alcMakeContextCurrent(c)){ alcDestroyContext(c); alcCloseDevice(d); return 0; }
    device=d; context=c;
    alDistanceModel(AL_NONE);
    alListener3f(AL_POSITION,camera_x,camera_y,camera_z);
    return 0;
}
void SoundControl::DestroySound()
{
    if(context) { alcMakeContextCurrent(static_cast<ALCcontext*>(context));
        for(int j=0;j<MAX_SOUNDLISTS;j++) if(sources[0][j]) { alSourceStop(sources[0][j]); alSourcei(sources[0][j],AL_BUFFER,0); alDeleteSources(1,&sources[0][j]); } for(int i=0;i<MAX_LOADSOUND;i++){ if(buffers[i]) alDeleteBuffers(1,&buffers[i]); useflag[i]=false; buffers[i]=0; }
        alcMakeContextCurrent(nullptr); alcDestroyContext(static_cast<ALCcontext*>(context)); context=nullptr; }
    if(device){ alcCloseDevice(static_cast<ALCdevice*>(device)); device=nullptr; }
    for(int i=0;i<MAX_LOADSOUND;i++){ useflag[i]=false; buffers[i]=0; } for(int j=0;j<MAX_SOUNDLISTS;j++) sources[0][j]=0;
}
void SoundControl::SetVolume(float volume){ mastervolume=normalized_volume(volume); if(mastervolume<0.01f) mastervolume=0; }
void SoundControl::SetCamera(float x,float y,float z,float rx)
{
    camera_x=x; camera_y=y; camera_z=z; camera_rx=rx;
    if(context){ alcMakeContextCurrent(static_cast<ALCcontext*>(context)); alListener3f(AL_POSITION,x,y,z); alListener3f(AL_VELOCITY,0,0,0); float ori[]={std::cos(rx),0,std::sin(rx),0,1,0}; alListenerfv(AL_ORIENTATION,ori); }
}
int SoundControl::LoadSound(const char* filename)
{
    if(!filename) return -1;
    int slot=-1; for(int i=0;i<MAX_LOADSOUND;i++) if(!useflag[i]){slot=i;break;} if(slot<0) return -1;
    FILE* fp=vfs_fopen(filename,"rb"); if(!fp) return -1; Pcm pcm{}; bool ok=openal_pcm::decode_wav(fp,pcm); fclose(fp); if(!ok) return -1;
    if(!context){ useflag[slot]=true; return slot; }
    alcMakeContextCurrent(static_cast<ALCcontext*>(context)); alGenBuffers(1,&buffers[slot]); if(!buffers[slot]) return -1;
    alBufferData(buffers[slot],pcm.format,pcm.bytes.data(),static_cast<ALsizei>(pcm.bytes.size()),pcm.rate);
    if(alGetError()!=AL_NO_ERROR){ alDeleteBuffers(1,&buffers[slot]); buffers[slot]=0; return -1; }
    useflag[slot]=true; return slot;
}
int SoundControl::PlaySound(int id,int volume,int pan)
{
    if(id<0||id>=MAX_LOADSOUND||!useflag[id]) return 0;
    if(!context) { (void)gain_for(volume,mastervolume); (void)std::clamp(pan,-100,100); return 1; }
    alcMakeContextCurrent(static_cast<ALCcontext*>(context)); ALuint src=0;
    for(int i=0;i<MAX_SOUNDLISTS;i++){ if(!sources[0][i]) { alGenSources(1,&sources[0][i]); if(!sources[0][i]) return 0; alSourcei(sources[0][i],AL_SOURCE_RELATIVE,AL_TRUE); alSourcei(sources[0][i],AL_LOOPING,AL_FALSE); } ALint state; alGetSourcei(sources[0][i],AL_SOURCE_STATE,&state); if(state!=AL_PLAYING){src=sources[0][i];break;} }
    if(!src) return 0;
    float g=gain_for(volume,mastervolume); float p=float(std::clamp(pan,-100,100))/100.0f;
    alSourcei(src,AL_BUFFER,buffers[id]); alSourcef(src,AL_GAIN,g); alSource3f(src,AL_POSITION,p,0,-std::sqrt(std::max(0.0f,1-p*p))); alSourcePlay(src);
    return alGetError()==AL_NO_ERROR?1:0;
}
int SoundControl::Play3DSound(int id,float x,float y,float z,int volume)
{
    if(id<0||id>=MAX_LOADSOUND||!useflag[id]) return 0;
    float dx=x-camera_x,dy=y-camera_y,dz=z-camera_z,dist=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(dist)||dist>MAX_SOUNDDIST) return 0;
    // Match sound-ezds.cpp:Play3DSound/CalculationVolume exactly. Its pan
    // calculation is commented out; compatibility playback is centred.
    // A successfully scheduled silent voice is still a successful play.
    volume=std::clamp(volume,-100,100);
    int atten=openal_pcm::distance_volume(volume,dist);
    return PlaySound(id,atten,0);
}
int SoundControl::GetTotalSounds(){int n=0;for(bool b:useflag)if(b)n++;return n;}
void SoundControl::CleanupSound(int id)
{
    if(id<0||id>=MAX_LOADSOUND||!useflag[id])return;
    if(context){alcMakeContextCurrent(static_cast<ALCcontext*>(context)); for(int j=0;j<MAX_SOUNDLISTS;j++)if(sources[0][j]){ ALint attached=0; alGetSourcei(sources[0][j],AL_BUFFER,&attached); if(attached==static_cast<ALint>(buffers[id])) { alSourceStop(sources[0][j]); alSourcei(sources[0][j],AL_BUFFER,0); } } if(buffers[id])alDeleteBuffers(1,&buffers[id]);}
    buffers[id]=0;useflag[id]=false;
}
