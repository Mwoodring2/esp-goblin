#include <Arduino.h>
#include <Wire.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include "boards/hosyond_es3c28p.h"

static void printBytes(const char* label, uint64_t bytes) {
    Serial.printf("%s: %llu bytes (%.2f MiB)\n",
                  label,
                  (unsigned long long)bytes,
                  (double)bytes / (1024.0 * 1024.0));
}

void goblinPrintHardwareProbe() {
    Serial.println();
    Serial.println("=== ESP GOBLIN HARDWARE PROBE ===");
    Serial.printf("Board profile: %s\n", ESP_GOBLIN_BOARD_NAME);
    Serial.printf("Board ID: %s\n", ESP_GOBLIN_BOARD_ID);
    Serial.printf("CPU MHz: %u\n", ESP.getCpuFreqMHz());
    Serial.printf("SDK: %s\n", ESP.getSdkVersion());

    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_size);
    if (flash_result == ESP_OK) {
        printBytes("Detected flash", flash_size);
    } else {
        Serial.printf("Detected flash: ERROR %d\n", (int)flash_result);
    }

    printBytes("PSRAM", ESP.getPsramSize());
    printBytes("Free PSRAM", ESP.getFreePsram());
    printBytes("Heap", ESP.getHeapSize());
    printBytes("Free heap", ESP.getFreeHeap());
    printBytes("Largest 8-bit block",
               heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    Serial.printf("LCD: ILI9341V %dx%d SPI\n",
                  ESP_GOBLIN_LCD_WIDTH, ESP_GOBLIN_LCD_HEIGHT);

    Wire.begin(ESP_GOBLIN_TOUCH_SDA, ESP_GOBLIN_TOUCH_SCL);
    Wire.beginTransmission(ESP_GOBLIN_TOUCH_ADDR);
    const uint8_t touch_result = Wire.endTransmission();

    Serial.printf("FT6336G @ 0x%02X: %s\n",
                  ESP_GOBLIN_TOUCH_ADDR,
                  touch_result == 0 ? "FOUND" : "NOT FOUND");

    Serial.printf("RGB GPIO: %d\n", ESP_GOBLIN_RGB_PIN);
    Serial.printf("Battery ADC GPIO: %d\n", ESP_GOBLIN_BATTERY_ADC);
    Serial.println("=================================");
    Serial.println();
}
