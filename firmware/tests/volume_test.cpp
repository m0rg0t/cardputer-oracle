#include "oracle/volume.h"
#include "oracle/sound.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace oracle;
int main(int argc,char** argv) {
    int previous=-32768, peak=0;
    std::ofstream lut;
    if(argc>1) lut.open(argv[1],std::ios::binary);
    for(int i=-32768;i<=32767;++i) {
        auto out=loudVoice(std::int16_t(i));
        assert(out>=previous && std::abs(int(out))<kVoiceCeiling);
        if(i>-32768) assert(int(out)==-int(loudVoice(std::int16_t(-i))));
        previous=out;peak=std::max(peak,std::abs(int(out)));
        if(lut) {lut.put(char(std::uint16_t(out)));lut.put(char(std::uint16_t(out)>>8));}
    }
    assert(loudVoice(0)==0 && loudVoice(1000)==2000);
    int effectPeak=0;
    for(auto sound:{Sound::Click,Sound::Scroll,Sound::Cancel,Sound::Ritual,Sound::Reveal,Sound::Error}) {
        SoundSynth synth;synth.start(sound);std::int16_t part[512];
        while(synth.active()) {
            auto n=synth.render(part,512);
            for(unsigned i=0;i<n;++i) effectPeak=std::max(effectPeak,std::abs(int(part[i])));
        }
    }
    // Bound the actual M5Unified I2S mix, including worst-sign simultaneous effect.
    const double masterGain=32.0*255*255*255*255/std::pow(2.0,36);
    const double effectGain=std::pow(kEffectVolume/255.0,2);
    const double worst=(kVoiceCeiling+effectPeak*effectGain)*masterGain;
    assert(worst<32767);
    std::cout<<"PASS: all PCM16 inputs; speech peak "<<peak<<", effects "<<effectPeak<<", conservative I2S mix "<<worst<<" < 32767\n";
}
