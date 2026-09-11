#include "oracle/locale.h"
#include "oracle/tarot_receipt.h"
#include "oracle/tarot.h"
#include "oracle/draw.h"
#include "oracle/print_protocol.h"
#include <cstring>
#include <new>

namespace oracle {
bool buildTarotReceipt(const Prediction& p, std::unique_ptr<std::uint8_t[]>& bytes,
                       std::size_t& size, unsigned& height) {
    if(p.topic!="tarot" || !validPrediction(p) || !validSpread(p.tarotCards)) return false;
    const auto rows=wrap(p.text,19);
    const auto questionRows=p.question.empty()?std::vector<std::string>{}:wrap(ORACLE_I18N("Question: ", "Вопрос: ", "问题： ")+p.question,28);
    if(rows.empty() || rows.size()>29) return false;
    const unsigned bodyY=210+(questionRows.empty()?0:questionRows.size()*(kChinese?26:18)+12);
    height=bodyY+rows.size()*(kChinese?38:26)+50;
    if(height>bq95::kMaxHeight) return false;
    size=bq95::kReceiptHeader+height*bq95::kRowBytes;
    std::unique_ptr<std::uint8_t[]> data(new(std::nothrow) std::uint8_t[size]{});
    if(!data) return false;
    lgfx::LGFX_Sprite strip;
    strip.setColorDepth(1);
    if(!strip.createSprite(384,32)) return false;
    strip.setPaletteColor(0,0x000000); strip.setPaletteColor(1,0xFFFFFF);
    for(unsigned top=0;top<height;top+=32) {
        strip.fillScreen(0xFFFFFF);
        if(top<210) {
            drawText(strip,30,14-int(top),ORACLE_I18N("ORACLE / TAROT", "ORACLE / ТАРО", "ORACLE / 塔罗"),3,0);
            drawTarotCards(strip,p,24,58-int(top),344,136,0);
        }
        for(unsigned i=0;i<questionRows.size();++i) {
            const int y=210+i*(kChinese?26:18)-int(top);
            if(y>-(kChinese?24:16) && y<32) drawText(strip,24,y,questionRows[i],2,0);
        }
        for(unsigned i=0;i<rows.size();++i) {
            const int y=bodyY+i*(kChinese?38:26)-int(top);
            if(y>-(kChinese?36:24) && y<32) drawText(strip,21,y,rows[i],3,0);
        }
        const int end=bodyY+rows.size()*(kChinese?38:26)-int(top);
        if(end>-48 && end<32) {
            strip.drawFastHLine(24,end+4,336,0);
            drawText(strip,36,end+18,ORACLE_I18N("Playful reading / OpenAI", "Шутливый расклад / OpenAI", "趣味解读 / OpenAI"),2,0);
        }
        for(unsigned y=0;y<32 && top+y<height;++y) for(unsigned x=0;x<384;++x) {
            if(strip.readPixel(x,y)==0) data[bq95::kReceiptHeader+(top+y)*48+x/8]|=1u<<(x%8);
        }
    }
    std::memcpy(data.get(),"ORP1",4);
    auto u16=[&](unsigned pos,unsigned value) {data[pos]=value;data[pos+1]=value>>8;};
    auto u32=[&](unsigned pos,std::uint32_t value) {for(unsigned i=0;i<4;++i)data[pos+i]=value>>(i*8);};
    u16(4,384); u16(6,height); u32(8,height*48);
    u32(12,bq95::crc32(data.get()+bq95::kReceiptHeader,height*48));
    const auto content=receiptContent(p);
    u32(16,bq95::crc32(reinterpret_cast<const std::uint8_t*>(content.data()),content.size()));
    bytes=std::move(data); return true;
}
}
