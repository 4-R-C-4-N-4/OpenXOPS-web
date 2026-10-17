//! @file sound-openal-test.cpp
//! Synthetic WAV/VFS/OpenAL-null smoke test. No game audio assets are used.
#include "../src/sound.h"
#include "../src/platform/vfs.h"
#include "../src/platform/openal/pcm.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>


static void put16(std::vector<unsigned char>& b,unsigned v){b.push_back(v);b.push_back(v>>8);}
static void put32(std::vector<unsigned char>& b,unsigned v){for(int i=0;i<4;i++)b.push_back(v>>(i*8));}
static void chunk(std::vector<unsigned char>& b,const char* id,const std::vector<unsigned char>& d){b.insert(b.end(),id,id+4);put32(b,d.size());b.insert(b.end(),d.begin(),d.end());if(d.size()&1)b.push_back(0);}
static std::vector<unsigned char> wav(bool stereo=false){std::vector<unsigned char>b={'R','I','F','F',0,0,0,0,'W','A','V','E'},f;put16(f,1);put16(f,stereo?2:1);put32(f,22050);put32(f,stereo?88200:44100);put16(f,stereo?4:2);put16(f,16);chunk(b,"fmt ",f);chunk(b,"LIST",std::vector<unsigned char>{'I','N','F','O'});std::vector<unsigned char>d;for(int i=0;i<2205;i++){put16(d,0x1234);if(stereo)put16(d,0x7fff);}chunk(b,"data",d);unsigned n=b.size()-8;for(int i=0;i<4;i++)b[4+i]=n>>(i*8);return b;}
static bool save(const char* path,const std::vector<unsigned char>& b){FILE*f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(b.data(),1,b.size(),f)==b.size();fclose(f);return ok;}
int main(){
 const std::filesystem::path root=std::filesystem::temp_directory_path()/"openxops-openal-generated";std::filesystem::create_directories(root/"data"/"sound");
 std::string mono=(root/"data"/"sound"/"test.wav").string(),stereo_file=(root/"data"/"sound"/"stereo.wav").string(),bad=(root/"data"/"sound"/"bad.wav").string();
 if(!save(mono.c_str(),wav())||!save(stereo_file.c_str(),wav(true))||!save(bad.c_str(),std::vector<unsigned char>{'N','O','P','E'}))return 1;
 if(!vfs_mount_dir(root.string().c_str(),"generated"))return 2;
 ALCdevice* device=alcOpenDevice(nullptr);if(!device)return 3;ALCcontext* context=alcCreateContext(device,nullptr);if(!context)return 3;alcMakeContextCurrent(context);
 FILE* decoded_file=fopen(stereo_file.c_str(),"rb");
 openal_pcm::Pcm decoded{};
 if(!openal_pcm::decode_wav(decoded_file,decoded))return 10;
 fclose(decoded_file);
 if(decoded.format!=AL_FORMAT_MONO16 || decoded.bytes.size()!=2205*2)return 11;
 for(size_t i=0;i<decoded.bytes.size();i+=2)if(decoded.bytes[i]!=0x34 || decoded.bytes[i+1]!=0x12)return 12;
 SoundControl sound;
 if(sound.InitSound(nullptr)!=0 || !alcGetCurrentContext())return 13;
 int id=sound.LoadSound("data/sound/test.wav");if(id<0||sound.GetTotalSounds()!=1)return 4;
 if(sound.PlaySound(id,100,-100)!=1||sound.Play3DSound(id,0,0,-5,100)!=1||sound.Play3DSound(id,0,0,-MAX_SOUNDDIST-1,100)!=0)return 5;
 int stereo=sound.LoadSound("data/sound/stereo.wav");if(stereo<0)return 6;
 if(sound.LoadSound("data/sound/bad.wav")!=-1)return 7;
 sound.CleanupSound(id);if(sound.GetTotalSounds()!=1)return 8;
 sound.DestroySound();if(sound.GetTotalSounds()!=0)return 9;
 alcMakeContextCurrent(nullptr);alcDestroyContext(context);alcCloseDevice(device);
 vfs_unmount_all();std::filesystem::remove_all(root);return 0;
}
