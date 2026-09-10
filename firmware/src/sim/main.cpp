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
        app.view.question="Как пройдёт мой доклад?"; oracle::draw(canvas,app.view); save("13-question");
        app.submitQuestion(100); app.view.notice="Ждём толкование OpenAI..."; app.tick(800);
        oracle::draw(canvas,app.view); save("14-generating");
        oracle::Prediction tarot;
        if(!tarotResponse.empty()) {
            std::ifstream file(tarotResponse);
            std::string raw((std::istreambuf_iterator<char>(file)),{});
            assert(oracle::parseTarotResponse(raw,{1,17,19},app.view.question,42,tarot));
        } else {
            assert(oracle::makeTarotPrediction({1,17,19},"Сегодня даже строгий слушатель улыбнётся: твоя магия сработает, особенно если не забудешь включить микрофон.",42,tarot,app.view.question));
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
        app.view.printNotice="Печать: 42%"; oracle::draw(canvas,app.view,artReady?&art:nullptr); save("06-printing");
        app.view.printStatus=oracle::PrintStatus::Uncertain;app.view.printNotice="Проверь бумагу. P: повтор";
        oracle::draw(canvas,app.view,artReady?&art:nullptr);save("06b-print-uncertain");
        app.view.printNotice.clear();app.view.printStatus=oracle::PrintStatus::Unavailable;
        app.view.screen=oracle::Screen::Error; app.view.notice="Колода недоступна"; oracle::draw(canvas,app.view,artReady?&art:nullptr); save("07-error");
        app.back(); app.changeTheme(1); app.trigger(10000); app.tick(15000);
        oracle::draw(canvas,app.view,artReady?&art:nullptr); save("08-beer-result");
        app.back(); app.changeTheme(1); app.trigger(20000); app.tick(25000);
        oracle::draw(canvas,app.view,artReady?&art:nullptr); save("09-tea-result");
        app.view.notice="Громкость: 100%";oracle::draw(canvas,app.view,artReady?&art:nullptr);save("10-volume");
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
            if(hit(SDL_SCANCODE_P)) app.view.notice="BLE проверяется на устройстве";
            if(hit(SDL_SCANCODE_R)) app.view.notice="Голос проверяется на устройстве";
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
