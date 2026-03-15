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

### Быстрый старт

```cpp
#include <NEURGenerator.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключи
const char* apiKey = "YOUR_SK_API_KEY";
const char* privateKey = "YOUR_PK_API_KEY";
const char* myMemoryEmail = "YOUR_EMAIL";

// Создаем объект генератора
NEURGenerator generator;

void setup() {
    Serial.begin(115200);
    
    // Устанавливаем ключи
    generator.setKeySecret(apiKey, privateKey);
    generator.setMyMemmory(myMemoryEmail);
    
    // Подключаемся к WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // Проверяем баланс
    if (generator.getApiPollen(apiKey)) {
        Serial.printf("Баланс: %s pollen\n", generator.getPollen());
    }
}
```

### Пример генерации с callback'ами

```cpp
#include <NEURGenerator.h>

NEURGenerator generator;

void onRenderEnd() {
    // Данные готовы
    uint8_t* data = generator.getImageData();
    size_t size = generator.getImageDataSize();
    Serial.printf("Получено %d байт JPEG\n", size);
}

void setup() {
    Serial.begin(115200);
    
    // Настройка
    generator.setKeySecret("sk_key", "pk_key", "email");
    generator.onRenderEnd(onRenderEnd);
    
    // Подготовка промпта
    generator.data_prepare(
        "красивый закат над морем",  // промпт
        "high quality",               // суффикс
        "digital art",                // модификатор
        "ugly, blurry",               // negative prompt
        true                          // перевод
    );
    
    // Отправка запроса
    generator.send_request();
}

void loop() {
    generator.tick(WiFi.status() == WL_CONNECTED);
}
```

## Примеры использования

| Пример | Описание | Ссылка |
|--------|----------|--------|
| **Basic** | Базовая проверка подключения и баланса | [`examples/Basic/Basic.ino`](examples/Basic/Basic.ino) |
| **GenerateImage** | Генерация изображения по промпту | [`examples/GenerateImage/GenerateImage.ino`](examples/GenerateImage/GenerateImage.ino) |
| **SD_Save** | Сохранение JPEG на SD карту | [`examples/SD_Save/SD_Save.ino`](examples/SD_Save/SD_Save.ino) |
| **TFT_Display** | Вывод изображения на TFT дисплей | [`examples/TFT_Display/TFT_Display.ino`](examples/TFT_Display/TFT_Display.ino) |

---

## История версий

### Версия 1.2.5
- ✨ Добавлена полная система callback'ов
- ✨ Улучшена работа с PSRAM
- ✨ Оптимизация чтения JPEG
- ✨ Исправлены ошибки WDT

### Версия 1.2.0
- ✨ Подготовка промптов (суффиксы и модификаторы)
- ✨ Автоматический перевод через MyMemory API
- ✨ Хранение JPEG данных в PSRAM
- ✨ Методы для получения сырых данных

### Версия 1.1.0
- ✨ Генерация изображений
- ✨ Поддержка разных моделей
- ✨ Настройка размера и качества
- ✨ Поддержка negative prompts
- ✨ Автоматические повторы при ошибках

### Версия 1.0.1
- 🔧 Переход на GyverHTTP и GSON
- 🔧 Поддержка PSRAM
- 🔧 Добавлена проверка через ESP32Ping

### Версия 1.0.0
- 🎉 Базовая инициализация
- 🎉 Проверка баланса Pollen
- 🎉 Подключение к WiFi

---

## Зависимости

| Библиотека | Описание | Ссылка |
|------------|----------|--------|
| **GyverHTTP** | HTTP клиент | [GitHub](https://github.com/GyverLibs/GyverHTTP) |
| **GSON** | Парсер JSON | [GitHub](https://github.com/GyverLibs/GSON) |
| **GTimer** | Таймеры | [GitHub](https://github.com/GyverLibs/GTimer) |
| **ESP32Ping** | Проверка доступности хоста | [GitHub](https://github.com/marian-craciunescu/ESP32Ping) |
| **JPEGDEC** | Декодирование JPEG | [GitHub](https://github.com/bitbank2/JPEGDEC) |
