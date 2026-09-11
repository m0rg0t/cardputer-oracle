#include "oracle/chinese_font.h"
#include "oracle/chinese_font_file.h"
#include "oracle/text_rules.h"
#include "oracle/core.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
int main() {
    using namespace oracle;
    if(!kChinese) return 0;
    std::uint16_t rows[12];
    assert(loadChineseGlyph(0x8336,rows)); // 茶 exists without the SD card.
    assert(!loadChineseGlyph(0x1f600,rows));
    assert(hermes_terminal::utf8Columns("茶A茶",7)==5);
    assert(hermes_terminal::utf8PrefixBytes("茶A",4,1)==0);
    assert(hermes_terminal::utf8PrefixBytes("茶A",4,2)==3);
    assert(wrap("茶茶",1)==std::vector<std::string>({"?","?"}));
    const auto lines=wrap("茶A茶 B茶茶",3);
    std::string restored;
    for(const auto& line:lines) {
        assert(hermes_terminal::utf8Columns(line.data(),line.size())<=3);
        restored+=line;
    }
    assert(restored=="茶A茶B茶茶");
    const auto punctuation=wrap("茶茶。茶茶！",4);
    assert(punctuation.size()==4 && punctuation[0]=="茶" && punctuation[1]=="茶。" && punctuation[3]=="茶！");
    FILE* file=fopen("sdcard/ORACLE/fonts/chinese12.bin","rb"); assert(file);
    unsigned reads=0;
    const auto read=[&](unsigned offset,std::uint8_t* bytes,unsigned size) {
        ++reads; assert(size<=26);
        return fseek(file,offset,SEEK_SET)==0 && fread(bytes,1,size,file)==size;
    };
    assert(readChineseFontFile(0x9f99,rows,read)); // 龙: lookup in the complete font.
    assert(reads<=17);
    assert(!readChineseFontFile(0xffff,rows,read));
    assert(!readChineseFontFile(0x8336,rows,[](unsigned,std::uint8_t*,unsigned){ return false; }));
    fclose(file);
    std::cout<<"PASS: Chinese fallback glyphs, double-width UTF-8 wrapping, bounded SD glyph lookup\n";
}
