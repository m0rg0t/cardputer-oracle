#include <M5GFX.h>
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include "oracle/core.h"
#include "oracle/chinese_font.h"
#include "oracle/chinese_font_file.h"
#include "oracle/draw.h"
#include "oracle/tarot_api.h"
#include "oracle/tarot_receipt.h"
#include "oracle/print_protocol.h"

namespace {
M5GFX display;
lgfx::LGFX_Sprite canvas(&display);
std::string shots, artPath, tarotResponse;
lgfx::LGFX_Sprite art;
bool artReady=false;
void save(const std::string& name) {
    std::vector<lgfx::rgb888_t> pixels(240*135);
    canvas.readRect(0,0,240,135,pixels.data());
    const auto path=shots+"/"+name+".ppm";
    auto* f=fopen(path.c_str(),"wb");
    if(!f) { perror(path.c_str()); std::exit(1); }
    fprintf(f,"P6\n240 135\n255\n");
    for(const auto& p:pixels) { unsigned char rgb[]={p.r,p.g,p.b}; fwrite(rgb,1,3,f); }
    fclose(f);
}
int run(bool* running) {
    if(oracle::kChinese) oracle::setChineseGlyphReader([](std::uint16_t code,std::uint16_t* rows) {
        FILE* file=fopen("sdcard/ORACLE/fonts/chinese12.bin","rb");
        if(!file) return false;
        const bool found=oracle::readChineseFontFile(code,rows,[&](unsigned offset,std::uint8_t* bytes,unsigned size) {
            return fseek(file,offset,SEEK_SET)==0 && fread(bytes,1,size,file)==size;
        });
        fclose(file); return found;
    });
    display.init(); display.setRotation(1);
    canvas.setColorDepth(16); if(!canvas.createSprite(240,135)) return 1;
    oracle::BuiltinCatalog catalog;
    oracle::DeckSource source(catalog,42);
    oracle::Controller app(source);
    app.view.batteryPercent=76; // Simulated value; hardware uses M5Cardputer.Power.
    if(!artPath.empty()) {
        FILE* f=fopen(artPath.c_str(),"rb");
        if(f) {
            fseek(f,0,SEEK_END);const auto n=ftell(f);rewind(f);
            if(n>0 && n<65536) {
                std::vector<std::uint8_t> jpeg(n);fread(jpeg.data(),1,n,f);
                art.setColorDepth(16);art.createSprite(92,92);
                artReady=art.drawJpg(jpeg.data(),jpeg.size(),0,0,92,92);
            }
            fclose(f);
        }
    }
    if(!shots.empty()) {
        app.tick(100); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("01-idle");
        app.changeTheme(1); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("01b-idle-beer");
        app.changeTheme(1); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("01c-idle-tea");
        app.changeTheme(1);
        oracle::draw(canvas,app.view); save("11-idle-tarot");
        app.trigger(100); oracle::draw(canvas,app.view); save("12-question-empty");
        app.view.question=ORACLE_I18N("How will my weekend go?", "Как пройдут выходные?", "我的周末会怎样？"); oracle::draw(canvas,app.view); save("13-question");
        app.submitQuestion(100); app.view.notice=ORACLE_I18N("Waiting for the reading...", "Ждём толкование OpenAI...", "等待解读..."); app.tick(800);
        oracle::draw(canvas,app.view); save("14-generating");
        oracle::Prediction tarot;
        if(!tarotResponse.empty()) {
            std::ifstream file(tarotResponse);
            std::string raw((std::istreambuf_iterator<char>(file)),{});
            assert(oracle::parseTarotResponse(raw,{1,17,19},app.view.question,42,tarot));
        } else {
            assert(oracle::makeTarotPrediction({1,17,19},ORACLE_I18N("A quiet afternoon is approaching. You will finish that book, provided the cat agrees to give up its place on the last chapter.", "Тебя ждёт тихий вечер с книгой. Ты наконец дочитаешь последнюю главу, если кот согласится освободить страницу.", "宁静的午后将至。只要猫咪同意让出最后一章的位置，你就能读完那本书。"),42,tarot,app.view.question));
        }
        assert(app.acceptRemote(tarot,1000)); app.tick(3700); oracle::draw(canvas,app.view); save("15-tarot-cards");
        app.tick(5400); oracle::draw(canvas,app.view); save("16-tarot-reading");
        app.scroll(999); oracle::draw(canvas,app.view); save("17-tarot-last-lines");
        std::unique_ptr<std::uint8_t[]> receipt;
        std::size_t size=0; unsigned height=0,verifiedHeight=0;
        assert(oracle::buildTarotReceipt(tarot,receipt,size,height));
        auto content=oracle::receiptContent(tarot);
        auto crc=oracle::bq95::crc32(reinterpret_cast<const std::uint8_t*>(content.data()),content.size());
        assert(oracle::bq95::validReceipt(receipt.get(),size,crc,verifiedHeight) && verifiedHeight==height);
        std::ofstream(shots+"/tarot-receipt.bin",std::ios::binary).write(reinterpret_cast<const char*>(receipt.get()),size);
        tarot.question+="!"; content=oracle::receiptContent(tarot);
        crc=oracle::bq95::crc32(reinterpret_cast<const std::uint8_t*>(content.data()),content.size());
        assert(!oracle::bq95::validReceipt(receipt.get(),size,crc,verifiedHeight));
        app.back(); app.changeTheme(1);
        app.trigger(100); app.tick(1400); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("02-ritual");
        app.tick(2900); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("03-reveal");
        app.tick(5000); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("04-result");
        app.scroll(999); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("05-last-lines");
        app.view.notice.clear(); app.view.printStatus=oracle::PrintStatus::Sending;
        app.view.printNotice=ORACLE_I18N("Printing: 42%", "Печать: 42%", "正在打印：42%"); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("06-printing");
        app.view.printStatus=oracle::PrintStatus::Uncertain;app.view.printNotice=ORACLE_I18N("Check paper. P: retry", "Проверь бумагу. P: повтор", "检查纸张。P：重试");
        oracle::draw(canvas,app.view,artReady?&art:nullptr);save("06b-print-uncertain");
        app.view.printNotice.clear();app.view.printStatus=oracle::PrintStatus::Unavailable;
        app.view.screen=oracle::Screen::Error; app.view.notice=ORACLE_I18N("Deck unavailable", "Колода недоступна", "牌组不可用"); oracle::draw(canvas,app.view,artReady?&art:nullptr); save("07-error");
        app.back(); app.changeTheme(1); app.trigger(10000); app.tick(15000);
        oracle::draw(canvas,app.view,artReady?&art:nullptr); save("08-beer-result");
        app.back(); app.changeTheme(1); app.trigger(20000); app.tick(25000);
        oracle::draw(canvas,app.view,artReady?&art:nullptr); save("09-tea-result");
        app.view.notice=ORACLE_I18N("Volume: 100%", "Громкость: 100%", "音量：100%");oracle::draw(canvas,app.view,artReady?&art:nullptr);save("10-volume");
        app.view.screen=oracle::Screen::Idle; app.view.printStatus=oracle::PrintStatus::Sending;
        app.view.playing=true; app.view.sd=false; app.view.batteryPercent=100;
        oracle::draw(canvas,app.view); save("18-battery-full-busy");
        app.view.printStatus=oracle::PrintStatus::Searching; app.view.sd=true; app.view.batteryPercent=20;
        oracle::draw(canvas,app.view); save("19-battery-low");
        app.view.batteryPercent=0; app.view.muted=true;
        oracle::draw(canvas,app.view); save("20-battery-empty");
        app.view.batteryPercent=-1;
        oracle::draw(canvas,app.view); save("21-battery-unknown");
        std::exit(0);
    }
    static_cast<lgfx::Panel_sdl*>(display.panel())->setWindowTitle("Oracle ADV | Home: left/right theme, other keys start; result: Enter, arrows, M, Esc; Q: quit");
    Uint8 previous[SDL_NUM_SCANCODES]{};
    while(*running) {
        auto* keys=SDL_GetKeyboardState(nullptr);
        auto hit=[&](SDL_Scancode k){return keys[k]&&!previous[k];};
        bool anyKey=false;
        for(int i=1;i<SDL_NUM_SCANCODES;++i) anyKey|=hit(static_cast<SDL_Scancode>(i));
        if(app.view.screen==oracle::Screen::Idle && anyKey) {
            const bool left=hit(SDL_SCANCODE_LEFT)||hit(SDL_SCANCODE_COMMA);
            const bool right=hit(SDL_SCANCODE_RIGHT)||hit(SDL_SCANCODE_SLASH);
            if(left!=right) app.changeTheme(left?-1:1);
            else if(!left && !right) app.trigger(SDL_GetTicks());
        }
        else {
            if(hit(SDL_SCANCODE_Q)) break;
            if(hit(SDL_SCANCODE_RETURN)||hit(SDL_SCANCODE_SPACE)||hit(SDL_SCANCODE_N)) app.trigger(SDL_GetTicks());
            if(hit(SDL_SCANCODE_ESCAPE)) app.back();
            if(hit(SDL_SCANCODE_UP)) app.scroll(-1);
            if(hit(SDL_SCANCODE_DOWN)) app.scroll(1);
            if(hit(SDL_SCANCODE_I)) app.view.showArt=!app.view.showArt;
            if(hit(SDL_SCANCODE_M)) app.view.muted=!app.view.muted;
            if(hit(SDL_SCANCODE_P)) app.view.notice=ORACLE_I18N("Test BLE on the device", "BLE проверяется на устройстве", "在设备上测试 BLE");
            if(hit(SDL_SCANCODE_R)) app.view.notice=ORACLE_I18N("Test voice on the device", "Голос проверяется на устройстве", "在设备上测试语音");
        }
        std::memcpy(previous,keys,sizeof(previous));
        app.tick(SDL_GetTicks());
        oracle::draw(canvas,app.view,artReady?&art:nullptr); canvas.pushSprite(0,0); SDL_Delay(33);
    }
    return 0;
}
}
int main(int argc,char** argv) {
    for(int i=1;i+1<argc;++i) {
        if(!std::strcmp(argv[i],"--shots")) shots=argv[++i];
        else if(!std::strcmp(argv[i],"--art")) artPath=argv[++i];
        else if(!std::strcmp(argv[i],"--tarot-response")) tarotResponse=argv[++i];
    }
    return lgfx::Panel_sdl::main(run,128);
}
