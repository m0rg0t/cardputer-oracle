#include "oracle/core.h"
#include "oracle/text_rules.h"
#include "oracle/wav.h"
#include <cassert>
#include <iostream>
#include <set>
#include <cstring>
struct MemoryReader {
    std::vector<std::uint8_t> data; std::size_t pos=0;
    std::size_t size(){return data.size();}
    bool seek(std::size_t n){pos=n;return n<=data.size();}
    std::size_t read(std::uint8_t* p,std::size_t n) {auto got=std::min(n,data.size()-pos); std::memcpy(p,data.data()+pos,got);pos+=got;return got;}
};
struct Broken : oracle::Catalog {std::size_t size()const override{return 3;} bool load(std::size_t,oracle::Prediction&)override{return false;}};
void u32(std::vector<std::uint8_t>& d,std::uint32_t x){for(int i=0;i<4;++i)d.push_back(x>>(8*i));}
void tag(std::vector<std::uint8_t>& d,const char* s){for(int i=0;i<4;++i)d.push_back(s[i]);}
MemoryReader wav(bool metadata=true){
    MemoryReader r; auto& d=r.data;
    tag(d,"RIFF");u32(d,0);tag(d,"WAVE");
    if(metadata){tag(d,"JUNK");u32(d,3);d.insert(d.end(),{1,2,3,0});}
    tag(d,"fmt ");u32(d,16);d.insert(d.end(),{1,0,1,0});u32(d,16000);u32(d,32000);d.insert(d.end(),{2,0,16,0});
    tag(d,"data");u32(d,4);d.insert(d.end(),{0,0,1,0});
    auto n=d.size()-8;for(int i=0;i<4;++i)d[4+i]=n>>(8*i);return r;
}
int main(){
    using namespace oracle;
    BuiltinCatalog cat; assert(cat.size()==18);
    constexpr unsigned defaultCards=6;
    for(unsigned seed=0;seed<80;++seed){
        DeckSource engine(cat,seed); Prediction p;std::string last;
        for(unsigned round=0;round<8;++round){
            std::set<std::string> ids;
            for(unsigned i=0;i<defaultCards;++i){assert(engine.next({},p));assert(p.topic=="life" || p.topic=="it");assert(p.id!=last);assert(ids.insert(p.id).second);assert(validPrediction(p));last=p.id;}
            assert(ids.size()==defaultCards);
        }
        assert(!engine.next({"topic",""},p)); assert(!engine.next({"","question"},p));
    }
    DeckSource src(cat,32);Controller c(src);
    assert(c.trigger(0xffffff00));assert(!c.trigger(0xffffff01));assert(!c.tick(0xffffff10));
    assert(c.tick(std::uint32_t(0xffffff00u+2600u)));assert(!c.tick(10000));
    c.scroll(9999);assert(c.view.scroll==c.view.lines.size()-kVisibleLines);c.scroll(-999);assert(!c.view.scroll);
    c.back();assert(c.view.screen==Screen::Idle);
    assert(c.trigger(1));c.back();assert(!c.tick(9999));
    Broken broken;DeckSource bad(broken,1);Prediction p;assert(!bad.next({},p));
    assert(!validAssetPath("/ORACLE/../secret.wav",".wav"));assert(!validAssetPath("https://x/a.wav",".wav"));
    assert(validAssetPath("/ORACLE/audio/cat.wav",".wav"));
    for(std::size_t i=0;i<cat.size();++i){cat.load(i,p);auto lines=wrap(p.text,kColumns);assert(!lines.empty());
        for(auto& line:lines){assert(hermes_terminal::utf8Columns(line.data(),line.size())<=kColumns);assert(line.find('?')==std::string::npos || p.text.find('?')!=std::string::npos);}
    }
    auto rows=wrap("Ёж ёж\nАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩ",4);for(auto& row:rows)assert(hermes_terminal::utf8Columns(row.data(),row.size())<=4);
    WavInfo info;auto good=wav();assert(parseWav(good,info));assert(info.rate==16000 && info.bytes==4 && good.pos==info.offset);
    auto truncated=good;truncated.data.pop_back();assert(!parseWav(truncated,info));
    auto stereo=wav(false);stereo.data[22]=2;assert(!parseWav(stereo,info));
    auto huge=wav();huge.data[16]=255;assert(!parseWav(huge,info));
    auto odd=wav(false);odd.data[40]=3;assert(!parseWav(odd,info));
    std::cout<<"PASS: 19,200 draws, deck cycle boundaries, cancellation/rollover, Cyrillic wrapping, paths, bounded WAV parser\n";
}
