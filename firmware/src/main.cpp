#include "oracle/locale.h"
#include "oracle/chinese_font.h"
#include "oracle/chinese_font_file.h"
#include <SD.h>
#include <M5Cardputer.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <memory>
#include <set>
#include "oracle/core.h"
#include "oracle/draw.h"
#include "oracle/wav.h"
#include "oracle/sound.h"
#include "oracle/input.h"
#include "oracle/bq95_printer.h"
#include "oracle/volume.h"
#include "oracle/tarot_client.h"
#include "oracle/tarot.h"
#include "oracle/diagnostics.h"

namespace {
using namespace oracle;
#if ORACLE_LANG_ZH
File chineseFont;
SemaphoreHandle_t chineseFontMutex=nullptr;
bool readSdChineseGlyph(std::uint16_t code,std::uint16_t* rows) {
    if(!chineseFontMutex || xSemaphoreTake(chineseFontMutex,pdMS_TO_TICKS(1000))!=pdTRUE) return false;
    struct Entry { std::uint16_t code=0, rows[12]{}; };
    static Entry cache[32];
    static unsigned next=0;
    for(const auto& entry:cache) if(entry.code==code) {
        std::memcpy(rows,entry.rows,24); xSemaphoreGive(chineseFontMutex); return true;
    }
    const bool found=readChineseFontFile(code,rows,[](unsigned offset,std::uint8_t* bytes,unsigned size) {
        return chineseFont && chineseFont.seek(offset) && chineseFont.read(bytes,size)==size;
    });
    if(found) { cache[next].code=code; std::memcpy(cache[next].rows,rows,24); next=(next+1)%32; }
    xSemaphoreGive(chineseFontMutex);
    return found;
}
#endif
std::uint8_t masterVolume=kDefaultVolume;
class SdCatalog final : public Catalog {
    std::vector<std::string> ids_;
    std::vector<unsigned char> themes_;
public:
    bool begin() {
        ids_.clear(); themes_.clear(); auto f=SD.open("/ORACLE/index.json");
        if(!f || f.size()>16384) return false;
        JsonDocument doc;
        const auto size=f.size();
        std::unique_ptr<char[]> bytes(new(std::nothrow) char[size]);
        if(!bytes || f.read(reinterpret_cast<std::uint8_t*>(bytes.get()),size)!=size) return false;
        if(deserializeJson(doc,bytes.get(),size) || doc["schema"].as<int>()!=1 || !doc["ids"].is<JsonArray>()) return false;
        if(!matchesPackLanguage(doc["language"].as<const char*>())) return false;
        auto list=doc["ids"].as<JsonArray>();
        if(list.size()<1 || list.size()>256) return false;
        std::set<std::string> unique;
        for(JsonVariant v:list) {
            if(!v.is<const char*>()) { ids_.clear(); return false; }
            std::string id=v.as<const char*>();
            if(!validId(id) || !unique.insert(id).second) { ids_.clear(); return false; }
            ids_.push_back(id);
        }
        const auto themes=doc["themes"].as<JsonArray>();
        if(themes.size()==ids_.size()) {
            for(JsonVariant value:themes) {
                if(!value.is<unsigned>() || value.as<unsigned>()>2) { themes_.clear(); break; }
                themes_.push_back(value.as<unsigned>());
            }
        }
        return true;
    }
    std::size_t size() const override { return ids_.size(); }
    bool matches(std::size_t i,Theme theme) override {
        if(i>=ids_.size()) return false;
        if(themes_.size()==ids_.size()) return themes_[i]==unsigned(theme);
        return Catalog::matches(i,theme); // Legacy SD index: load() yields between files.
    }
    bool load(std::size_t i,Prediction& p) override {
        delay(1);
        if(i>=ids_.size()) return false;
        auto path="/ORACLE/cards/"+ids_[i]+".json";
        auto f=SD.open(path.c_str()); if(!f || f.size()>4096) return false;
        JsonDocument doc;
        const auto size=f.size();
        std::unique_ptr<char[]> bytes(new(std::nothrow) char[size]);
        if(!bytes || f.read(reinterpret_cast<std::uint8_t*>(bytes.get()),size)!=size) return false;
        if(deserializeJson(doc,bytes.get(),size) || doc["schema"].as<int>()!=1) return false;
        p={doc["id"]|"",doc["title"]|"",doc["text"]|"",doc["topic"]|"",doc["image"]|"",doc["audio"]|"",doc["motif"]|99u};
        return p.id==ids_[i] && validPrediction(p);
    }
};
class AudioPlayer {
    File file_;
    WavInfo info_;
    std::uint32_t remaining_=0;
    std::int16_t buffers_[3][2048]{};
    unsigned next_=0;
    std::size_t pending_=0;
    bool draining_=false;
public:
    std::string error;
    void stop() {
        M5Cardputer.Speaker.stop(0);
        draining_=true;
        file_.close(); remaining_=0; pending_=0; next_=0; error.clear();
    }
    bool start(const std::string& path) {
        stop(); error.clear();
        if(path.empty()) { error=ORACLE_I18N("No voice for this card", "Для карточки нет голоса", "此卡无语音"); return false; }
        file_=SD.open(path.c_str());
        if(!file_ || !parseWav(file_,info_)) { file_.close(); error=ORACLE_I18N("WAV unavailable", "WAV недоступен", "WAV 不可用"); return false; }
        if(!M5Cardputer.Speaker.begin()) { file_.close(); error=ORACLE_I18N("Speaker error", "Ошибка динамика", "扬声器错误"); return false; }
        M5Cardputer.Speaker.setVolume(masterVolume);
        M5Cardputer.Speaker.setChannelVolume(0,255);
        remaining_=info_.bytes;
        return true;
    }
    bool playing() const { return remaining_ || pending_ || M5Cardputer.Speaker.isPlaying(0); }
    void update() {
        if(!file_) return;
        if(draining_) {
            if(M5Cardputer.Speaker.isPlaying(0)) return;
            draining_=false;
        }
        // Three buffers per M5Unified's runtime PCM contract; at most two queued.
        if(M5Cardputer.Speaker.isPlaying(0)>=2) return;
        if(!pending_ && remaining_) {
            const std::size_t n=std::min(std::uint32_t(sizeof(buffers_[0])),remaining_);
            const auto got=file_.read(reinterpret_cast<std::uint8_t*>(buffers_[next_]),n);
            if(got!=n) { stop(); error=ORACLE_I18N("WAV read error", "Ошибка чтения WAV", "WAV 读取错误"); return; }
            remaining_-=got; pending_=got/2;
            for(std::size_t i=0;i<pending_;++i) buffers_[next_][i]=loudVoice(buffers_[next_][i]);
        }
        if(pending_ && M5Cardputer.Speaker.playRaw(buffers_[next_],pending_,info_.rate,false,1,0,false)) {
            pending_=0; next_=(next_+1)%3;
        }
        if(!remaining_ && !pending_) file_.close();
    }
};
class EffectOutput {
public:
    bool ready() {
        diagnosticMark(DiagnosticStage::Sound);
        if(!M5Cardputer.Speaker.begin()) return false;
        M5Cardputer.Speaker.setChannelVolume(1,kEffectVolume);
        return true;
    }
    std::size_t queued() const { return M5Cardputer.Speaker.isPlaying(1); }
    void stop() { M5Cardputer.Speaker.stop(1); }
    bool submit(const std::int16_t* pcm,std::size_t count) {
        return M5Cardputer.Speaker.playRaw(pcm,count,kSoundRate,false,1,1,false);
    }
};
BuiltinCatalog builtin;
SdCatalog sdCatalog;
std::unique_ptr<DeckSource> source;
std::unique_ptr<Controller> app;
Bq95Printer printer;
TarotClient tarot;
AudioPlayer player;
EffectOutput effectOutput;
SoundPlayer<EffectOutput> effects(effectOutput);
NarrationGate narration;
M5Canvas canvas(&M5Cardputer.Display);
M5Canvas art;
bool artReady=false, boardReady=false, autoScroll=true;
std::uint32_t lastDraw=0, lastScroll=0, lastBattery=0;
KeyboardEdges keyboardEdges;

void updateBattery() {
    const int level=M5Cardputer.Power.getBatteryLevel();
    app->view.batteryPercent=(level>=0 && level<=100)?level:-1;
    lastBattery=millis();
}

void loadArt() {
    artReady=false;
    if(app->view.result.image.empty()) return;
    auto f=SD.open(app->view.result.image.c_str());
    if(!f || f.size()>65536) { app->view.notice=ORACLE_I18N("Image unavailable", "Картинка недоступна", "图像不可用"); return; }
    f.close();
    if(!art.getBuffer()) { art.setColorDepth(16); if(!art.createSprite(92,92)) return; }
    art.fillScreen(0x100E1B);
    artReady=art.drawJpgFile(SD,app->view.result.image.c_str(),0,0,92,92);
    if(!artReady) app->view.notice=ORACLE_I18N("Image damaged", "Картинка повреждена", "图像损坏");
}
void startVoice() {
    if(app->view.muted || app->view.result.topic=="tarot") return;
    if(!player.start(app->view.result.audio)) app->view.notice=player.error;
}
void trigger() {
    if(app->view.screen==Screen::Ritual || app->view.screen==Screen::Generating) return;
    narration.cancel();
    player.stop();
    diagnosticMark(DiagnosticStage::Select);
    if(app->trigger(millis())) {
        artReady=false;
        if(app->view.screen==Screen::Question) effects.play(Sound::Click);
        else {
            diagnosticMark(DiagnosticStage::Image);
            loadArt();
            app->view.started=millis();
            diagnosticMark(DiagnosticStage::Sound);
            effects.play(Sound::Ritual);
        }
    }
    else effects.play(Sound::Error);
}
void submitQuestion() {
    if(!app->submitQuestion(millis())) return;
    art.deleteSprite(); artReady=false;
    if(printer.busy() || !printer.releaseRadio()) app->failRemote(ORACLE_I18N("Wait for printing to finish", "Дождись окончания печати", "等待打印完成"));
    else if(!tarot.start(app->view.question)) app->failRemote(tarot.notice());
    else effects.play(Sound::Click);
}
void key(char k) {
    if(k=='`' || k==27) { tarot.cancel(); narration.cancel(); player.stop(); app->back(); effects.play(Sound::Cancel); }
    else if(k=='\n' || k==' ' || k=='n' || k=='N') trigger();
    else if(k==';' || k==',' || k=='w' || k=='W') {
        auto before=app->view.scroll; app->scroll(-1); autoScroll=false;
        if(before!=app->view.scroll) effects.play(Sound::Scroll);
    }
    else if(k=='.' || k=='s' || k=='S') {
        auto before=app->view.scroll; app->scroll(1); autoScroll=false;
        if(before!=app->view.scroll) effects.play(Sound::Scroll);
    }
    else if(k=='m' || k=='M') {
        app->view.muted=!app->view.muted; effects.mute(app->view.muted);
        if(app->view.muted) { narration.cancel(); player.stop(); }
        else effects.play(Sound::Click);
    }
    else if(k=='r' || k=='R') {
        if(app->view.screen==Screen::Result) {
            player.stop(); effects.stop(); effects.play(Sound::Click);
            app->view.notice.clear();
            if(app->view.result.topic=="tarot") app->view.notice=ORACLE_I18N("Tarot: text and print", "Таро: текст и печать", "塔罗：文字并打印");
            else narration.request();
        }
    }
    else if(k=='i' || k=='I') {
        if(app->view.screen==Screen::Result) { app->view.showArt=!app->view.showArt; effects.play(Sound::Click); }
    }
    else if(k=='a' || k=='A') { autoScroll=!autoScroll; lastScroll=millis(); effects.play(Sound::Click); }
    else if(k=='+' || k=='=' || k=='-') {
        masterVolume=std::max(0,std::min(255,int(masterVolume)+(k=='-'?-15:15)));
        M5Cardputer.Speaker.setVolume(masterVolume);
        app->view.notice=ORACLE_I18N("Volume: ", "Громкость: ", "音量： ")+std::to_string((unsigned(masterVolume)*100+127)/255)+"%";
        effects.play(Sound::Click);
    }
    else if(k=='p' || k=='P') {
        if(app->view.screen==Screen::Result) {
            app->view.notice.clear();
            if(tarot.busy()) app->view.notice=ORACLE_I18N("Finishing network request", "Завершаем сетевой запрос", "正在完成网络请求");
            else if(!printer.busy()) printer.submit(app->view.result);
            effects.play(Sound::Click);
        }
    }
}
}
void setup() {
    Serial.begin(115200);
    diagnosticBoot();
    auto config=M5.config(); config.internal_mic=false; config.internal_spk=true;
    M5Cardputer.begin(config,true);
    M5Cardputer.Speaker.setVolume(masterVolume);
    M5Cardputer.Display.setRotation(1); M5Cardputer.Display.setBrightness(160);
    canvas.setColorDepth(16);
    if(!canvas.createSprite(240,135)) { M5Cardputer.Display.println("No framebuffer memory"); return; }
    boardReady=M5.getBoard()==m5::board_t::board_M5CardputerADV;
    if(!boardReady) { M5Cardputer.Display.println("Cardputer ADV required"); return; }
    SPI.begin(40,39,14,12);
    bool mounted=SD.begin(12,SPI,20000000) && SD.cardType()!=CARD_NONE;
#if ORACLE_LANG_ZH
    if(mounted) {
        chineseFont=SD.open("/ORACLE/fonts/chinese12.bin");
        chineseFontMutex=xSemaphoreCreateMutex();
        setChineseGlyphReader(readSdChineseGlyph);
    }
#endif
    if(mounted) diagnosticSaveBoot();
    bool loaded=mounted && sdCatalog.begin();
    Catalog& catalog=loaded?static_cast<Catalog&>(sdCatalog):static_cast<Catalog&>(builtin);
    source.reset(new DeckSource(catalog,esp_random())); app.reset(new Controller(*source));
    app->view.sd=loaded;
    updateBattery();
    printer.begin();
    Serial.println(kBuildLanguageMarker);
    Serial.printf("Oracle ready: %u cards, SD pack %s, heap %u\n",unsigned(catalog.size()),loaded?"yes":"no",ESP.getFreeHeap());
}
void loop() {
    if(!boardReady || !app) {delay(50);return;}
    M5Cardputer.update();
    std::uint64_t heldKeys=0;
    for(const auto& pos:M5Cardputer.Keyboard.keyList()) heldKeys|=keyBit(pos.y,pos.x);
    const auto pressedKeys=keyboardEdges.update(heldKeys);
    const bool home=app->view.screen==Screen::Idle;
    if(M5Cardputer.BtnA.wasPressed()) {
        if(app->view.screen==Screen::Question) submitQuestion(); else trigger();
    }
    if(pressedKeys) {
        if(home) {
            const auto action=homeAction(pressedKeys);
            if(action==HomeAction::PreviousTheme || action==HomeAction::NextTheme) {
                if(app->changeTheme(action==HomeAction::PreviousTheme ? -1 : 1)) effects.play(Sound::Click);
            } else if(action==HomeAction::Start) trigger();
        }
        else {
            const auto& keys=M5Cardputer.Keyboard.keysState();
            if(app->view.screen==Screen::Question) {
                // Process newly pressed keys only, even when the preceding
                // letter/Shift is still held during normal two-finger typing.
                for(const auto& pos:M5Cardputer.Keyboard.keyList()) {
                    if(!(pressedKeys & keyBit(pos.y,pos.x))) continue;
                    const auto k=M5Cardputer.Keyboard.getKey(pos);
                    if(k==KEY_ENTER) { submitQuestion(); break; }
                    if(k=='`' || k==27) { key(k); break; }
                    if(k==KEY_BACKSPACE) { eraseQuestionKey(app->view.question); effects.play(Sound::Click); }
                    else if(k==KEY_TAB) { app->view.russianInput=!app->view.russianInput; effects.play(Sound::Click); }
                    else if(k<127 && appendQuestionKey(app->view.question,char(k),app->view.russianInput)) effects.play(Sound::Click);
                }
            } else if(keys.enter) key('\n'); else if(!keys.word.empty()) key(keys.word[0]);
        }
    }
    const auto now=millis();
    if(std::uint32_t(now-lastBattery)>=5000) updateBattery();
    Prediction remote; std::string remoteError;
    if(tarot.take(remote,remoteError) && app->view.screen==Screen::Generating) {
        if(remoteError.empty() && app->acceptRemote(remote,now)) effects.play(Sound::Ritual);
        else { app->failRemote(remoteError.empty()?ORACLE_I18N("Invalid reading", "Неверный расклад", "解读无效"):remoteError); effects.play(Sound::Error); }
    }
    if(app->view.screen==Screen::Generating) app->view.notice=tarot.notice();
    if(app->tick(now)) {
        effects.play(Sound::Reveal);
        if(app->view.result.topic!="tarot") narration.request();
        lastScroll=now+1700; autoScroll=true;
        if(!tarot.busy() && !printer.busy()) {
            diagnosticMark(DiagnosticStage::Receipt);
            printer.submit(app->view.result);
        }
    }
    app->view.printStatus=printer.status();
    app->view.printNotice=printer.resultId()==app->view.result.id ? printer.notice() :
        (printer.busy()?ORACLE_I18N("Printing previous receipt", "Печатаем предыдущий чек", "正在打印上次小票"):ORACLE_I18N("P: print on BQ95", "P: печать на BQ95", "P：在BQ95上打印"));
    effects.update();
    if(narration.update(now,effects.busy(),app->view.screen==Screen::Result && !app->view.muted)) startVoice();
    player.update(); app->view.playing=player.playing();
    if(!player.error.empty() && app->view.screen==Screen::Result) app->view.notice=player.error;
    // Rollover-safe elapsed times; manual scrolling disables the automatic reader.
    if(app->view.screen==Screen::Result && !app->view.showArt && autoScroll && std::uint32_t(now-app->view.started)>=4300 && std::uint32_t(now-lastScroll)>=3200) {
        app->scroll(1); lastScroll=now;
    }
    if(now-lastDraw>=40) {
        lastDraw=now;
        draw(canvas,app->view,artReady?&art:nullptr);
        canvas.pushSprite(0,0);
    }
    delay(2);
}
