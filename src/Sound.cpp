// Sound implementation using OpenAL-soft (optional).
// If OpenAL headers/lib are absent the whole body compiles away to stubs.
#include "Sound.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
namespace fs = std::filesystem;

// ─── tiny OGG decoder (stb_vorbis embedded as header would be ideal, but we
//     shell out to the OS decoder via OpenAL's alure/dr_libs-style; here we use
//     a manual decode path with the system libvorbisfile if it exists at link time,
//     otherwise we skip audio entirely). ─────────────────────────────────────────
// We use a load-time dlopen approach so the binary doesn't hard-depend on OpenAL.

#ifdef __linux__
#include <dlfcn.h>
#define OPENAL_AVAILABLE 1
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define OPENAL_AVAILABLE 1
#elif defined(__APPLE__)
#define OPENAL_AVAILABLE 1
#endif

#if OPENAL_AVAILABLE
// OpenAL types we need (subset of al.h / alc.h, avoids hard header dependency)
typedef unsigned int  ALuint;
typedef int           ALint;
typedef unsigned int  ALenum;
typedef float         ALfloat;
typedef void          ALvoid;
typedef char          ALchar;
struct ALCdevice; struct ALCcontext;
#define AL_NO_ERROR            0
#define AL_FORMAT_MONO16       0x1101
#define AL_FORMAT_STEREO16     0x1103
#define AL_BUFFERS_PROCESSED   0x1016
#define AL_LOOPING             0x1007
#define AL_GAIN                0x100A
#define AL_PITCH               0x1003
#define AL_SOURCE_STATE        0x1010
#define AL_PLAYING             0x1012
#define AL_TRUE                1
#define AL_FALSE               0
#define ALC_DEFAULT_DEVICE_SPECIFIER 0x1004

// function pointers loaded at runtime
static void*  g_al  = nullptr;
static void*  g_alc = nullptr;
static ALuint (*al_GenBuffers)(ALint,ALuint*) = nullptr;
static void   (*al_BufferData)(ALuint,ALenum,const ALvoid*,ALint,ALint) = nullptr;
static ALuint (*al_GenSources)(ALint,ALuint*) = nullptr;
static void   (*al_Sourcei)(ALuint,ALenum,ALint) = nullptr;
static void   (*al_Sourcef)(ALuint,ALenum,ALfloat) = nullptr;
static void   (*al_SourcePlay)(ALuint) = nullptr;
static void   (*al_SourceStop)(ALuint) = nullptr;
static void   (*al_DeleteSources)(ALint,const ALuint*) = nullptr;
static void   (*al_DeleteBuffers)(ALint,const ALuint*) = nullptr;
static void   (*al_GetSourcei)(ALuint,ALenum,ALint*) = nullptr;
static ALCdevice*  (*alc_OpenDevice)(const ALchar*) = nullptr;
static ALCcontext* (*alc_CreateContext)(ALCdevice*,const ALint*) = nullptr;
static void   (*alc_MakeContextCurrent)(ALCcontext*) = nullptr;
static void   (*alc_DestroyContext)(ALCcontext*) = nullptr;
static void   (*alc_CloseDevice)(ALCdevice*) = nullptr;

// stb_vorbis - we include a tiny subset inline rather than the full header
// to avoid bloating context. For a real build use stb_vorbis.c.
// Here we use libvorbisfile via dlopen as an alternative.
typedef struct OggVorbis_File_ OggVorbis_File;
static int  (*ov_fopen)(const char*,OggVorbis_File*) = nullptr;
static long (*ov_read)(OggVorbis_File*,char*,int,int,int,int,int*) = nullptr;
static void*(*ov_info)(OggVorbis_File*,int) = nullptr;   // returns vorbis_info*
static int  (*ov_clear)(OggVorbis_File*) = nullptr;
static int64_t (*ov_pcm_total)(OggVorbis_File*,int) = nullptr;

struct VorbisInfo { int version; int channels; long rate; long bitrate_upper,bitrate_nominal,bitrate_lower,bitrate_window; void*codec_setup; };

static void* loadSym(void* lib, const char* name) {
#ifdef _WIN32
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}
static void* openLib(const char* name) {
#ifdef _WIN32
    return (void*)LoadLibraryA(name);
#elif defined(__APPLE__)
    return dlopen(name, RTLD_NOW|RTLD_LOCAL);
#else
    return dlopen(name, RTLD_NOW|RTLD_LOCAL);
#endif
}

static bool loadOpenAL() {
    const char* alNames[]  = {"libopenal.so.1","libopenal.so","OpenAL32.dll","libopenal.dylib",nullptr};
    const char* alcNames[] = {nullptr}; // same lib usually
    for(int i=0; alNames[i]; i++) { g_al = openLib(alNames[i]); if(g_al) break; }
    if(!g_al) return false;
    g_alc = g_al;
    #define LS(fn,nm) fn=(decltype(fn))loadSym(g_al,nm); if(!fn) return false
    LS(al_GenBuffers,"alGenBuffers"); LS(al_BufferData,"alBufferData");
    LS(al_GenSources,"alGenSources"); LS(al_Sourcei,"alSourcei");
    LS(al_Sourcef,"alSourcef"); LS(al_SourcePlay,"alSourcePlay");
    LS(al_SourceStop,"alSourceStop"); LS(al_DeleteSources,"alDeleteSources");
    LS(al_DeleteBuffers,"alDeleteBuffers"); LS(al_GetSourcei,"alGetSourcei");
    LS(alc_OpenDevice,"alcOpenDevice"); LS(alc_CreateContext,"alcCreateContext");
    LS(alc_MakeContextCurrent,"alcMakeContextCurrent");
    LS(alc_DestroyContext,"alcDestroyContext"); LS(alc_CloseDevice,"alcCloseDevice");
    #undef LS
    return true;
}
static bool loadVorbis() {
    const char* names[] = {"libvorbisfile.so.3","libvorbisfile.so","vorbisfile.dll","libvorbisfile.dylib",nullptr};
    void* lib = nullptr;
    for(int i=0;names[i];i++) { lib=openLib(names[i]); if(lib) break; }
    if(!lib) return false;
    #define LS(fn,nm) fn=(decltype(fn))loadSym(lib,nm); if(!fn) return false
    LS(ov_fopen,"ov_fopen"); LS(ov_read,"ov_read");
    LS(ov_info,"ov_info"); LS(ov_clear,"ov_clear"); LS(ov_pcm_total,"ov_pcm_total");
    #undef LS
    return true;
}

// ─── Decode one .ogg file into a 16-bit PCM buffer ────────────────────────────
struct PcmBuf { std::vector<int16_t> samples; int channels=0; int rate=0; };
static bool decodeOgg(const fs::path& path, PcmBuf& out) {
    if(!ov_fopen) return false;
    // OggVorbis_File is ~880 bytes; allocate on heap to avoid size guessing
    std::vector<char> vf(1024, 0);
    if(ov_fopen(path.string().c_str(), (OggVorbis_File*)vf.data()) != 0) return false;
    VorbisInfo* vi = (VorbisInfo*)ov_info((OggVorbis_File*)vf.data(), -1);
    out.channels = vi ? vi->channels : 1;
    out.rate     = vi ? (int)vi->rate : 44100;
    char buf[4096]; int section=0; long n;
    while((n=ov_read((OggVorbis_File*)vf.data(),buf,sizeof buf,0,2,1,&section))>0) {
        const int16_t* p=(const int16_t*)buf; out.samples.insert(out.samples.end(),p,p+n/2);
    }
    ov_clear((OggVorbis_File*)vf.data());
    return !out.samples.empty();
}

// ─── Sound index: maps event name -> list of .ogg paths (random pick each time)
struct SoundIndex {
    std::unordered_map<std::string, std::vector<fs::path>> events;
    void build(const fs::path& root) {
        std::error_code ec;
        for(auto& e : fs::recursive_directory_iterator(root, ec)) {
            if(!e.is_regular_file() || e.path().extension()!=".ogg") continue;
            // path like sounds/block/stone/break1.ogg -> event "block.stone.break"
            auto rel = fs::relative(e.path(), root, ec);
            std::string ev;
            auto it = rel.begin();
            // skip leading "sounds/" segment if present
            if(it!=rel.end() && *it=="sounds") ++it;
            int seg=0;
            for(;it!=rel.end();++it,++seg) {
                if(it->extension()==".ogg") {
                    // strip trailing digits from stem to get base event
                    std::string stem = it->stem().string();
                    while(!stem.empty() && std::isdigit((unsigned char)stem.back())) stem.pop_back();
                    if(!ev.empty()) ev+='.';
                    ev+=stem;
                } else {
                    if(!ev.empty()) ev+='.';
                    ev+=it->string();
                }
            }
            if(!ev.empty()) events[ev].push_back(e.path());
        }
    }
    const fs::path* pick(const std::string& ev) const {
        auto it=events.find(ev); if(it==events.end()||it->second.empty()) return nullptr;
        static std::mt19937 rng(std::random_device{}());
        return &it->second[std::uniform_int_distribution<size_t>(0,it->second.size()-1)(rng)];
    }
};

struct Sound::Impl {
    ALCdevice*  device=nullptr;
    ALCcontext* ctx=nullptr;
    SoundIndex  index;
    std::unordered_map<ALuint, bool> sources;   // source -> looping
    std::unordered_map<std::string,ALuint> loops;
    std::unordered_map<std::string,ALuint> bufCache;  // path str -> buffer
    std::mutex mu;

    ALuint getBuffer(const fs::path& p) {
        const std::string key=p.string();
        auto it=bufCache.find(key); if(it!=bufCache.end()) return it->second;
        PcmBuf pcm; if(!decodeOgg(p,pcm)) return 0;
        ALuint buf=0; al_GenBuffers(1,&buf);
        ALenum fmt = pcm.channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
        al_BufferData(buf, fmt, pcm.samples.data(), (ALint)(pcm.samples.size()*2), pcm.rate);
        bufCache[key]=buf; return buf;
    }
    void gc() {  // remove finished one-shot sources
        for(auto it=sources.begin();it!=sources.end();) {
            if(!it->second) { ALint state=0; al_GetSourcei(it->first,AL_SOURCE_STATE,&state);
                if(state!=AL_PLAYING){ al_DeleteSources(1,&it->first); it=sources.erase(it); continue; } }
            ++it;
        }
    }
    void play(const SoundIndex& idx, const std::string& ev, float vol, float pitch, bool loop_, Sound& owner) {
        std::lock_guard<std::mutex> lk(mu); gc();
        const fs::path* p=idx.pick(ev); if(!p) return;
        ALuint buf=getBuffer(*p); if(!buf) return;
        ALuint src=0; al_GenSources(1,&src);
        al_Sourcei(src,0x1009,buf);  // AL_BUFFER=0x1009
        al_Sourcef(src,AL_GAIN,vol*owner.volMaster);
        al_Sourcef(src,AL_PITCH,pitch);
        al_Sourcei(src,AL_LOOPING,loop_?AL_TRUE:AL_FALSE);
        al_SourcePlay(src);
        sources[src]=loop_;
        if(loop_) loops[ev]=src;
    }
    void stopLoop(const std::string& ev) {
        std::lock_guard<std::mutex> lk(mu);
        auto it=loops.find(ev); if(it==loops.end()) return;
        al_SourceStop(it->second); al_DeleteSources(1,&it->second); sources.erase(it->second); loops.erase(it);
    }
};
#else
struct Sound::Impl {};
static bool loadOpenAL(){return false;} static bool loadVorbis(){return false;}
#endif

Sound& Sound::get(){ static Sound s; return s; }

bool Sound::init(const fs::path& root) {
    m_root = root;
#if OPENAL_AVAILABLE
    if(!loadOpenAL()||!loadVorbis()) { std::cout<<"[Sound] OpenAL or libvorbisfile not found - running silent\n"; return false; }
    m_impl = std::make_unique<Impl>();
    m_impl->device  = alc_OpenDevice(nullptr);
    if(!m_impl->device) { m_impl.reset(); return false; }
    m_impl->ctx = alc_CreateContext(m_impl->device, nullptr);
    alc_MakeContextCurrent(m_impl->ctx);
    m_impl->index.build(root);
    std::cout<<"[Sound] OpenAL ok, "<<m_impl->index.events.size()<<" sound events indexed from "<<root<<"\n";
    m_ok=true; return true;
#else
    return false;
#endif
}
void Sound::shutdown() {
#if OPENAL_AVAILABLE
    if(!m_impl) return;
    for(auto& kv:m_impl->sources) { al_SourceStop(kv.first); al_DeleteSources(1,&kv.first); }
    for(auto& kv:m_impl->bufCache) { ALuint b=kv.second; al_DeleteBuffers(1,&b); }
    if(m_impl->ctx)    { alc_MakeContextCurrent(nullptr); alc_DestroyContext(m_impl->ctx); }
    if(m_impl->device) { alc_CloseDevice(m_impl->device); }
    m_impl.reset(); m_ok=false;
#endif
}
void Sound::play(const std::string& ev, float vol, float pitch) {
#if OPENAL_AVAILABLE
    if(!m_ok||!m_impl) return; m_impl->play(m_impl->index,ev,vol,pitch,false,*this);
#endif
}
void Sound::loop(const std::string& ev, float vol) {
#if OPENAL_AVAILABLE
    if(!m_ok||!m_impl) return;
    { std::lock_guard<std::mutex> lk(m_impl->mu);          // play() locks the same mutex itself: never hold it here
      if(m_impl->loops.count(ev)) return; }                // already looping
    m_impl->play(m_impl->index,ev,vol,1.f,true,*this);
#endif
}
void Sound::stopLoop(const std::string& ev) {
#if OPENAL_AVAILABLE
    if(!m_ok||!m_impl) return; m_impl->stopLoop(ev);
#endif
}
