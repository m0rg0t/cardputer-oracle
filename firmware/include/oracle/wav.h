#pragma once
#include <cstdint>
#include <cstring>
namespace oracle {
struct WavInfo { std::uint32_t rate=0, offset=0, bytes=0; };
inline std::uint16_t le16(const std::uint8_t* p) { return p[0] | std::uint16_t(p[1])<<8; }
inline std::uint32_t le32(const std::uint8_t* p) { return p[0] | std::uint32_t(p[1])<<8 | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24; }
// Reader needs size(), seek(offset), read(buffer, count); uses at most 16 bytes of header RAM.
template<class Reader> bool parseWav(Reader& f,WavInfo& out) {
    out={}; std::uint8_t h[16];
    if(f.size()<12 || !f.seek(0) || f.read(h,12)!=12 || std::memcmp(h,"RIFF",4) || std::memcmp(h+8,"WAVE",4)) return false;
    const std::uint64_t end=std::uint64_t(le32(h+4))+8;
    if(end>f.size() || end<12) return false;
    bool fmt=false, data=false;
    for(std::uint64_t pos=12;pos+8<=end;) {
        if(!f.seek(pos) || f.read(h,8)!=8) return false;
        const auto n=le32(h+4); const auto payload=pos+8;
        if(payload+n>end) return false;
        if(!std::memcmp(h,"fmt ",4)) {
            if(fmt || n<16 || f.read(h,16)!=16) return false;
            out.rate=le32(h+4);
            if(le16(h)!=1 || le16(h+2)!=1 || le16(h+14)!=16 || le16(h+12)!=2 ||
                out.rate<8000 || out.rate>48000 || le32(h+8)!=out.rate*2) return false;
            fmt=true;
        } else if(!std::memcmp(h,"data",4)) {
            if(data || !n || n%2) return false;
            out.offset=payload; out.bytes=n; data=true;
        }
        pos=payload+n+(n&1);
        if(pos>end) return false;
    }
    return fmt && data && f.seek(out.offset);
}
}
