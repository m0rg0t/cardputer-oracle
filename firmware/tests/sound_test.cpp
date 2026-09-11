#include "oracle/sound.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace oracle;

struct FakeOutput {
    struct Buffer { const std::int16_t* ptr; std::vector<std::int16_t> original; };
    std::deque<Buffer> queue;
    bool fail=false, stopping=false;
    unsigned submitted=0;
    bool ready(){return true;}
    std::size_t queued()const{return queue.size();}
    void stop(){stopping=true;}
    void verify()const{for(const auto& b:queue)assert(std::equal(b.original.begin(),b.original.end(),b.ptr));}
    bool submit(const std::int16_t* pcm,std::size_t n){
        verify();assert(queue.size()<2);
        if(fail)return false;
        queue.push_back({pcm,{pcm,pcm+n}});++submitted;return true;
    }
    void retire(){verify();if(stopping){queue.clear();stopping=false;}else if(!queue.empty())queue.pop_front();}
};
std::vector<std::int16_t> render(Sound sound,std::size_t chunk){
    SoundSynth synth;synth.start(sound);std::vector<std::int16_t> out,part(chunk);
    while(synth.active()){auto n=synth.render(part.data(),part.size());assert(n);out.insert(out.end(),part.begin(),part.begin()+n);}
    assert(synth.render(part.data(),part.size())==0);return out;
}
void u32(std::ofstream& f,std::uint32_t v){for(int i=0;i<4;++i)f.put(char(v>>(8*i)));}
void u16(std::ofstream& f,std::uint16_t v){f.put(char(v));f.put(char(v>>8));}
void wav(const std::string& name,const std::vector<std::int16_t>& data){
    std::ofstream f(name,std::ios::binary);assert(f);
    f.write("RIFF",4);u32(f,36+data.size()*2);f.write("WAVEfmt ",8);u32(f,16);
    u16(f,1);u16(f,1);u32(f,kSoundRate);u32(f,kSoundRate*2);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,data.size()*2);for(auto x:data)u16(f,std::uint16_t(x));assert(f);
}
int main(int argc,char** argv){
    const Sound types[]={Sound::Click,Sound::Scroll,Sound::Cancel,Sound::Ritual,Sound::Reveal,Sound::Error};
    const char* names[]={"click","scroll","cancel","ritual","reveal","error"};
    const unsigned duration[]={70,35,195,2600,630,270};
    std::vector<std::int16_t> preview;
    for(unsigned i=0;i<6;++i){
        auto pcm=render(types[i],512);assert(pcm.size()==duration[i]*16);assert(pcm.front()==0 && pcm.back()==0);
        assert(pcm==render(types[i],137)); // chunk boundaries cannot change the waveform
        int peak=0;double sum=0;for(auto v:pcm){peak=std::max(peak,std::abs(int(v)));sum+=v;}
        assert(peak>300 && peak<16000);assert(std::abs(sum/pcm.size())<50);
        if(argc>1)wav(std::string(argv[1])+"/"+names[i]+".wav",pcm);
        preview.insert(preview.end(),pcm.begin(),pcm.end());preview.insert(preview.end(),8000,0);
    }
    if(argc>1)wav(std::string(argv[1])+"/all-effects.wav",preview);
    SoundSynth empty;std::int16_t scratch[3];assert(empty.render(scratch,3)==0);
    empty.start(Sound::Ritual);assert(empty.render(nullptr,100)==0);empty.stop();assert(!empty.active());
    FakeOutput output;SoundPlayer<FakeOutput> fx(output);
    assert(fx.play(Sound::Ritual));fx.update();fx.update();assert(output.queued()==2);
    assert(!fx.play(Sound::Click));auto before=output.submitted;
    for(int i=0;i<10;++i){
        fx.update();
    }
    assert(output.submitted==before);
    output.verify();
    // The old two live buffers must remain immutable until asynchronous stop completes.
    assert(fx.play(Sound::Reveal));fx.update();output.verify();assert(output.submitted==before);
    output.retire();fx.update();assert(output.submitted==before+1);
    for(unsigned i=0;i<100 && fx.busy();++i){fx.update();output.retire();}
    assert(!fx.busy());
    assert(fx.play(Sound::Click));output.fail=true;fx.update();assert(fx.busy());
    output.fail=false;fx.update();assert(output.queued()==1);
    fx.mute(true);assert(!fx.play(Sound::Reveal));fx.update();output.verify();output.retire();assert(!fx.busy());
    fx.mute(false);assert(fx.play(Sound::Cancel));fx.update();output.retire();
    // Stress buffer rotation, preemption and mute with a fake delayed audio task.
    for(unsigned i=0;i<1000;++i){
        if(i%53==0)fx.play(Sound::Ritual);
        if(i%53==37)fx.play(Sound::Reveal);
        if(i%101==0)fx.mute(true);
        if(i%101==2)fx.mute(false);
        fx.update();output.verify();if(i%3==0)output.retire();
    }
    NarrationGate gate;gate.request();assert(!gate.update(0,true,true));
    assert(!gate.update(100,false,true));assert(!gate.update(169,false,true));assert(gate.update(170,false,true));
    assert(!gate.update(200,false,true));
    gate.request();assert(!gate.update(300,false,false));assert(!gate.update(999,false,true));
    gate.request();gate.cancel();assert(!gate.update(9999,false,true));
    gate.request();assert(!gate.update(0xfffffff0u,false,true));assert(gate.update(0x36u,false,true));
    std::cout<<"PASS: six synth sounds, envelopes/headroom, chunk continuity, live-buffer lifetimes, priority/mute, narration ordering and rollover\n";
}
