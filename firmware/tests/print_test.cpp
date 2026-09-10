#include "oracle/print_protocol.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
using namespace oracle::bq95;
struct Transport {
    unsigned calls=0,failAt=~0u,lastProgress=0;
    std::vector<std::uint8_t> stream;
    bool write(const std::uint8_t* p,std::size_t n) {
        if(calls++==failAt) return false;
        stream.insert(stream.end(),p,p+n); return true;
    }
    void progress(unsigned value) { assert(value>=lastProgress && value<=100); lastProgress=value; }
};
void put32(std::vector<std::uint8_t>& p,unsigned pos,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) p[pos+i]=value>>(8*i);
}
int main(int argc,char** argv) {
    const auto golden=reinterpret_cast<const std::uint8_t*>("123456789");
    assert(crc8(golden,9)==0xf4 && crc32(golden,9)==0xcbf43926);
    std::uint8_t bytes[72], zero=0;
    assert(packet(0xa3,&zero,1,bytes)==9);
    const std::uint8_t query[]={0x51,0x78,0xa3,0,1,0,0,0,0xff};
    assert(std::memcmp(bytes,query,9)==0);
    assert(packet(0,nullptr,65,bytes)==0);
    // Fragmented notifications, debug text, bad direction/CRC/length and recovery.
    for(unsigned seed=1;seed<=100;++seed) {
        Parser parser; Frame frame; unsigned responses=0;
        std::vector<std::uint8_t> input(100,0x41);
        input.insert(input.end(),{0x51,0x78,0xa3,1,0xff,0xff,0x51});
        input.insert(input.end(),query,query+9); // request echo must be ignored
        for(unsigned state : {0u,4u,8u}) {
            auto b=std::uint8_t(state); auto n=packet(0xa3,&b,1,bytes); bytes[3]=1;
            auto corrupt=std::vector<std::uint8_t>(bytes,bytes+n); corrupt[n-2]^=1;
            input.insert(input.end(),corrupt.begin(),corrupt.end());
            input.insert(input.end(),bytes,bytes+n);
        }
        for(auto byte:input) if(parser.push(byte,frame)) {
            assert(frame.op==0xa3 && frame.size==1);
            assert(frame.data[0]==(responses==0?0:responses==1?4:8)); ++responses;
        }
        assert(responses==3);
    }
    assert(ready(0) && ready(4) && !ready(8) && !ready(0xff));
    std::vector<std::uint8_t> receipt(20+96,0);
    std::memcpy(receipt.data(),"ORP1",4); receipt[4]=0x80;receipt[5]=1;receipt[6]=2;
    receipt[20]=1;receipt[67]=0x80;receipt[68]=0xa5;
    put32(receipt,8,96); put32(receipt,12,crc32(receipt.data()+20,96));put32(receipt,16,0x12345678);
    unsigned height=0;assert(validReceipt(receipt.data(),receipt.size(),0x12345678,height) && height==2);
    assert(!validReceipt(receipt.data(),receipt.size(),0,height));
    assert(!validReceipt(receipt.data(),receipt.size()-1,0x12345678,height));
    receipt.back()^=1;assert(!validReceipt(receipt.data(),receipt.size(),0x12345678,height));receipt.back()^=1;
    Transport good; assert(transmit(good,receipt.data()+20,2)==Outcome::Sent && good.lastProgress==100);
    // Every failure point terminates immediately: no duplicate row or automatic retry.
    for(unsigned fail=0;fail<good.calls;++fail) {
        Transport bad;bad.failAt=fail;
        assert(transmit(bad,receipt.data()+20,2)==(fail<6?Outcome::Failed:Outcome::Uncertain));
        assert(bad.calls==fail+1);
        assert(std::equal(bad.stream.begin(),bad.stream.end(),good.stream.begin()));
    }
    assert(transmit(good,nullptr,2)==Outcome::Failed);
    // Optional export uses the exact C++ firmware encoder for Mac end-to-end testing.
    if(argc==3) {
        std::ifstream f(argv[1],std::ios::binary);
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)),{});
        assert(data.size()>20);
        auto crc=std::uint32_t(data[16])|(std::uint32_t(data[17])<<8)|(std::uint32_t(data[18])<<16)|(std::uint32_t(data[19])<<24);
        assert(validReceipt(data.data(),data.size(),crc,height));
        Transport actual;assert(transmit(actual,data.data()+20,height)==Outcome::Sent);
        std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(actual.stream.data()),actual.stream.size());assert(out);
        std::cout<<"Exported "<<height<<" rows, "<<actual.stream.size()<<" bytes\n";
    }
    std::cout<<"PASS: CRC goldens, fragmented/junk status parser, receipt integrity, every transmit failure point, no retries\n";
}
