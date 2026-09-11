#pragma once
#include <cstdint>
#include <cstring>
namespace oracle {
// Read only the header and O(log n) fixed records; never allocate the font book.
template<class Read> bool readChineseFontFile(std::uint16_t code, std::uint16_t* rows, Read read) {
    std::uint8_t record[26];
    if(!read(0,record,12) || std::memcmp(record,"OCF1",4)) return false;
    const auto count=std::uint32_t(record[4])|(std::uint32_t(record[5])<<8)|
        (std::uint32_t(record[6])<<16)|(std::uint32_t(record[7])<<24);
    if(!count || count>65536) return false;
    unsigned low=0,high=count;
    while(low<high) {
        const auto mid=low+(high-low)/2;
        if(!read(12+mid*26,record,26)) return false;
        const auto found=unsigned(record[0])|(unsigned(record[1])<<8);
        if(found==code) {
            for(unsigned i=0;i<12;++i) rows[i]=unsigned(record[2+i*2])|(unsigned(record[3+i*2])<<8);
            return true;
        }
        if(found<code) low=mid+1; else high=mid;
    }
    return false;
}
}
