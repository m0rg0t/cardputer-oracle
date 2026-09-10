#include "oracle/sound.h"
#include <algorithm>
#include <cmath>

namespace oracle {
SoundSynth::SoundSynth() {
    for(unsigned i=0;i<sine_.size();++i) sine_[i]=std::sin(i*6.28318530718f/256.0f);
}
void SoundSynth::bell(float hz,unsigned startMs,unsigned durationMs,float level) {
    if(count_>=bells_.size() || !durationMs) return;
    auto& b=bells_[count_++]; b={};
    b.start=startMs*(kSoundRate/1000); b.duration=durationMs*(kSoundRate/1000);
    b.step=std::uint32_t(hz*(4294967296.0/kSoundRate));
    b.overtoneStep=std::uint32_t(hz*2.01*(4294967296.0/kSoundRate));
    b.level=level; b.decay=std::exp(-5.0f/b.duration); b.envelope=1;
    length_=std::max(length_,b.start+b.duration);
}
void SoundSynth::start(Sound sound) {
    stop(); count_=0;
    switch(sound) {
    case Sound::Click: bell(1318.51f,0,70,3300); break;
    case Sound::Scroll: bell(1046.50f,0,35,2000); break;
    case Sound::Cancel:
        bell(783.99f,0,100,3600); bell(523.25f,55,140,3000); break;
    case Sound::Ritual: {
        constexpr float notes[]={392,523.25f,587.33f,783.99f,1046.50f,1174.66f,1567.98f,2093.0f};
        constexpr unsigned starts[]={0,300,620,950,1300,1630,1930,2220};
        for(unsigned i=0;i<8;++i) bell(notes[i],starts[i],380,3800+i*350);
        break;
    }
    case Sound::Reveal:
        bell(1046.50f,0,560,5200); bell(1318.51f,45,520,4200);
        bell(1567.98f,90,490,3600); bell(2093.0f,180,450,2400); break;
    case Sound::Error:
        bell(392,0,140,3000); bell(349.23f,100,170,2400); break;
    case Sound::None: break;
    }
}
std::size_t SoundSynth::render(std::int16_t* out,std::size_t capacity) {
    if(!out) return 0;
    const auto size=std::min<std::size_t>(capacity,length_-position_);
    for(std::size_t i=0;i<size;++i,++position_) {
        float mixed=0;
        for(unsigned j=0;j<count_;++j) {
            auto& b=bells_[j];
            if(position_<b.start || position_>=b.start+b.duration) continue;
            const auto age=position_-b.start;
            // Soft attack and final fade suppress onset/offset clicks.
            const float attack=std::min(1.0f,age/96.0f);
            const float release=std::min(1.0f,(b.duration-age-1)/480.0f);
            mixed+=(sine_[b.phase>>24]+0.20f*sine_[b.overtone>>24])*b.level*b.envelope*attack*release;
            b.phase+=b.step; b.overtone+=b.overtoneStep; b.envelope*=b.decay;
        }
        out[i]=std::int16_t(std::max(-16000.0f,std::min(16000.0f,mixed)));
    }
    return size;
}
}
