#include "oracle/chinese_font.h"
#include <cstring>
namespace oracle {
#if ORACLE_LANG_ZH
#include "oracle/chinese_glyphs.h"
static ChineseGlyphReader externalReader = nullptr;
void setChineseGlyphReader(ChineseGlyphReader reader) { externalReader = reader; }
bool loadChineseGlyph(std::uint32_t code, std::uint16_t* rows) {
    if (code > 65535) return false;
    unsigned low=0, high=sizeof(kChineseGlyphs)/sizeof(kChineseGlyphs[0]);
    while(low<high) {
        const unsigned mid=low+(high-low)/2;
        if(kChineseGlyphs[mid].code<code) low=mid+1;
        else high=mid;
    }
    if(low<sizeof(kChineseGlyphs)/sizeof(kChineseGlyphs[0]) && kChineseGlyphs[low].code==code) {
        std::memcpy(rows,kChineseGlyphs[low].rows,24); return true;
    }
    return externalReader && externalReader(code,rows);
}
#else
void setChineseGlyphReader(ChineseGlyphReader) {}
bool loadChineseGlyph(std::uint32_t, std::uint16_t*) { return false; }
#endif
}
