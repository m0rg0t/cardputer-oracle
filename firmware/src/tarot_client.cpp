#include "oracle/locale.h"
#include "oracle/tarot_client.h"
#include "oracle/tarot.h"
#include "oracle/tarot_api.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include "oracle/diagnostics.h"
#include <atomic>
#include <algorithm>
#include <ctime>
#include <new>

namespace oracle {
namespace {
enum class Stage { Scanning, Connecting, Clock, Request, Done };
struct Network { std::string ssid, password; };
bool safeHost(const std::string& value) {
    if(value.empty() || value.size()>253 || value.find('.')==std::string::npos) return false;
    for(char c:value) if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='-')) return false;
    return true;
}
struct BoundedBody : Stream {
    std::string body;
    std::atomic<bool>& cancel;
    const std::uint32_t began=millis();
    explicit BoundedBody(std::atomic<bool>& stop):cancel(stop) { body.reserve(2048); }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    std::size_t write(std::uint8_t value) override { return write(&value,1); }
    std::size_t write(const std::uint8_t* data,std::size_t size) override {
        if(cancel || std::uint32_t(millis()-began)>40000 || body.size()+size>16384) return 0;
        body.append(reinterpret_cast<const char*>(data),size); return size;
    }
};
}
struct TarotClient::Impl {
    std::vector<Network> networks;
    std::string host, token, ca, model, question, error;
    Prediction result;
    std::array<unsigned,3> cards{};
    std::atomic<bool> cancelled{false}, done{false};
    std::atomic<Stage> stage{Stage::Connecting};
    std::atomic<unsigned> wifiReason{0};
    bool eventsRegistered=false;
    bool active=false; // UI-owned; done publishes all other result fields.
    bool load() {
        auto file=SD.open("/ORACLE/config.json");
        if(!file || file.size()>4096) { error=ORACLE_I18N("Need ORACLE/config.json", "Нужен ORACLE/config.json", "需要 ORACLE/config.json"); return false; }
        JsonDocument doc;
        if(deserializeJson(doc,file) || doc["schema"]!=1 || !doc["wifi"].is<JsonArray>()) {
            error=ORACLE_I18N("Check SD configuration", "Проверь конфиг на SD", "检查 SD 配置"); return false;
        }
        const auto wifi=doc["wifi"].as<JsonArray>();
        if(wifi.size()<1 || wifi.size()>4) { error=ORACLE_I18N("Wi-Fi settings required", "Нужны настройки Wi-Fi", "需要设置 Wi-Fi"); return false; }
        networks.clear();
        for(JsonObject item:wifi) {
            if(!item["ssid"].is<const char*>() || !item["password"].is<const char*>()) { error=ORACLE_I18N("Check Wi-Fi settings", "Проверь настройки Wi-Fi", "检查 Wi-Fi 设置"); return false; }
            Network network{item["ssid"].as<std::string>(),item["password"].as<std::string>()};
            if(network.ssid.empty() || network.ssid.size()>32 || (!network.password.empty() && (network.password.size()<8 || network.password.size()>63))) {
                error=ORACLE_I18N("Check Wi-Fi settings", "Проверь настройки Wi-Fi", "检查 Wi-Fi 设置"); return false;
            }
            networks.push_back(std::move(network));
        }
        host=doc["tarot"]["host"]|"";
        token=doc["tarot"]["token"]|"";
        model=doc["tarot"]["model"]|"openai/gpt-5.6-luna";
        if(!safeHost(host) || model.empty() || model.size()>96 || token.size()<16 || token.size()>256) { error=ORACLE_I18N("Set up LiteLLM on SD", "Настрой LiteLLM на SD", "在 SD 卡上设置 LiteLLM"); return false; }
        for(char c:token) if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-')) {
            error=ORACLE_I18N("Check Tarot token", "Проверь токен Таро", "检查塔罗令牌"); return false;
        }
        file.close();
        file=SD.open("/ORACLE/tls/roots.pem");
        if(!file || file.size()<512 || file.size()>24000) { error=ORACLE_I18N("No TLS certificate on SD", "Нет TLS-сертификата на SD", "SD卡无TLS证书"); return false; }
        ca.resize(file.size());
        if(file.read(reinterpret_cast<std::uint8_t*>(&ca[0]),ca.size())!=ca.size() || ca.find("-----BEGIN CERTIFICATE-----")!=0) {
            error=ORACLE_I18N("Check TLS certificate", "Проверь TLS-сертификат", "检查 TLS 证书"); return false;
        }
        return true;
    }
    bool connectWifi() {
        diagnosticMark(DiagnosticStage::Wifi);
        if(!eventsRegistered) {
            WiFi.onEvent([this](WiFiEvent_t,WiFiEventInfo_t info) {
                wifiReason=info.wifi_sta_disconnected.reason;
                Serial.printf("Oracle Wi-Fi disconnect reason %u\n",wifiReason.load());
            },ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
            eventsRegistered=true;
        }
        WiFi.persistent(false); WiFi.setAutoReconnect(false);
        if(!WiFi.mode(WIFI_STA)) { error=ORACLE_I18N("Wi-Fi radio failed", "Wi-Fi: радио не запустилось", "Wi-Fi 无线模块失败"); return false; }
        WiFi.setSleep(false);
        WiFi.disconnect(false,false);
        vTaskDelay(pdMS_TO_TICKS(150));
        stage=Stage::Scanning;
        WiFi.scanNetworks(true,true);
        const auto began=millis();
        while(!cancelled && WiFi.scanComplete()==WIFI_SCAN_RUNNING && std::uint32_t(millis()-began)<8000) vTaskDelay(pdMS_TO_TICKS(50));
        const auto found=WiFi.scanComplete();
        if(found==WIFI_SCAN_RUNNING) esp_wifi_scan_stop();
        if(cancelled) { WiFi.scanDelete(); return false; }
        std::vector<unsigned> candidates;
        for(unsigned i=0;i<networks.size();++i) for(int j=0;j<found;++j) if(WiFi.SSID(j).c_str()==networks[i].ssid) {
            candidates.push_back(i); break;
        }
        WiFi.scanDelete();
        const bool visible=!candidates.empty();
        for(unsigned i=0;i<networks.size();++i)
            if(std::find(candidates.begin(),candidates.end(),i)==candidates.end()) candidates.push_back(i);
        Serial.printf("Oracle Wi-Fi scan %d, saved network visible %u\n",found,unsigned(visible));
        stage=Stage::Connecting;
        for(const auto index:candidates) {
            if(cancelled) return false;
            const auto& network=networks[index];
            wifiReason=0;
            WiFi.begin(network.ssid.c_str(),network.password.c_str());
            const auto attempt=millis();
            while(!cancelled && WiFi.status()!=WL_CONNECTED && std::uint32_t(millis()-attempt)<25000) vTaskDelay(pdMS_TO_TICKS(50));
            if(WiFi.status()==WL_CONNECTED) return true;
            const auto reason=wifiReason.load();
            Serial.printf("Oracle Wi-Fi failed: status %u, reason %u\n",unsigned(WiFi.status()),reason);
            if(reason==WIFI_REASON_AUTH_FAIL || reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || reason==WIFI_REASON_HANDSHAKE_TIMEOUT)
                error=ORACLE_I18N("Wi-Fi: check password / WPA2", "Wi-Fi: проверь пароль/WPA2", "Wi-Fi：检查密码/WPA2");
            else if(!visible && (reason==WIFI_REASON_NO_AP_FOUND || found>=0)) error=ORACLE_I18N("No Wi-Fi: check name / 2.4GHz", "Нет сети: проверь имя/2.4GHz", "无Wi‑Fi：检查名称/2.4GHz");
            else error=ORACLE_I18N("Wi-Fi: failed, code ", "Wi-Fi: нет связи, код ", "Wi-Fi：失败，代码 ")+std::to_string(reason);
            WiFi.disconnect(false,false);
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        return false;
    }
    bool clockReady() {
        diagnosticMark(DiagnosticStage::Clock);
        stage=Stage::Clock;
        configTime(0,0,"time.cloudflare.com","pool.ntp.org","time.google.com");
        const auto began=millis();
        while(!cancelled && time(nullptr)<1767225600 && std::uint32_t(millis()-began)<12000) vTaskDelay(pdMS_TO_TICKS(50));
        if(time(nullptr)>=1767225600) return true;
        error=ORACLE_I18N("Could not sync the clock", "Не удалось сверить время", "无法同步时钟"); return false;
    }
    bool request() {
        diagnosticMark(DiagnosticStage::Https);
        stage=Stage::Request;
        WiFiClientSecure tls;
        tls.setCACert(ca.c_str()); tls.setHandshakeTimeout(12);
        HTTPClient http;
        http.setConnectTimeout(15000); http.setTimeout(35000);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); http.setReuse(false);
        const auto url="https://"+host+"/v1/responses";
        if(!http.begin(tls,url.c_str())) { error=ORACLE_I18N("Could not open HTTPS", "Не удалось открыть HTTPS", "无法打开 HTTPS"); return false; }
        http.addHeader("Authorization",("Bearer "+token).c_str());
        http.addHeader("Content-Type","application/json");
        std::string body=tarotApiRequest(model,cards,question);
        if(body.empty()) { error=ORACLE_I18N("Check question and model", "Проверь вопрос и модель", "检查问题和模型"); http.end(); return false; }
        // Exactly one POST; a timeout does not authorize a second paid request.
        const auto status=http.POST(reinterpret_cast<std::uint8_t*>(&body[0]),body.size());
        BoundedBody response(cancelled);
        const bool received=!cancelled && status>0 && http.getSize()<=16384 && http.writeToStream(&response)>=0;
        http.end(); tls.stop();
        if(cancelled) return false;
        if(status<0 || !received) { error=ORACLE_I18N("No server response / TLS", "Нет ответа сервера / TLS", "服务器无响应/TLS"); return false; }
        if(status!=200) {
            if(status==401 || status==403) error=ORACLE_I18N("Check token on SD", "Проверь токен на SD", "检查 SD 卡中的令牌");
            else if(status==429) error=ORACLE_I18N("Rate limit. Try later", "Лимит запросов. Попробуй позже", "请求受限，请稍后再试");
            else if(status==400 || status==404) error=ORACLE_I18N("Check LiteLLM model", "Проверь модель в LiteLLM", "检查 LiteLLM 模型");
            else if(status==503 || status==409) error=ORACLE_I18N("Server busy. Try later", "Сервер занят. Попробуй позже", "服务器忙，请稍后再试");
            else error=ORACLE_I18N("No response from LiteLLM", "LiteLLM не получил ответ", "LiteLLM无响应");
            return false;
        }
        if(!parseTarotResponse(response.body,cards,question,esp_random(),result)) {
            error=ORACLE_I18N("Incomplete reading. Retry", "Неполный расклад. Повтори", "解读不完整。重试"); return false;
        }
        return true;
    }
    static void worker(void* data) {
        auto& self=*static_cast<Impl*>(data);
        if(self.connectWifi() && !self.cancelled && self.clockReady() && !self.cancelled) self.request();
        WiFi.disconnect(true,false); WiFi.mode(WIFI_OFF);
        if(self.cancelled) { self.result={}; self.error=ORACLE_I18N("Request cancelled", "Запрос отменён", "请求已取消"); }
        self.networks.clear(); self.token.clear(); self.ca.clear();
        self.stage=Stage::Done;
        self.done.store(true); // No access to self after this publication.
        vTaskDelete(nullptr);
    }
};
bool TarotClient::start(const std::string& question) {
    startError_.clear();
    if(busy()) { startError_=ORACLE_I18N("Finishing previous request", "Завершаем предыдущий запрос", "正在完成上次请求"); return false; }
    if(!impl_) impl_=new(std::nothrow) Impl;
    if(!impl_) { startError_=ORACLE_I18N("Not enough memory for Tarot", "Мало памяти для Таро", "内存不足，无法占卜"); return false; }
    if(!validQuestion(question)) { startError_=ORACLE_I18N("Check your question", "Проверь вопрос", "检查你的问题"); return false; }
    auto& s=*impl_;
    s.question=question;
    s.error.clear(); s.result={}; s.cancelled=false; s.done=false;
    if(!s.load()) { startError_=s.error; return false; }
    if(ESP.getFreeHeap()<100000) { startError_=ORACLE_I18N("Not enough memory for Wi-Fi", "Мало памяти для сети", "内存不足，无法连接Wi‑Fi"); return false; }
    s.cards=drawArcana(esp_random());
    s.stage=Stage::Connecting; s.active=true;
    if(xTaskCreate(Impl::worker,"oracle-tarot",12288,&s,1,nullptr)==pdPASS) return true;
    s.active=false; startError_=ORACLE_I18N("Could not start Wi-Fi", "Не удалось запустить сеть", "无法启动 Wi-Fi"); return false;
}
void TarotClient::cancel() { if(busy()) impl_->cancelled=true; }
bool TarotClient::busy() const { return impl_ && impl_->active; }
bool TarotClient::take(Prediction& result,std::string& error) {
    if(!busy() || !impl_->done.load()) return false;
    result=std::move(impl_->result); error=std::move(impl_->error); impl_->active=false;
    return true;
}
std::string TarotClient::notice() const {
    if(!startError_.empty()) return startError_;
    if(!impl_) return ORACLE_I18N("Set up Tarot on SD", "Настрой Таро на SD", "在 SD 卡上设置 Tarot");
    if(impl_->cancelled) return ORACLE_I18N("Finishing request...", "Завершаем запрос...", "正在完成请求…");
    switch(impl_->stage.load()) {
        case Stage::Scanning:return ORACLE_I18N("Finding 2.4GHz network...", "Ищем сеть 2.4GHz...", "正在查找 2.4GHz 网络…");
        case Stage::Connecting:return ORACLE_I18N("Connecting Wi-Fi, up to 25s", "Подключаем Wi-Fi, до 25 с", "正在连接 Wi-Fi，最多 25 秒");
        case Stage::Clock:return ORACLE_I18N("Syncing the clock...", "Сверяем время...", "正在同步时钟...");
        case Stage::Request:return ORACLE_I18N("Waiting for the reading...", "Ждём толкование OpenAI...", "等待解读...");
        default:return ORACLE_I18N("Reading ready", "Расклад готов", "解读完成");
    }
}
}
