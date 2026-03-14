# NEURGenerator for ESP32-S3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)

Библиотека для генерации изображений через Pollinations.ai API на ESP32.

## Версия 1.1.0 - Генерация изображений

Новые возможности:
- Генерация изображений по текстовому описанию
- Поддержка разных моделей (flux, turbo)
- Настройка размера и качества
- Поддержка negative prompts
- Автоматические повторы при ошибках

### Пример генерации

```cpp
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
