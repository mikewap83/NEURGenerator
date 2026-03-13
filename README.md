# NEURGenerator for ESP32-S3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)

Библиотека для генерации изображений через Pollinations.ai API на ESP32.

## Версия 1.0
Базовая инициализация и проверка баланса Pollen.

### Возможности
- ✅ Подключение к WiFi
- ✅ Установка API ключа
- ✅ Проверка баланса Pollen
- ✅ Обработка ошибок

### Установка

**Через Arduino IDE:**
1. Скачайте архив
2. Arduino IDE → Скетч → Подключить библиотеку → Добавить .ZIP библиотеку

**Вручную:**
Скопируйте папку `NEURGenerator` в `Documents/Arduino/libraries/`

### Использование

```cpp
#include <NEURGenerator.h>

NEURGenerator generator;

void setup() {
    Serial.begin(115200);
    
    generator.connectWiFi("Ваш_SSID", "Ваш_пароль");
    generator.setApiKey("ваш_api_ключ");
    
    if (generator.checkBalance()) {
        Serial.print("Баланс: ");
        Serial.println(generator.getBalance());
    }
}
