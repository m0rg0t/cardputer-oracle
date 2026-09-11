#include "oracle/locale.h"
#include "oracle/bq95_printer.h"
#include "oracle/print_protocol.h"
#include "oracle/tarot_receipt.h"
#include <Arduino.h>
#include <SD.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include "oracle/diagnostics.h"

namespace oracle {
namespace {
enum class Error { None, NoMemory, Receipt, NotFound, Connect, Profile, State, Timeout, Link, Write, Overflow };
struct Job { std::unique_ptr<std::uint8_t[]> bytes; unsigned height=0; };
struct Notification { std::uint8_t channel=0, size=0, bytes[64]{}; };
constexpr std::uint8_t kManufacturer[]={0x13,0x03,0xaf,0x12,0x18,0x19};
}
struct Bq95Printer::Impl {
    QueueHandle_t jobs=nullptr, notifications=nullptr;
    std::atomic<PrintStatus> state{PrintStatus::Unavailable};
    std::atomic<Error> error{Error::None};
    std::atomic<bool> occupied{false}, overflow{false};
    std::atomic<unsigned> percent{0};
    NimBLEClient* client=nullptr;
    NimBLERemoteCharacteristic* writer=nullptr;
    bq95::Parser parsers[2];
    bool initialized=false, blocked=false;
    unsigned replies=0;
    std::uint32_t deadlineStart=0;

    struct ScanCallbacks : NimBLEAdvertisedDeviceCallbacks {
        std::atomic<bool> found{false};
        NimBLEAddress address;
        void onResult(NimBLEAdvertisedDevice* device) override {
            if(found) return;
            if(device->getName()!="BQ95") return;
            const auto manufacturer=device->getManufacturerData();
            const bool matchingData=manufacturer.size()==sizeof(kManufacturer) &&
                std::memcmp(manufacturer.data(),kManufacturer,sizeof(kManufacturer))==0;
            if(device->getAddress().toString()!="13:03:af:12:18:19" && !matchingData) return;
            address=device->getAddress(); found.store(true);
            // Let the bounded scan finish. In NimBLE 1.4, stop() with
            // maxResults=0 would free this callback's advertisedDevice early.
        }
    } scanCallbacks;

    void receive(unsigned channel, const std::uint8_t* data, std::size_t size) {
        while(size) {
            Notification n; n.channel=channel; n.size=std::min(size,sizeof(n.bytes));
            std::memcpy(n.bytes,data,n.size);
            if(xQueueSend(notifications,&n,0)!=pdTRUE) overflow=true;
            data+=n.size; size-=n.size;
        }
    }
    void drain() {
        Notification n;
        while(xQueueReceive(notifications,&n,0)==pdTRUE) {
            bq95::Frame frame;
            for(unsigned i=0;i<n.size;++i) if(parsers[n.channel].push(n.bytes[i],frame)) {
                if(frame.op==0xa3 && frame.size) {
                    ++replies;
                    if(!bq95::ready(frame.data[0])) { blocked=true; error=Error::State; }
                }
            }
        }
    }
    bool healthy() {
        drain();
        if(overflow) { error=Error::Overflow; return false; }
        if(blocked) return false;
        if(!client || !client->isConnected()) { error=Error::Link; return false; }
        if(std::uint32_t(millis()-deadlineStart)>60000) { error=Error::Timeout; return false; }
        return true;
    }
    bool write(const std::uint8_t* bytes, std::size_t size) {
        if(!writer || !size) return false;
        const auto mtu=client->getMTU();
        const std::size_t chunk=std::min<unsigned>(63,mtu>3?mtu-3:20);
        for(std::size_t offset=0;offset<size;) {
            if(!healthy()) return false;
            const auto count=std::min(chunk,size-offset);
            if(!writer->writeValue(bytes+offset,count,false)) { error=Error::Write; return false; }
            offset+=count;
            vTaskDelay(pdMS_TO_TICKS(12));
        }
        return healthy();
    }
    void progress(unsigned value) { percent=value; }
    bool query(unsigned waitMs) {
        drain(); const auto before=replies;
        std::uint8_t packet[9], zero=0;
        if(!write(packet,bq95::packet(0xa3,&zero,1,packet))) return false;
        const auto started=millis();
        while(std::uint32_t(millis()-started)<waitMs) {
            if(!healthy()) return false;
            if(replies!=before) return true;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        error=Error::Timeout; return false;
    }
    bool connect() {
        if(!initialized) {
            if(ESP.getFreeHeap()<90000) { error=Error::NoMemory; return false; }
            diagnosticMark(DiagnosticStage::Bluetooth);
            // tools/nimble_m5apps.py removes the library's mandatory NVS init.
            // Bond/CCCD persistence is disabled; BQ95 reconnects every job.
            NimBLEDevice::init("Oracle ADV");
            NimBLEDevice::setMTU(128);
            initialized=true;
        }
        if(!client) client=NimBLEDevice::createClient();
        if(!client) { error=Error::NoMemory; return false; }
        if(!client->isConnected()) {
            state=PrintStatus::Searching;
            auto scan=NimBLEDevice::getScan();
            scanCallbacks.found=false;
            scan->setAdvertisedDeviceCallbacks(&scanCallbacks);
            scan->setMaxResults(0); // Do not retain unrelated nearby devices.
            scan->setActiveScan(true); scan->setInterval(100); scan->setWindow(60);
            scan->start(3,false);
            if(!scanCallbacks.found) { error=Error::NotFound; return false; }
            state=PrintStatus::Connecting;
            client->setConnectTimeout(6);
            client->setConnectionParams(12,24,0,200);
            if(!client->connect(scanCallbacks.address)) { error=Error::Connect; return false; }
        }
        auto service=client->getService(NimBLEUUID(std::uint16_t(0xae30)));
        if(!service) { error=Error::Profile; return false; }
        writer=service->getCharacteristic(NimBLEUUID(std::uint16_t(0xae01)));
        auto notify=service->getCharacteristic(NimBLEUUID(std::uint16_t(0xae02)));
        if(!writer || !writer->canWriteNoResponse() || !notify || !notify->canNotify()) { error=Error::Profile; return false; }
        if(!notify->subscribe(true,[this](NimBLERemoteCharacteristic*,std::uint8_t* p,std::size_t n,bool){receive(0,p,n);},true)) {
            error=Error::Profile; return false;
        }
        auto extra=service->getCharacteristic(NimBLEUUID(std::uint16_t(0xae04)));
        if(extra && extra->canNotify() && !extra->subscribe(true,[this](NimBLERemoteCharacteristic*,std::uint8_t* p,std::size_t n,bool){receive(1,p,n);},true)) {
            error=Error::Profile; return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        // These retries only ask readiness; no paper is sent until a valid reply.
        for(unsigned i=0;i<3;++i) {
            if(query(2000)) { error=Error::None; return true; }
            if(blocked || !client->isConnected() || overflow) break;
        }
        return false;
    }
    void run(Job& job) {
        error=Error::None; percent=0; blocked=false; overflow=false; replies=0;
        parsers[0]={}; parsers[1]={};
        Notification old; while(xQueueReceive(notifications,&old,0)==pdTRUE) {}
        deadlineStart=millis();
        if(!connect()) state=(error==Error::NotFound ? PrintStatus::Unavailable : PrintStatus::Failed);
        else {
            state=PrintStatus::Sending;
            auto outcome=bq95::transmit(*this,job.bytes.get()+bq95::kReceiptHeader,job.height);
            if(outcome==bq95::Outcome::Sent) {
                vTaskDelay(pdMS_TO_TICKS(500));
                if(!query(2500)) outcome=bq95::Outcome::Uncertain;
            }
            state=outcome==bq95::Outcome::Sent ? PrintStatus::Sent :
                  outcome==bq95::Outcome::Uncertain ? PrintStatus::Uncertain : PrintStatus::Failed;
        }
        // Release the camera so FunPrint/the next request can reconnect cleanly.
        if(client && client->isConnected()) {
            client->disconnect();
            for(unsigned i=0;i<100 && client->isConnected();++i) vTaskDelay(pdMS_TO_TICKS(10));
        }
        writer=nullptr;
        Serial.printf("BQ95 status %u, error %u, heap %u\n",unsigned(state.load()),unsigned(error.load()),ESP.getFreeHeap());
    }
    static void worker(void* arg) {
        auto self=static_cast<Impl*>(arg);
        for(;;) {
            Job* pointer=nullptr;
            if(xQueueReceive(self->jobs,&pointer,portMAX_DELAY)==pdTRUE) {
                { std::unique_ptr<Job> job(pointer); self->run(*job); }
                self->occupied=false;
            }
        }
    }
};
bool Bq95Printer::begin() {
    impl_=new(std::nothrow) Impl;
    if(!impl_) return false;
    impl_->jobs=xQueueCreate(1,sizeof(Job*));
    impl_->notifications=xQueueCreate(16,sizeof(Notification));
    if(impl_->jobs && impl_->notifications && xTaskCreate(Impl::worker,"oracle-print",8192,impl_,1,nullptr)==pdPASS) return true;
    if(impl_->jobs) vQueueDelete(impl_->jobs);
    if(impl_->notifications) vQueueDelete(impl_->notifications);
    delete impl_; impl_=nullptr; return false;
}
PrintStatus Bq95Printer::status() const { return impl_?impl_->state.load():PrintStatus::Unavailable; }
bool Bq95Printer::busy() const { return impl_ && impl_->occupied; }
bool Bq95Printer::releaseRadio() {
    if(!impl_) return true;
    bool idle=false;
    if(!impl_->occupied.compare_exchange_strong(idle,true)) return false;
    if(impl_->initialized) {
        if(impl_->client && impl_->client->isConnected()) { impl_->occupied=false; return false; }
        // Delete the one client explicitly: NimBLE 1.4's clearAll loop modifies
        // its client list during iteration. With an empty list deinit is safe.
        if(impl_->client && !NimBLEDevice::deleteClient(impl_->client)) { impl_->occupied=false; return false; }
        impl_->client=nullptr; impl_->writer=nullptr;
        NimBLEDevice::deinit(true); impl_->initialized=NimBLEDevice::getInitialized();
        if(impl_->initialized) { impl_->occupied=false; return false; }
    }
    impl_->occupied=false; return true;
}
bool Bq95Printer::submit(const Prediction& result) {
    if(!impl_) return false;
    bool idle=false;
    if(!impl_->occupied.compare_exchange_strong(idle,true)) return false;
    resultId_=result.id;
    auto fail=[&](Error e) { impl_->error=e; impl_->state=PrintStatus::Failed; impl_->occupied=false; return false; };
    if(!validPrediction(result)) return fail(Error::Receipt);
    // SD access stays on the UI task; the BLE task owns a frozen RAM raster.
    // Voice streaming and JPEG reads therefore never race this SD File.
    std::unique_ptr<Job> job(new(std::nothrow) Job);
    if(!job) return fail(Error::NoMemory);
    std::size_t size=0;
    if(result.topic=="tarot") {
        if(!buildTarotReceipt(result,job->bytes,size,job->height)) return fail(Error::Receipt);
    } else {
        auto file=SD.open(("/ORACLE/print/"+result.id+".bin").c_str());
        if(!file || file.size()<bq95::kReceiptHeader || file.size()>bq95::kReceiptHeader+bq95::kRowBytes*bq95::kMaxHeight) return fail(Error::Receipt);
        size=file.size();
        job->bytes.reset(new(std::nothrow) std::uint8_t[size]);
        if(!job->bytes) return fail(Error::NoMemory);
        if(file.read(job->bytes.get(),size)!=size) return fail(Error::Receipt);
    }
    const auto content=receiptContent(result);
    const auto contentCrc=bq95::crc32(reinterpret_cast<const std::uint8_t*>(content.data()),content.size());
    if(!bq95::validReceipt(job->bytes.get(),size,contentCrc,job->height)) return fail(Error::Receipt);
    impl_->state=PrintStatus::Searching; impl_->error=Error::None; impl_->percent=0;
    Job* pointer=job.release();
    if(xQueueSend(impl_->jobs,&pointer,0)!=pdTRUE) { delete pointer; return fail(Error::NoMemory); }
    return true;
}
std::string Bq95Printer::notice() const {
    if(!impl_) return ORACLE_I18N("Printing unavailable", "Печать недоступна", "无法打印");
    switch(status()) {
        case PrintStatus::Searching: return ORACLE_I18N("Searching for BQ95...", "Ищем BQ95...", "正在搜索BQ95...");
        case PrintStatus::Connecting: return ORACLE_I18N("Connecting to BQ95...", "Подключаем BQ95...", "正在连接 BQ95…");
        case PrintStatus::Sending: return ORACLE_I18N("Printing: ", "Печать: ", "正在打印： ")+std::to_string(impl_->percent.load())+"%";
        case PrintStatus::Sent: return ORACLE_I18N("Receipt sent. P: copy", "Чек передан. P: ещё копия", "小票已发送。P：复制");
        case PrintStatus::Uncertain: return ORACLE_I18N("Check paper. P: retry", "Проверь бумагу. P: повтор", "检查纸张。P：重试");
        default: break;
    }
    switch(impl_->error.load()) {
        case Error::None: return ORACLE_I18N("P: print on BQ95", "P: печать на BQ95", "P：在BQ95上打印");
        case Error::NotFound: return ORACLE_I18N("BQ95 not found. P: search", "BQ95 не найден. P: поиск", "未找到 BQ95。P：搜索");
        case Error::Receipt: return ORACLE_I18N("No receipt: update ORACLE", "Нет чека: обнови ORACLE", "无小票：更新ORACLE");
        case Error::NoMemory: return ORACLE_I18N("Not enough memory to print", "Мало памяти для печати", "内存不足，无法打印");
        case Error::State: return ORACLE_I18N("Check camera and paper", "Проверь камеру и бумагу", "检查相机和纸张");
        case Error::Profile: return ORACLE_I18N("BQ95 service unavailable", "BQ95: сервис недоступен", "BQ95 服务不可用");
        case Error::Timeout: case Error::Connect: return ORACLE_I18N("Open FunPrint QR. P: retry", "Открой QR FunPrint. P: снова", "打开FunPrint二维码。P：重试");
        default: return ORACLE_I18N("BQ95 disconnected", "Связь с BQ95 прервалась", "BQ95 已断开");
    }
}
}
