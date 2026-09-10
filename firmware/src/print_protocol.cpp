#include "oracle/print_protocol.h"
#include <algorithm>
#include <cstring>

namespace oracle::bq95 {
std::uint8_t crc8(const std::uint8_t* p, std::size_t n) {
    std::uint8_t crc=0;
    while(n--) { crc^=*p++; for(int i=0;i<8;++i) crc=std::uint8_t((crc<<1)^((crc&128)?7:0)); }
    return crc;
}
std::uint32_t crc32(const std::uint8_t* p, std::size_t n) {
    std::uint32_t crc=~0u;
    while(n--) { crc^=*p++; for(int i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
    return ~crc;
}
std::size_t packet(std::uint8_t op, const std::uint8_t* data, std::size_t n, std::uint8_t* out) {
    if(n>64 || !out || (n && !data)) return 0;
    out[0]=0x51; out[1]=0x78; out[2]=op; out[3]=0; out[4]=n; out[5]=0;
    if(n) std::memcpy(out+6,data,n);
    out[n+6]=crc8(data,n); out[n+7]=0xff;
    return n+8;
}
void Parser::discard(std::size_t n) {
    std::memmove(bytes_.data(),bytes_.data()+n,count_-n); count_-=n;
}
bool Parser::push(std::uint8_t byte, Frame& frame) {
    if(count_==bytes_.size()) discard(1);
    bytes_[count_++]=byte;
    while(count_>=2) {
        if(bytes_[0]!=0x51 || bytes_[1]!=0x78) { discard(1); continue; }
        if(count_<6) return false;
        const unsigned n=bytes_[4]+unsigned(bytes_[5])*256;
        if(n>64 || bytes_[3]!=1) { discard(1); continue; }
        if(count_<n+8) return false;
        if(bytes_[n+7]!=0xff || bytes_[n+6]!=crc8(bytes_.data()+6,n)) { discard(1); continue; }
        frame.op=bytes_[2]; frame.size=n;
        std::copy_n(bytes_.data()+6,n,frame.data.begin());
        discard(n+8); return true;
    }
    return false;
}
bool validReceipt(const std::uint8_t* p, std::size_t n, std::uint32_t contentCrc, unsigned& height) {
    if(!p || n<kReceiptHeader || std::memcmp(p,"ORP1",4)) return false;
    auto u16=[&](unsigned i){return unsigned(p[i])+(unsigned(p[i+1])<<8);};
    auto u32=[&](unsigned i){return std::uint32_t(p[i])+(std::uint32_t(p[i+1])<<8)+(std::uint32_t(p[i+2])<<16)+(std::uint32_t(p[i+3])<<24);};
    const auto h=u16(6), size=u32(8);
    if(u16(4)!=kWidth || !h || h>kMaxHeight || size!=h*kRowBytes || n!=kReceiptHeader+size || u32(16)!=contentCrc) return false;
    if(crc32(p+kReceiptHeader,size)!=u32(12)) return false;
    height=h; return true;
}
}
