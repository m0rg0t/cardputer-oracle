#include "oracle/locale.h"
#include "oracle/draw.h"
#include "oracle/text_rules.h"
#include "oracle/cyrillic_font.h"
#include "oracle/chinese_font.h"
#include <cmath>
#include <cstdio>

namespace oracle {
void drawText(lgfx::LGFX_Sprite& c, int x, int y, const std::string& str, int scale, std::uint32_t color) {
    c.setTextFont(1); c.setTextSize(scale); c.setTextWrap(false); c.setTextColor(color);
    for (std::size_t i=0; i<str.size();) {
        std::uint32_t cp; i += hermes_terminal::decodeUtf8(str.data()+i, str.size()-i, cp);
        const int glyph=hermes_terminal::cyrillicGlyphIndex(cp);
        if (hermes_terminal::isChinese(cp)) {
            std::uint16_t rows[12];
            if(loadChineseGlyph(cp,rows)) {
                for(int row=0;row<12;++row) for(int col=0;col<12;++col)
                    if(rows[row] & (1u<<(11-col))) c.fillRect(x+col*scale,y+row*scale,scale,scale,color);
            } else c.drawRect(x,y,11*scale,11*scale,color);
        } else if (glyph>=0) {
            for (int row=0;row<7;++row) for(int col=0;col<5;++col)
                if(hermes_terminal::kCyrillicGlyphs[glyph][row] & (0x10>>col)) c.fillRect(x+col*scale,y+row*scale,scale,scale,color);
        } else {
            char ch=cp<128 ? char(cp) : hermes_terminal::asciiFallback(cp);
            c.setCursor(x,y); c.write(ch ? ch : '?');
        }
        x+=6*scale*hermes_terminal::glyphColumns(cp);
    }
}
void drawTarotCards(lgfx::LGFX_Sprite& c, const Prediction& p, int x, int y, int width, int height, std::uint32_t color) {
    const int step=width/3, card=step-8;
    for(unsigned i=0;i<3;++i) {
        const int left=x+i*step, mid=left+card/2, cy=y+height/2;
        c.drawRoundRect(left,y,card,height,5,color);
        c.drawRoundRect(left+4,y+4,card-8,height-8,4,color);
        drawText(c,left+10,y+10,std::to_string(p.tarotCards[i]),1,color);
        const int radius=std::min(card/3,height/4);
        c.drawCircle(mid,cy,radius,color);
        const auto kind=p.tarotCards[i]%4;
        if(kind==0) {
            c.drawEllipse(mid,cy,radius-4,radius/2,color);
            c.fillCircle(mid,cy,std::max(2,radius/5),color);
        } else if(kind==1) {
            c.drawTriangle(mid,cy-radius+4,mid-radius+4,cy+radius/2,mid+radius-4,cy+radius/2,color);
            c.drawTriangle(mid,cy+radius-4,mid-radius+4,cy-radius/2,mid+radius-4,cy-radius/2,color);
        } else if(kind==2) {
            c.drawCircle(mid-3,cy,radius/2,color);
            c.drawFastVLine(mid+radius/2,cy-radius/2,radius,color);
        } else {
            c.drawLine(mid-radius+4,cy,mid+radius-4,cy,color);
            c.drawLine(mid,cy-radius+4,mid,cy+radius-4,color);
            c.drawLine(mid-radius/2,cy-radius/2,mid+radius/2,cy+radius/2,color);
            c.drawLine(mid-radius/2,cy+radius/2,mid+radius/2,cy-radius/2,color);
        }
        const char* labels[]={ORACLE_I18N("PAST", "БЫЛО", "过去"),ORACLE_I18N("PRESENT", "СЕЙЧАС", "现在"),ORACLE_I18N("FUTURE", "ВПЕРЕДИ", "未来")};
        const int length=hermes_terminal::utf8Columns(labels[i],std::char_traits<char>::length(labels[i]));
        drawText(c,mid-length*3,y+height-16,labels[i],1,color);
    }
}
namespace {
constexpr std::uint32_t bg=0x100E1B, panel=0x211C32, ink=0xF6EDDD, gold=0xEAC57B, muted=0xAEA0BF;
void text(lgfx::LGFX_Sprite& c,int x,int y,const std::string& str,int scale=1,std::uint32_t color=ink) {
    drawText(c,x,y,str,scale,color);
}
void center(lgfx::LGFX_Sprite& c, int y, const std::string& str, int scale=1, std::uint32_t color=ink) {
    int cols=hermes_terminal::utf8Columns(str.data(),str.size());
    text(c,(240-cols*6*scale)/2,y,str,scale,color);
}
void star(lgfx::LGFX_Sprite& c,int x,int y,int r,std::uint32_t color) {
    c.drawLine(x-r,y,x+r,y,color); c.drawLine(x,y-r,x,y+r,color);
    if(r>4) { c.drawLine(x-2,y-2,x+2,y+2,color); c.drawLine(x-2,y+2,x+2,y-2,color); }
}
void motif(lgfx::LGFX_Sprite& c,int x,int y,int r,unsigned kind,std::uint32_t frame=0) {
    c.drawCircle(x,y,r,gold); c.drawCircle(x,y,r-3,panel);
    if(kind==0) { // Watching eye.
        c.drawEllipse(x,y,r-7,(r-7)/2,gold);
        c.fillCircle(x,y,std::max(2,r/4),gold);
        c.fillCircle(x+2,y-2,std::max(1,r/9),bg);
    } else if(kind==1) {
        c.fillCircle(x-2,y,r/2,gold); c.fillCircle(x+r/5,y-r/6,r/2,bg);
        star(c,x+r/3,y-r/3,std::max(2,r/6),ink);
    } else if(kind==2) {
        c.drawTriangle(x,y-r+8,x-r+8,y+r/2,x+r-8,y+r/2,gold);
        c.drawTriangle(x,y+r-8,x-r+8,y-r/2,x+r-8,y-r/2,muted);
    } else {
        c.drawCircle(x,y-3,r/2,gold);
        c.drawLine(x-r/2,y+r/2,x+r/2,y+r/2,gold);
        star(c,x+2,y-5,std::max(2,r/5),ink);
    }
    for(int i=0;i<8;++i) {
        float a=i*0.785398f+frame*0.0007f;
        int px=x+std::cos(a)*(r+6),py=y+std::sin(a)*(r+6);
        c.drawPixel(px,py,muted);
    }
}
void footer(lgfx::LGFX_Sprite& c,const std::string& label) {
    c.drawFastHLine(8,120,224,panel); center(c,kChinese?123:125,label,1,muted);
}
}
void draw(lgfx::LGFX_Sprite& c,const View& v,lgfx::LGFX_Sprite* image) {
    c.fillScreen(bg);
    text(c,8,5,"ORACLE / 06.0",1,gold);
    const bool knownBattery=v.batteryPercent>=0 && v.batteryPercent<=100;
    const auto batteryColor=!knownBattery?muted:v.batteryPercent<=20?0xFF786E:gold;
    c.drawRect(90,5,12,8,batteryColor);
    c.fillRect(102,7,2,4,batteryColor);
    if(knownBattery) c.fillRect(92,7,(v.batteryPercent*8+50)/100,4,batteryColor);
    char batteryLabel[5]="--%";
    if(knownBattery) std::snprintf(batteryLabel,sizeof(batteryLabel),"%d%%",v.batteryPercent);
    text(c,108,5,batteryLabel,1,batteryColor);
    if(v.printStatus==PrintStatus::Searching || v.printStatus==PrintStatus::Connecting) text(c,138,5,"BT...",1,muted);
    else if(v.printStatus==PrintStatus::Sending) text(c,138,5,"PRINT",1,gold);
    text(c,174,5,v.sd?"SD":"DEMO",1,muted);
    text(c,204,5,v.muted?"MUTE":v.playing?"VOICE":v.theme==Theme::Tarot?"AI":"LOCAL",1,muted);
    c.drawFastHLine(8,17,224,panel);
    if(v.screen==Screen::Idle) {
        motif(c,120,58,29,0,v.frame);
        star(c,48,51,6,gold); star(c,192,64,4,muted);
        center(c,kChinese?92:98,ORACLE_I18N("YOUR FUTURE AWAITS", "СУДЬБА ЖДЁТ", "你的未来在等你"),1,gold);
        center(c,kChinese?106:110,std::string("< ")+themeLabel(v.theme)+" >",1,ink);
        footer(c,ORACLE_I18N("Other keys: get a fortune", "Остальные клавиши: прогноз", "其他按键：抽签"));
    } else if(v.screen==Screen::Question) {
        text(c,10,24,ORACLE_I18N("ASK THE CARDS", "ВОПРОС К КАРТАМ", "询问塔罗牌"),1,gold);
        text(c,190,24,v.russianInput?"RU":"EN",1,muted);
        if(v.question.empty()) {
            text(c,10,48,ORACLE_I18N("Type your question", "Напиши свой вопрос", "输入英文或拼音问题"),1,muted);
            text(c,10,65,ORACLE_I18N("or Enter: a general reading", "или Enter: общий расклад", "或按回车：进行通用解读"),1,muted);
        } else {
            const auto rows=wrap(v.question+"_",18);
            const auto first=rows.size()>kVisibleLines?rows.size()-kVisibleLines:0;
            for(unsigned i=first;i<rows.size();++i) text(c,10,40+(i-first)*kLineHeight,rows[i],2);
        }
        footer(c,ORACLE_I18N("Enter: read  Tab: RU/EN  Esc: back", "Enter: гадать  Tab: RU/EN  Esc: назад", "Enter：阅读  Tab：RU/EN  Esc：返回"));
    } else if(v.screen==Screen::Ritual) {
        const auto elapsed=std::uint32_t(v.frame-v.started);
        motif(c,120,57,26+(elapsed/140)%5,elapsed/650%4,v.frame*4);
        const char* phrases[]={ORACLE_I18N("Shuffling possibilities...", "Тасуем вероятности...", "正在洗牌..."),ORACLE_I18N("Consulting the cat...", "Советуемся с котом...", "正在咨询猫咪…"),ORACLE_I18N("Checking with the stars...", "Уточняем у звёзд...", "正在向星辰求问…"),ORACLE_I18N("Future almost ready", "Будущее почти готово", "未来即将揭晓")};
        center(c,97,phrases[std::min(3u,elapsed/650)],1,gold);
        c.fillRect(20,113,std::min(200u,elapsed*200/2600),2,gold);
        footer(c,ORACLE_I18N("ESC: cancel ritual", "ESC: отменить ритуал", "ESC：取消仪式"));
    } else if(v.screen==Screen::Generating) {
        motif(c,120,53,24,(v.frame/700)%4,v.frame*3);
        center(c,88,ORACLE_I18N("THE CARDS ARE THINKING", "КАРТЫ СОВЕТУЮТСЯ", "牌正在思考"),1,gold);
        center(c,105,v.notice.empty()?ORACLE_I18N("Connecting...", "Подключаемся...", "正在连接…"):v.notice,1,ink);
        footer(c,ORACLE_I18N("ESC: cancel", "ESC: отменить", "ESC：取消"));
    } else if(v.screen==Screen::Result) {
        if(v.showArt || std::uint32_t(v.frame-v.started)<4300) {
            if(v.result.topic=="tarot") drawTarotCards(c,v.result,12,24,224,89,gold);
            else if(image) image->pushSprite(&c,74,22);
            else motif(c,120,65,32,v.result.motif);
            footer(c,v.result.title);
        } else {
            text(c,10,24,v.result.title,1,gold);
            star(c,221,27,4,gold);
            for(unsigned i=0;i<kVisibleLines && v.scroll+i<v.lines.size();++i)
                text(c,10,39+i*kLineHeight,v.lines[v.scroll+i],2);
            const auto maxScroll=v.lines.size()>kVisibleLines ? v.lines.size()-kVisibleLines : 0;
            if(maxScroll) {
                c.fillRect(232,39,2,72,panel);
                c.fillRect(232,39+int(v.scroll*60/maxScroll),2,12,gold);
            }
            const auto help=(v.frame/4000)%2 ? (v.result.topic=="tarot" ? ORACLE_I18N(";/.: scroll  N: new reading", ";/.: листать  N: новый расклад", ";/.: 滚动  N: 新占卜") : ORACLE_I18N(";/.: scroll  N: new  R: voice", ";/.: листать  N: ещё  R: голос", ";/.: 滚动  N: 新建  R: 语音")) : ORACLE_I18N("P: print  +/-: volume", "P: печать  +/-: громкость", "P：打印  +/-：音量");
            const bool printActive=v.printStatus==PrintStatus::Searching || v.printStatus==PrintStatus::Connecting || v.printStatus==PrintStatus::Sending;
            footer(c,!v.notice.empty()?v.notice:!v.printNotice.empty() && (printActive || (v.frame/4000)%2) ? v.printNotice:help);
        }
    } else {
        center(c,44,ORACLE_I18N("THE ORACLE NEEDS A MOMENT", "ОРАКУЛ ЗАДУМАЛСЯ", "神谕需要片刻"),1,gold);
        center(c,67,v.notice.empty()?ORACLE_I18N("Check the deck on SD", "Проверь колоду на SD", "检查 SD 卡中的牌组"):v.notice);
        footer(c,ORACLE_I18N("ENTER: retry  ESC: back", "ENTER: повторить  ESC: назад", "ENTER：重试  ESC：返回"));
    }
}
}
