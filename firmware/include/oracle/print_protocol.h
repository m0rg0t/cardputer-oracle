#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace oracle::bq95 {
constexpr unsigned kWidth = 384, kRowBytes = 48, kMaxHeight = 1024, kReceiptHeader = 20;
std::uint8_t crc8(const std::uint8_t* bytes, std::size_t count);
std::uint32_t crc32(const std::uint8_t* bytes, std::size_t count);
std::size_t packet(std::uint8_t op, const std::uint8_t* data, std::size_t size, std::uint8_t* out);
struct Frame { std::uint8_t op=0, size=0; std::array<std::uint8_t,64> data{}; };
class Parser {
public:
    bool push(std::uint8_t byte, Frame& frame);
private:
    std::array<std::uint8_t,72> bytes_{};
    std::size_t count_=0;
    void discard(std::size_t size);
};
inline bool ready(std::uint8_t state) { return state==0 || state==4; }
bool validReceipt(const std::uint8_t* data, std::size_t size, std::uint32_t contentCrc, unsigned& height);
enum class Outcome { Sent, Failed, Uncertain };

// The transport owns pacing, MTU splitting, connection/status checks and timeouts.
// Never retry a raster write: the peer may already have printed part of it.
template<class Transport> Outcome transmit(Transport& io, const std::uint8_t* raster, unsigned height) {
    if(!raster || !height || height>kMaxHeight) return Outcome::Failed;
    std::uint8_t bytes[72];
    auto send=[&](std::uint8_t op, const std::uint8_t* payload, std::size_t n) {
        return io.write(bytes, packet(op,payload,n,bytes));
    };
    const std::uint8_t zero=0, quality=0x33, speed=0x0a, feedSpeed=0x19;
    const std::uint8_t energy[]={0x10,0x27}, feed[]={0x30,0};
    const std::uint8_t start[]={0xaa,0x55,0x17,0x38,0x44,0x5f,0x5f,0x5f,0x44,0x38,0x2c};
    const std::uint8_t end[]={0xaa,0x55,0x17,0,0,0,0,0,0,0,0x17};
    if(!send(0xa3,&zero,1) || !send(0xa4,&quality,1) || !send(0xa6,start,sizeof(start)) ||
       !send(0xaf,energy,2) || !send(0xbe,&zero,1) || !send(0xbd,&speed,1)) return Outcome::Failed;
    for(unsigned row=0; row<height; ++row) {
        if(!send(0xa2,raster+row*kRowBytes,kRowBytes)) return Outcome::Uncertain;
        io.progress((row+1)*100/height);
    }
    if(!send(0xbd,&feedSpeed,1) || !send(0xa1,feed,2) || !send(0xa6,end,sizeof(end)) || !send(0xa3,&zero,1)) return Outcome::Uncertain;
    return Outcome::Sent; // Bytes accepted, not an assertion about paper.
}
}
