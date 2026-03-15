#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <GSON.h>
#include <GyverHTTP.h>
#include <GTimer.h>
#include <ESP32Ping.h>
#include <esp_task_wdt.h>

// Константы для переводчика
#define TRANS_HOST "api.mymemory.translated.net"
#define TRANS_PORT 443

class NEURGenerator {
public:
    // Конструктор
    NEURGenerator();
    
    // Настройка WiFi
    void connectWiFi(const char* ssid, const char* password);
    
    // Установка API ключа
    void setApiKey(const char* key);
    
    // Проверка баланса
    bool checkBalance();
    
    // Получить баланс (в виде строки)
    const char* getBalance();
    
    // ========== МЕТОДЫ ДЛЯ ПРОМПТОВ ==========
    
    // Установка суффикса (добавляется к промпту)
    void setSuffix(const char* suffix);
    
    // Установка модификатора (стиль, техника)
    void setModifier(const char* modifier);
    
    // Установка negative prompt
    void setDenial(const char* denial);
    
    // Включение/выключение перевода
    void setTranslate(bool enable);
    
    // Установка email для MyMemory (увеличивает лимит переводов)
    void setMyMemoryEmail(const char* email);
    
    // Подготовка промпта (объединение всех частей + перевод если нужно)
    bool preparePrompt(const char* prompt);
    
    // Получить подготовленный промпт
    const char* getPreparedPrompt();
    
    // Получить negative prompt
    const char* getDenial();
    
    // ========== МЕТОДЫ ДЛЯ ГЕНЕРАЦИИ ==========
    
    // Установка модели (flux, turbo, etc)
    void setModel(const char* model);
    
    // Установка размера изображения
    void setSize(uint16_t width, uint16_t height);
    
    // Установка качества (low, medium, high)
    void setQuality(const char* quality);
    
    // Генерация изображения по подготовленному промпту
    bool generate();
    
    // Генерация изображения с произвольным промптом
    bool generate(const char* prompt);
    
    // Генерация с произвольным промптом и negative prompt
    bool generate(const char* prompt, const char* negativePrompt);
    
    // ========== МЕТОДЫ ДЛЯ ПОЛУЧЕНИЯ ДАННЫХ ==========
    
    // Получить URL последнего сгенерированного изображения
    const char* getLastImageUrl();
    
    // Получить сырые данные JPEG (указатель на буфер в PSRAM)
    uint8_t* getImageData();
    
    // Получить размер данных JPEG
    size_t getImageDataSize();
    
    // Проверить, есть ли данные изображения
    bool hasImageData();
    
    // Очистить буфер изображения
    void clearImageData();
    
    // ========== МЕТОДЫ ДЛЯ ПЕРЕВОДА ==========
    
    // Прямой перевод текста (без генерации)
    bool translateText(const char* russian_text);
    
    // Получить переведенный текст
    const char* getTranslatedText();
    
    // Проверить, является ли текст русским
    bool isRussian(const char* text);
    
    // ========== СЛУЖЕБНЫЕ МЕТОДЫ ==========
    
    // Получить статус последней генерации
    int getLastHttpCode();
    
    // Получить статус подключения
    bool isWiFiConnected();
    
    // Получить последнюю ошибку
    const char* getLastError();
    
    // Тикер для обработки таймеров
    void tick();
    
    // Сброс WDT (для использования в циклах)
    void resetWDT();

private:
    // Буферы в PSRAM
    char* _apiKey;
    char* _balance;
    char* _lastError;
    char* _jsonBuffer;
    char* _lastImageUrl;
    char* _model;
    char* _quality;
    
    // Буферы для промптов
    char* _suffix;              // Суффикс для промпта
    char* _modifier;            // Модификатор для промпта
    char* _denial;              // Negative prompt
    char* _preparedPrompt;      // Подготовленный промпт
    char* _translatedText;      // Переведенный текст
    char* _myMemoryEmail;       // Email для MyMemory
    
    // Буфер для JPEG данных (PSRAM)
    uint8_t* _jpegData;
    size_t _jpegDataSize;
    size_t _jpegDataCapacity;
    
    uint16_t _width;
    uint16_t _height;
    int _lastHttpCode;
    
    bool _wifiConnected;
    bool _translateEnabled;     // Флаг перевода
    
    // GyverHTTP клиент
    ghttp::Client* _http;
    
    // Таймер для повторных попыток
    GTimer _timer;
    
    // Константы
    static constexpr const char* HOST = "gen.pollinations.ai";
    static constexpr uint16_t PORT = 443;
    static constexpr size_t JSON_BUFFER_SIZE = 4096;
    static constexpr size_t URL_BUFFER_SIZE = 512;
    static constexpr size_t PROMPT_BUFFER_SIZE = 1024;
    static constexpr size_t JPEG_BUFFER_SIZE = 256 * 1024; // 256KB для JPEG
    
    // Внутренние методы
    bool requestBalance();
    bool requestGeneration(const char* prompt, const char* negativePrompt = nullptr);
    bool requestTranslation(const char* text);
    void urlEncode(const char* src, char* dest, size_t destSize);
    bool isRussianText(const char* text);
    bool isEnglishText(const char* text);
    void cleanupHttp();
    void allocateBuffers();
    void clearJpegBuffer();
    
    // Флаги состояния
    struct {
        bool usePsram : 1;
        bool hasError : 1;
    } _flags;
};
