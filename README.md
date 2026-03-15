# NEURGenerator for ESP32-S3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)

Библиотека для генерации изображений через Pollinations.ai API на ESP32-S3 с поддержкой PSRAM, переводом промптов и WDT.

## Версия 1.2.5 - Финальная стабильная версия

### Возможности:
- ✅ Подготовка промптов (объединение с суффиксами и модификаторами)
- ✅ Автоматический перевод русских промптов через MyMemory API
- ✅ Генерация изображений через Pollinations.ai
- ✅ Хранение JPEG данных в PSRAM (до 256KB)
- ✅ Система callback'ов для отслеживания состояний
- ✅ Получение сырых данных для дальнейшей обработки
- ✅ WDT защита для предотвращения зависаний
- ✅ Поддержка разных моделей (flux, turbo, etc)
- ✅ Настройка размера и качества
- ✅ Поддержка negative prompts
- ✅ Автоматические повторы при ошибках
- ✅ Проверка баланса Pollen


### Простой пример генерации
```cpp
#include <NEURGenerator.h>

// Настройка параметров
generator.setModel("flux");
generator.setSize(512, 512);
generator.setQuality("medium");

// Генерация
generator.generateImage("a beautiful cat in space");

// Получение URL
String url = generator.getLastImageUrl();
```

### Пример с callback'ами

```cpp
#include <NEURGenerator.h>

// Callback при успешной генерации
void onRenderEnd() {
    uint8_t* data = generator->getImageData();
    size_t size = generator->getImageDataSize();
    Serial.printf("Получено %d байт JPEG\n", size);
}

NEURGenerator* generator;

void setup() {
    generator = new NEURGenerator("sk_key", "pk_key", "email");
    generator->onRenderEnd(onRenderEnd);
    
    // Подготовка и генерация
    generator->data_prepare("красивый закат", "high quality", "", "", true);
    generator->send_request();
}
```

### История версий
## Версия 1.2.5
✨ Добавлена полная система callback'ов

✨ Улучшена работа с PSRAM

✨ Оптимизация декодирования JPEG

✨ Исправлены ошибки WDT

## Версия 1.2.0
✨ Подготовка промптов (суффиксы и модификаторы)

✨ Автоматический перевод через MyMemory API

✨ Хранение JPEG данных в PSRAM

✨ Методы для получения сырых данных

## Версия 1.1.0
✨ Генерация изображений

✨ Поддержка разных моделей

✨ Настройка размера и качества

✨ Поддержка negative prompts

✨ Автоматические повторы при ошибках

## Версия 1.0.1
🔧 Переход на GyverHTTP и GSON

🔧 Поддержка PSRAM

🔧 Добавлена проверка через ESP32Ping

## Версия 1.0.0
🎉 Базовая инициализация

🎉 Проверка баланса Pollen

🎉 Подключение к WiFi

## Зависимости

- [GyverHTTP](https://github.com/GyverLibs/GyverHTTP) - HTTP клиент
- [GSON](https://github.com/GyverLibs/GSON) - Парсер JSON
- [GTimer](https://github.com/GyverLibs/GTimer) - Таймеры
- [ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping) - Проверка доступности хоста
- [JPEGDEC](https://github.com/bitbank2/JPEGDEC) - Декодирование JPEG
