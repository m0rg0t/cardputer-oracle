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
        if(!file || file.size()>4096) { error="Нужен ORACLE/config.json"; return false; }
        JsonDocument doc;
        if(deserializeJson(doc,file) || doc["schema"]!=1 || !doc["wifi"].is<JsonArray>()) {
            error="Проверь конфиг на SD"; return false;
        }
        const auto wifi=doc["wifi"].as<JsonArray>();
        if(wifi.size()<1 || wifi.size()>4) { error="Нужны настройки Wi-Fi"; return false; }
        networks.clear();
        for(JsonObject item:wifi) {
            if(!item["ssid"].is<const char*>() || !item["password"].is<const char*>()) { error="Проверь настройки Wi-Fi"; return false; }
            Network network{item["ssid"].as<std::string>(),item["password"].as<std::string>()};
            if(network.ssid.empty() || network.ssid.size()>32 || (!network.password.empty() && (network.password.size()<8 || network.password.size()>63))) {
                error="Проверь настройки Wi-Fi"; return false;
            }
            networks.push_back(std::move(network));
        }
        host=doc["tarot"]["host"]|"";
        token=doc["tarot"]["token"]|"";
        model=doc["tarot"]["model"]|"openai/gpt-5.6-luna";
        if(!safeHost(host) || model.empty() || model.size()>96 || token.size()<16 || token.size()>256) { error="Настрой LiteLLM на SD"; return false; }
        for(char c:token) if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-')) {
            error="Проверь токен Таро"; return false;
        }
        file.close();
        file=SD.open("/ORACLE/tls/roots.pem");
        if(!file || file.size()<512 || file.size()>24000) { error="Нет TLS-сертификата на SD"; return false; }
        ca.resize(file.size());
        if(file.read(reinterpret_cast<std::uint8_t*>(&ca[0]),ca.size())!=ca.size() || ca.find("-----BEGIN CERTIFICATE-----")!=0) {
            error="Проверь TLS-сертификат"; return false;
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
        if(!WiFi.mode(WIFI_STA)) { error="Wi-Fi: радио не запустилось"; return false; }
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
                error="Wi-Fi: проверь пароль/WPA2";
            else if(!visible && (reason==WIFI_REASON_NO_AP_FOUND || found>=0)) error="Нет сети: проверь имя/2.4GHz";
            else error="Wi-Fi: нет связи, код "+std::to_string(reason);
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
        error="Не удалось сверить время"; return false;
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
        if(!http.begin(tls,url.c_str())) { error="Не удалось открыть HTTPS"; return false; }
        http.addHeader("Authorization",("Bearer "+token).c_str());
        http.addHeader("Content-Type","application/json");
        std::string body=tarotApiRequest(model,cards,question);
        if(body.empty()) { error="Проверь вопрос и модель"; http.end(); return false; }
        // Exactly one POST; a timeout does not authorize a second paid request.
        const auto status=http.POST(reinterpret_cast<std::uint8_t*>(&body[0]),body.size());
        BoundedBody response(cancelled);
        const bool received=!cancelled && status>0 && http.getSize()<=16384 && http.writeToStream(&response)>=0;
        http.end(); tls.stop();
        if(cancelled) return false;
        if(status<0 || !received) { error="Нет ответа сервера / TLS"; return false; }
        if(status!=200) {
            if(status==401 || status==403) error="Проверь токен на SD";
            else if(status==429) error="Лимит запросов. Попробуй позже";
            else if(status==400 || status==404) error="Проверь модель в LiteLLM";
            else if(status==503 || status==409) error="Сервер занят. Попробуй позже";
            else error="LiteLLM не получил ответ";
            return false;
        }
        if(!parseTarotResponse(response.body,cards,question,esp_random(),result)) {
            error="Неполный расклад. Повтори"; return false;
        }
        return true;
    }
    static void worker(void* data) {
        auto& self=*static_cast<Impl*>(data);
        if(self.connectWifi() && !self.cancelled && self.clockReady() && !self.cancelled) self.request();
        WiFi.disconnect(true,false); WiFi.mode(WIFI_OFF);
        if(self.cancelled) { self.result={}; self.error="Запрос отменён"; }
        self.networks.clear(); self.token.clear(); self.ca.clear();
        self.stage=Stage::Done;
        self.done.store(true); // No access to self after this publication.
        vTaskDelete(nullptr);
    }
};
bool TarotClient::start(const std::string& question) {
    startError_.clear();
    if(busy()) { startError_="Завершаем предыдущий запрос"; return false; }
    if(!impl_) impl_=new(std::nothrow) Impl;
    if(!impl_) { startError_="Мало памяти для Таро"; return false; }
    if(!validQuestion(question)) { startError_="Проверь вопрос"; return false; }
    auto& s=*impl_;
    s.question=question;
    s.error.clear(); s.result={}; s.cancelled=false; s.done=false;
    if(!s.load()) { startError_=s.error; return false; }
    if(ESP.getFreeHeap()<100000) { startError_="Мало памяти для сети"; return false; }
    s.cards=drawArcana(esp_random());
    s.stage=Stage::Connecting; s.active=true;
    if(xTaskCreate(Impl::worker,"oracle-tarot",12288,&s,1,nullptr)==pdPASS) return true;
    s.active=false; startError_="Не удалось запустить сеть"; return false;
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
    if(!impl_) return "Настрой Таро на SD";
    if(impl_->cancelled) return "Завершаем запрос...";
    switch(impl_->stage.load()) {
        case Stage::Scanning:return "Ищем сеть 2.4GHz...";
        case Stage::Connecting:return "Подключаем Wi-Fi, до 25 с";
        case Stage::Clock:return "Сверяем время...";
        case Stage::Request:return "Ждём толкование OpenAI...";
        default:return "Расклад готов";
    }
}
}
