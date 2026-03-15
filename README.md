# NEURGenerator for ESP32-S3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)

Библиотека для генерации изображений через Pollinations.ai API на ESP32.

## Версия 1.2.0 - Подготовка промптов и хранение в PSRAM

Новые возможности:
- Подготовка промптов (объединение с суффиксами и модификаторами)
- Автоматический перевод русских промптов через MyMemory API
- Хранение JPEG данных в PSRAM
- Получение сырых данных для дальнейшей обработки
- WDT защита для предотвращения зависаний

### Пример подготовки промпта

```cpp
// Настройка параметров промпта
generator.setSuffix("high quality, detailed");
generator.setModifier("digital art");
generator.setTranslate(true);
generator.setMyMemoryEmail("your_email@gmail.com");

// Подготовка промпта (автоматический перевод если нужно)
generator.preparePrompt("красивый закат над морем");

// Генерация с подготовленным промптом
generator.generate();

// Получение данных
uint8_t* jpegData = generator.getImageData();
size_t jpegSize = generator.getImageDataSize();
Версия 1.1.0 - Генерация изображений
Новые возможности:

Генерация изображений по текстовому описанию

Поддержка разных моделей (flux, turbo)

Настройка размера и качества

Поддержка negative prompts

Автоматические повторы при ошибках

Пример генерации
cpp
// Настройка параметров
generator.setModel("flux");
generator.setSize(512, 512);
generator.setQuality("medium");

// Простая генерация
generator.generateImage("a beautiful cat in space");

// Генерация с negative prompt
generator.generateImage("a beautiful cat", "ugly, blurry, bad quality");

// Получение URL
String url = generator.getLastImageUrl();
История версий
Версия 1.2.0 (текущая)
✨ Добавлена подготовка промптов (суффиксы и модификаторы)
✨ Добавлен автоматический перевод через MyMemory API
✨ Добавлено хранение JPEG данных в PSRAM
✨ Добавлены методы для получения сырых данных
✨ Добавлена WDT защита от зависаний

Версия 1.1.0
✨ Добавлена генерация изображений
✨ Поддержка разных моделей
✨ Настройка размера и качества
✨ Поддержка negative prompts
✨ Автоматические повторы при ошибках

Версия 1.0.1
🔧 Переход на GyverHTTP и GSON
🔧 Поддержка PSRAM
🔧 Добавлена проверка через ESP32Ping

Версия 1.0.0
🎉 Базовая инициализация
🎉 Проверка баланса Pollen
🎉 Подключение к WiFi
