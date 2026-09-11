#include "oracle/diagnostics.h"
#include <Arduino.h>
#include <SD.h>
#include <esp_system.h>
namespace oracle {
namespace {
RTC_NOINIT_ATTR std::uint32_t savedMagic;
RTC_NOINIT_ATTR std::uint32_t savedStage;
portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
unsigned previous=0,reason=0;
const char* label(unsigned stage) {
    static const char* labels[]={"boot","card selection","JPEG image","sound","receipt","Bluetooth init","Wi-Fi","clock","HTTPS"};
    return stage<sizeof(labels)/sizeof(labels[0])?labels[stage]:"unknown";
}
}
void diagnosticMark(DiagnosticStage stage) {
    portENTER_CRITICAL(&lock);
    savedStage=unsigned(stage); savedMagic=0x4f524135;
    portEXIT_CRITICAL(&lock);
    Serial.printf("Oracle stage: %s; heap %u; stack %u\n",label(unsigned(stage)),ESP.getFreeHeap(),unsigned(uxTaskGetStackHighWaterMark(nullptr)));
}
void diagnosticBoot() {
    reason=unsigned(esp_reset_reason());
    previous=(reason!=ESP_RST_POWERON && savedMagic==0x4f524135)?savedStage:0;
    Serial.printf("Oracle boot: reset %u; previous %s\n",reason,label(previous));
    diagnosticMark(DiagnosticStage::Boot);
}
void diagnosticSaveBoot() {
    auto f=SD.open("/ORACLE/diagnostic.txt",FILE_WRITE);
    if(f) f.printf("Oracle 0.6.0\nReset reason: %u\nPrevious stage: %s\nFree heap: %u\n",reason,label(previous),ESP.getFreeHeap());
}
}
