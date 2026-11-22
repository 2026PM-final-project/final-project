#include "audio.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace bomber {
// [01] Lifecycle and thread-safe event handoff.
bool Audio::start() {
    if(running)return true;
    running=true;
    try {worker=std::thread(&Audio::loop,this);}catch(...){running=false;return false;}
    return true;
}
void Audio::play(Event event) {std::lock_guard<std::mutex> lock(mutex);pending.push_back(event);}
void Audio::mute(bool value) {muted=value;}
void Audio::stop() {running=false;if(worker.joinable())worker.join();}

// [02] Triple-buffered waveOut synthesis: melody + decaying event voices.
void Audio::loop() {
    constexpr int Rate=22050,Samples=1024,Count=3;
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;
    format.nSamplesPerSec=Rate;format.wBitsPerSample=16;
    format.nBlockAlign=2;format.nAvgBytesPerSec=Rate*2;
    HWAVEOUT device=nullptr;
    if(waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) {running=false;return;}
    struct Voice {float age=0,duration=.15f,from=440,to=220,volume=.15f;bool noise=false;};
    std::vector<Voice> voices;
    std::array<std::array<short,Samples>,Count> buffers{};
    std::array<WAVEHDR,Count> headers{};
    for(int i=0;i<Count;++i) {
        headers[i].lpData=reinterpret_cast<char*>(buffers[i].data());headers[i].dwBufferLength=Samples*2;
        waveOutPrepareHeader(device,&headers[i],sizeof(WAVEHDR));headers[i].dwFlags|=WHDR_DONE;
    }
    uint32_t noise=1234567;double clock=0;
    const float notes[]={220,277.18f,329.63f,440,329.63f,277.18f,246.94f,329.63f};
    while(running) {
        for(int i=0;i<Count&&running;++i) if(headers[i].dwFlags&WHDR_DONE) {
            std::vector<Event> events;
            {std::lock_guard<std::mutex> lock(mutex);events.swap(pending);}
            for(auto e:events) {
                Voice v;
                if(e==Event::Place) {v.from=400;v.to=160;v.duration=.10f;}
                if(e==Event::Explode) {v.from=90;v.to=35;v.duration=.34f;v.volume=.30f;v.noise=true;}
                if(e==Event::Pickup) {v.from=600;v.to=1300;v.duration=.24f;}
                if(e==Event::Hurt) {v.from=440;v.to=80;v.duration=.50f;v.volume=.24f;}
                if(e==Event::EnemyDown) {v.from=300;v.to=850;v.duration=.16f;}
                if(e==Event::Clear||e==Event::Win) {v.from=440;v.to=1760;v.duration=.85f;v.volume=.20f;}
                if(voices.size()<32)voices.push_back(v);
            }
            for(int sample=0;sample<Samples;++sample) {
                double frequency=notes[int(clock/.25)%8];
                double beat=std::fmod(clock,.25)/.25;
                double mixed=.032*std::sin(6.2831853*frequency*clock)*(1-beat);
                for(auto& v:voices) if(v.age<v.duration) {
                    float t=v.age/v.duration;
                    double phase=6.2831853*(v.from*v.age+.5f*(v.to-v.from)*v.age*t);
                    noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;
                    double wave=v.noise?double(noise%20000)/10000.-1.:std::sin(phase);
                    mixed+=wave*v.volume*(1-t)*(1-t);v.age+=1.f/Rate;
                }
                buffers[i][sample]=muted?0:short(std::clamp(mixed,-.95,.95)*32767);
                clock+=1./Rate;
            }
            voices.erase(std::remove_if(voices.begin(),voices.end(),[](const Voice& v){return v.age>=v.duration;}),voices.end());
            waveOutWrite(device,&headers[i],sizeof(WAVEHDR));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    waveOutReset(device);
    for(auto& h:headers)waveOutUnprepareHeader(device,&h,sizeof(WAVEHDR));
    waveOutClose(device);
}
} // namespace bomber
