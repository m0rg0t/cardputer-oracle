#pragma once
namespace oracle {
enum class DiagnosticStage { Boot, Select, Image, Sound, Receipt, Bluetooth, Wifi, Clock, Https };
void diagnosticBoot();
void diagnosticSaveBoot();
void diagnosticMark(DiagnosticStage stage);
}
