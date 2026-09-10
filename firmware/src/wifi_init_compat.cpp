// Same M5Apps-compatible Wi-Fi initialization as the local Hermes terminal.
#include <esp_err.h>
#include <esp_wifi.h>
extern "C" esp_err_t __real_esp_wifi_init(const wifi_init_config_t* config);
extern "C" esp_err_t __wrap_esp_wifi_init(const wifi_init_config_t* config) {
    if(!config) return ESP_ERR_INVALID_ARG;
    auto compatible=*config;
    compatible.nvs_enable=0; // Credentials live on SD; the launcher owns NVS.
    return __real_esp_wifi_init(&compatible);
}
