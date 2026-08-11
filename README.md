# NEURGenerator for ESP32-S3

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue)](https://www.espressif.com/)
[![Version](https://img.shields.io/badge/Version-2.0.0-brightgreen)](https://github.com/mikewap83/NEURGenerator)

Библиотека для генерации изображений через Pollinations.ai API на ESP32-S3 с поддержкой PSRAM, переводом промптов, WDT и расширенной настройкой моделей.

---

## 📦 Версия 2.0.0 - 🚀 ГЛОБАЛЬНОЕ ОБНОВЛЕНИЕ

### ✨ Новые возможности:

| Возможность | Описание |
|-------------|----------|
| 🆓 **Бесплатный режим** | Использование `image.pollinations.ai` без API ключа |
| 📐 **Адаптивные размеры** | Автоматическая подстройка размеров под модель из `config.json` |
| 🔄 **Прогрессивный JPEG** | Поддержка многослойных изображений (flux, sana, dreamshaper) |
| 📁 **Загрузка конфига** | Автоматическая загрузка моделей из `config.json` |
| 🔄 **Повторная загрузка** | Загрузка ранее сгенерированных изображений |
| 🎯 **Новые статусы** | `OK_WAITING_COMMAND`, `ERROR_BALANCEBUDGET`, `ERROR_ACCESSDENIED`, `ERROR_LOADEDOLDIMAGES` |
| 📊 **Новые callback'и** | `onRenderTft()`, `onRenderRet()`, `onRenderDel()` |
| ⚡ **Расширенный буфер** | Динамическое расширение буфера JPEG до 786KB |
| 🔧 **Singleton паттерн** | `NEURWorker::getInstance()` для удобства |
| 📂 **Работа с файлами** | Загрузка и создание конфига через `load_config_from_file()` и `create_example_config()` |

### 🔄 Изменения в API:

- `data_prepare()` больше не принимает параметр `translate` (используйте `setAPISwitch()`)
- Переход на новую структуру `flags` с дополнительными флагами
- Обновление системы статусов и callback'ов
- Добавлены методы для работы с конфигом

### 🛠️ Исправления:

- 🔧 Оптимизация работы с PSRAM
- 🔧 Улучшение обработки ошибок
- 🔧 Исправление проблем с WDT
- 🔧 Улучшение парсинга JSON
- 🔧 Исправление работы с прогрессивным JPEG

---

### ✨ Основные возможности:

- ✅ Подготовка промптов (объединение с суффиксами и модификаторами)
- ✅ Автоматический перевод русских промптов через MyMemory API
- ✅ Генерация изображений через Pollinations.ai (платный и бесплатный режимы)
- ✅ Хранение JPEG данных в PSRAM (динамическое расширение до 786KB)
- ✅ Система callback'ов для отслеживания состояний (8 callback'ов)
- ✅ Получение сырых данных для дальнейшей обработки
- ✅ WDT защита для предотвращения зависаний
- ✅ Поддержка разных моделей (flux, sana, dreamshaper, zimage)
- ✅ Настройка размера и качества (3 уровня)
- ✅ Поддержка negative prompts
- ✅ Автоматические повторы при ошибках
- ✅ Проверка баланса Pollen
- ✅ Загрузка конфига моделей из JSON
- ✅ Прогрессивный JPEG (поддержка FLUX, SANA, DREAMSHAPER)

---

## 📚 Содержание

1. [Быстрый старт](#быстрый-старт)
2. [Установка](#установка)
3. [Настройка](#настройка)
4. [Примеры использования](#примеры-использования)
5. [Callback'и](#callbackи)
6. [API Reference](#api-reference)
7. [Конфигурация моделей](#конфигурация-моделей)
8. [История версий](#история-версий)
9. [Зависимости](#зависимости)
---

## Быстрый старт

### 1. Базовый пример

```cpp
#include <NEURGenerator.h>
#include <FS.h>
#include <LittleFS.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiKey = "YOUR_SK_API_KEY";
const char* privateKey = "YOUR_PK_API_KEY";
const char* myMemoryEmail = "YOUR_EMAIL";

NEURGenerator generator;

void setup() {
    Serial.begin(115200);
    
    // Инициализация файловой системы
    LittleFS.begin();
    
    // Установка ключей
    generator.setKeySecret(apiKey, privateKey);
    generator.setMyMemmory(myMemoryEmail);
    
    if (generator.load_config_from_file("/config.json") == 0) {
        generator.create_example_config("/config.json");
    }
    
    // Подключение к WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // Проверка баланса
    if (generator.getApiPollen(apiKey)) {
        Serial.printf("Баланс: %s pollen\n", generator.getPollen());
    }
}

void loop() {
    generator.tick(WiFi.status() == WL_CONNECTED);
    delay(100);
}
```

### 2. Генерация изображения с callback'ами
```cpp
#include <NEURGenerator.h>
#include <FS.h>
#include <LittleFS.h>

NEURGenerator generator;

// Callback'и
void onRenderRun() {
    Serial.println("🎨 Начало генерации...");
}

void onRenderEnd() {
    Serial.println("✅ Генерация завершена!");
    uint8_t* data = generator.getImageData();
    size_t size = generator.getImageDataSize();
    Serial.printf("📊 Размер JPEG: %d байт\n", size);
}

void onRenderErr() {
    Serial.printf("❌ Ошибка: %s\n", generator.getStateStatus(true));
}

void onRenderEng() {
    Serial.println("🌐 Перевод выполнен");
}

void onRenderUnd() {
    Serial.println("⏸️ Генерация прервана");
}

void setup() {
    Serial.begin(115200);
    LittleFS.begin();
    
    // Настройка
    generator.setKeySecret("sk_key", "pk_key", "email");
    generator.onRenderRun(onRenderRun);
    generator.onRenderEnd(onRenderEnd);
    generator.onRenderErr(onRenderErr);
    generator.onRenderEng(onRenderEng);
    generator.onRenderUnd(onRenderUnd);
    
    // НОВЫЕ НАСТРОЙКИ
    generator.setAPISwitch(true);     // Включить перевод
    generator.setAPIFreely(false);    // Платный режим
    generator.setAPIAdjust(false);    // Адаптивные размеры
    generator.setUseScreen(true);     // Обработка экрана
    
    // Загрузка конфига
    generator.load_config_from_file("/config.json");
    
    // Подготовка промпта (БЕЗ параметра translate!)
    generator.data_prepare(
        "красивый закат над морем",  // промпт
        "high quality",               // суффикс
        "digital art",                // модификатор
        "ugly, blurry"                // negative prompt
    );
    
    // Отправка запроса
    generator.send_request();
}

void loop() {
    generator.tick(WiFi.status() == WL_CONNECTED);
    delay(100);
}
```

### Установка

#### 1. Через Library Manager (рекомендуется)

1. Откройте Arduino IDE
2. Перейдите в **Скетч → Подключить библиотеку → Управлять библиотеками...**
3. Найдите **NEURGenerator**
4. Нажмите **Установить**

#### 2. Вручную

1. Скачайте библиотеку с [GitHub](https://github.com/mikewap83/NEURGenerator)
2. Распакуйте в папку `libraries` вашего проекта
3. Переименуйте папку в `NEURGenerator`

#### 3. Зависимости

Библиотека требует установки следующих зависимостей:

| Библиотека | Версия | Описание |
|------------|--------|----------|
| **GyverHTTP** | ≥ 1.0 | HTTP клиент |
| **GSON** | ≥ 1.0 | Парсер JSON |
| **GTimer** | ≥ 1.0 | Таймеры |
| **ESP32Ping** | ≥ 1.0 | Проверка доступности хоста |
| **JPEGDEC** | ≥ 1.0 | Декодирование JPEG |

---

### Настройка

#### Основные настройки

```cpp
// Установка ключей API
generator.setKeySecret("sk_key", "pk_key");
generator.setMyMemmory("email@domain.com");

// Настройка параметров
generator.setUsePings(true);      // Использовать ping
generator.setUseLoges(true);      // Выводить логи
generator.setUseScreen(true);     // Обработка экрана
generator.setUseTasks(true);      // Сброс WDT

// Настройка WDT
generator.setWDT(10000, &twdt_config);

// Настройка таймаутов
generator.setAttempts(30000, 15000, 5, 5);
```

#### Настройки API (НОВОЕ в версии 2.0)
```cpp
// ⭐ РЕЖИМЫ РАБОТЫ
generator.setAPIFreely(false);    // false - платный, true - бесплатный
generator.setAPIAdjust(false);    // true - адаптивные размеры из config.json
generator.setAPISwitch(true);     // true - включить перевод промптов

// ⭐ НАСТРОЙКА МОДЕЛИ
generator.setAPINumber(0);        // Индекс модели (0, 1, 2...)
generator.setAPIModels("dreamshaper"); // Имя модели
generator.setAPILevels(1);        // Качество: 0-низкое, 1-среднее, 2-высокое
generator.setAPIScales(1);        // Размер: 0-маленький, 1-средний, 2-большой
generator.setAPIEnhanc(false);    // Улучшение изображения
generator.setAPIFilter(false);    // Фильтр контента
```
### Примеры использования

| Пример | Описание | Ссылка |
|--------|----------|--------|
| **Basic** | Базовая проверка подключения и баланса | [`examples/Basic/Basic.ino`](examples/Basic/Basic.ino) |
| **GenerateImage** | Генерация изображения по промпту | [`examples/GenerateImage/GenerateImage.ino`](examples/GenerateImage/GenerateImage.ino) |
| **PresentImage** | Показ изображения по url | [`examples/PresentImage/PresentImage.ino`](examples/PresentImage/PresentImage.ino) |
| **SD_Save** | Сохранение JPEG на SD карту | [`examples/SD_Save/SD_Save.ino`](examples/SD_Save/SD_Save.ino) |
| **TFT_Display** | Вывод изображения на TFT дисплей | [`examples/TFT_Display/TFT_Display.ino`](examples/TFT_Display/TFT_Display.ino) |


## Callback'и
### Полный список callback'ов (версия 2.0)

```cpp
// ⭐ ОСНОВНЫЕ CALLBACK'И
void onRenderRun() { /* Начало генерации */ }
void onRenderEnd() { /* Генерация завершена */ }
void onRenderErr() { /* Ошибка генерации */ }
void onRenderEng() { /* Перевод выполнен */ }
void onRenderUnd() { /* Генерация прервана */ }

// ⭐ НОВЫЕ CALLBACK'И (версия 2.0)
void onRenderTft() { /* Изображение готово для TFT */ }
void onRenderRet() { /* Повторная загрузка изображения */ }
void onRenderDel() { /* Изображение удалено (недоступно) */ }

// Регистрация callback'ов
generator.onRenderRun(onRenderRun);
generator.onRenderEnd(onRenderEnd);
generator.onRenderErr(onRenderErr);
generator.onRenderEng(onRenderEng);
generator.onRenderUnd(onRenderUnd);
generator.onRenderTft(onRenderTft); // ⭐ НОВЫЙ
generator.onRenderRet(onRenderRet); // ⭐ НОВЫЙ
generator.onRenderDel(onRenderDel); // ⭐ НОВЫЙ
```

## API Reference

### Основные методы

| Метод | Описание |
|-------|----------|
| `setKeySecret(sk, pk)` | Установка API ключей |
| `setMyMemmory(email)` | Установка email для MyMemory |
| `setUsePings(bool)` | Включить/выключить ping |
| `setUseLoges(bool)` | Включить/выключить логи |
| `setUseScreen(bool)` | Включить/выключить обработку экрана |
| `setUseTasks(bool)` | Включить/выключить сброс WDT |
| `setAttempts(clients, timeout, request, receive)` | Настройка таймаутов |
| `setWDT(timeout, config)` | Настройка WDT |
| `setStateStatus(Status)` | Установка статуса |
| `getStateStatus(bool)` | Получение статуса |
| `getStateUpdate()` | Проверка обновления состояния |
| `getStateNumber()` | Получение номера состояния |

### Настройка API

| Метод | Описание |
|-------|----------|
| `setAPIFreely(bool)` | Включить бесплатный режим |
| `setAPIAdjust(bool)` | Включить адаптивные размеры |
| `setAPISwitch(bool)` | Включить перевод промптов |
| `setAPINumber(uint8_t)` | Установить индекс модели |
| `setAPIModels(const char*)` | Установить имя модели |
| `setAPILevels(uint8_t)` | Установить качество (0-2) |
| `setAPIScales(uint8_t)` | Установить размер (0-2) |
| `setAPIEnhanc(bool)` | Включить улучшение |
| `setAPIFilter(bool)` | Включить фильтр |

### Работа с конфигом

| Метод | Описание |
|-------|----------|
| `load_config(jsonData, size)` | Загрузка конфига из JSON строки |
| `load_config_from_file(filename)` | Загрузка конфига из файла |
| `create_example_config(filename)` | Создание примера конфига |
| `getModelScale(model, level, ...)` | Получение размеров модели |

### Получение данных

| Метод | Описание |
|-------|----------|
| `getImageData()` | Получение указателя на JPEG данные |
| `getImageDataSize()` | Получение размера JPEG данных |
| `hasImageData()` | Проверка наличия данных |
| `clearImageData()` | Очистка данных |
| `getPollen()` | Получение баланса |
| `isGenerating()` | Проверка состояния генерации |
| `isWorkApiNow()` | Проверка работы API |
| `isRequestError()` | Проверка ошибки запроса |
| `isReceiveError()` | Проверка ошибки получения |

### Работа с моделями

| Метод | Описание |
|-------|----------|
| `getAPIModelsNames()` | Получение списка моделей |
| `getAPIModelsTitles()` | Получение названий моделей |
| `getAPIModelsPrice()` | Получение цен моделей |
| `getAPIModelsNamesCount()` | Количество моделей |
| `getAPIModelsNamesByIndex(i)` | Имя модели по индексу |
| `getAPIModelsTitleByIndex(i)` | Название модели по индексу |
| `getAPIModelsPriceByIndex(i)` | Цена модели по индексу |
| `getAPIModelsDisplay()` | Отображение текущей модели |

### Управление генерацией

| Метод | Описание |
|-------|----------|
| `data_prepare(prompt, suffix, modifi, denial)` | Подготовка промпта |
| `send_request()` | Отправка запроса |
| `resp_receive()` | Получение ответа |
| `stop_request()` | Остановка запроса |
| `stop_receive()` | Остановка получения |
| `PresentImage(url)` | Загрузка существующего изображения |
| `tick(WiFiState)` | Обновление состояния |


## Конфигурация моделей

### Формат config.json

Библиотека поддерживает загрузку конфигурации моделей из JSON файла:

```json
{
  "models": [
    {
      "name": "flux",
      "max_dimensionW": 1024,
      "max_dimensionH": 768,
      "scales": [
        { "level": 0, "width": 512, "height": 384 },
        { "level": 1, "width": 768, "height": 576 },
        { "level": 2, "width": 1024, "height": 768 }
      ]
    },
    {
      "name": "sana",
      "max_dimensionW": 576,
      "max_dimensionH": 384,
      "scales": [
        { "level": 0, "width": 480, "height": 320 },
        { "level": 1, "width": 528, "height": 352 },
        { "level": 2, "width": 576, "height": 384 }
      ]
    },
    {
      "name": "dreamshaper",
      "max_dimensionW": 576,
      "max_dimensionH": 384,
      "scales": [
        { "level": 0, "width": 480, "height": 320 },
        { "level": 1, "width": 528, "height": 352 },
        { "level": 2, "width": 576, "height": 384 }
      ]
    },
    {
      "name": "zimage",
      "max_dimensionW": 960,
      "max_dimensionH": 640,
      "scales": [
        { "level": 0, "width": 480, "height": 320 },
        { "level": 1, "width": 720, "height": 480 },
        { "level": 2, "width": 960, "height": 640 }
      ]
    }
  ]
}
```

### Параметры модели

| Параметр | Описание |
|----------|----------|
| `name` | Имя модели (должно совпадать с `setAPIModels()`) |
| `max_dimensionW` | Максимальная ширина |
| `max_dimensionH` | Максимальная высота |
| `scales` | Массив доступных размеров |
| `level` | Уровень размера (0, 1, 2) |
| `width` | Ширина для данного уровня |
| `height` | Высота для данного уровня |

### Автозагрузка конфига

```cpp
// Загрузка из файла
if (generator.load_config_from_file("/config.json") == 0) {
    // Если файл не найден - создаем пример
    generator.create_example_config("/config.json");
}

// Получение информации о моделях
uint8_t count = generator.getAPIModelsNamesCount();
for (uint8_t i = 0; i < count; i++) {
    Serial.printf("%s - %s (%s)\n",
        generator.getAPIModelsNamesByIndex(i),
        generator.getAPIModelsTitleByIndex(i),
        generator.getAPIModelsPriceByIndex(i));
}
```

## История версий

### Версия 2.0.0 - 🚀 ГЛОБАЛЬНОЕ ОБНОВЛЕНИЕ (ТЕКУЩАЯ)

**Новые возможности:**
- ✨ **Бесплатный режим** - использование `image.pollinations.ai` без API ключа
- ✨ **Адаптивные размеры** - автоматическая подстройка под модель из config.json
- ✨ **Прогрессивный JPEG** - поддержка многослойных изображений (flux, sana, dreamshaper)
- ✨ **Загрузка конфига** - автоматическая загрузка моделей из файла
- ✨ **Повторная загрузка** - загрузка ранее сгенерированных изображений
- ✨ **Новые статусы** - `OK_WAITING_COMMAND`, `ERROR_BALANCEBUDGET`, `ERROR_ACCESSDENIED`, `ERROR_LOADEDOLDIMAGES`
- ✨ **Новые callback'и** - `onRenderTft()`, `onRenderRet()`, `onRenderDel()`
- ✨ **Расширенный буфер** - динамическое расширение до 786KB
- ✨ **Singleton паттерн** - `NEURWorker::getInstance()` для удобства
- ✨ **Новые методы** - `getStateUpdate()`, `getStateNumber()`, `load_config_from_file()`, `create_example_config()`
- ✨ **Новые сеттеры** - `setAPIFreely()`, `setAPIAdjust()`, `setAPISwitch()`, `setUseScreen()`

**Исправления:**
- 🔧 Оптимизация работы с PSRAM
- 🔧 Улучшение обработки ошибок
- 🔧 Исправление проблем с WDT
- 🔧 Улучшение парсинга JSON
- 🔧 Исправление работы с прогрессивным JPEG

**Изменения:**
- 🔄 `data_prepare()` больше не принимает параметр `translate` (используйте `setAPISwitch()`)
- 🔄 Переход на новую структуру `flags` с дополнительными флагами
- 🔄 Обновление системы статусов и callback'ов

---

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

| Библиотека | Версия | Описание | Ссылка |
|------------|--------|----------|--------|
| **GyverHTTP** | ≥ 1.0 | HTTP клиент | [GitHub](https://github.com/GyverLibs/GyverHTTP) |
| **GSON** | ≥ 1.0 | Парсер JSON | [GitHub](https://github.com/GyverLibs/GSON) |
| **GTimer** | ≥ 1.0 | Таймеры | [GitHub](https://github.com/GyverLibs/GTimer) |
| **ESP32Ping** | ≥ 1.0 | Проверка доступности хоста | [GitHub](https://github.com/marian-craciunescu/ESP32Ping) |
| **JPEGDEC** | ≥ 1.0 | Декодирование JPEG | [GitHub](https://github.com/bitbank2/JPEGDEC) |
| **LittleFS** | Встроенная | Файловая система | - |

---

## Лицензия

MIT License - свободное использование в любых проектах.

---

## Автор

**mikewap83**

[GitHub](https://github.com/mikewap83/NEURGenerator)

---

## Поддержка

Если у вас возникли вопросы или проблемы:

1. Проверьте [примеры использования](examples/)
2. Создайте Issue на [GitHub](https://github.com/mikewap83/NEURGenerator/issues)
3. Обратитесь в Telegram канал проекта

---

**⭐ Если проект полезен, поставьте звезду на GitHub!**
