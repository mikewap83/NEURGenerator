#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <GSON.h>
#include <GyverHTTP.h>
#include <GTimer.h>
#include <ESP32Ping.h>

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
    
    // ========== НОВЫЕ МЕТОДЫ ДЛЯ ГЕНЕРАЦИИ ==========
    
    // Установка модели (flux, turbo, etc)
    void setModel(const char* model);
    
    // Установка размера изображения
    void setSize(uint16_t width, uint16_t height);
    
    // Установка качества (low, medium, high)
    void setQuality(const char* quality);
    
    // Генерация изображения по промпту
    bool generateImage(const char* prompt);
    
    // Генерация с дополнительными параметрами
    bool generateImage(const char* prompt, const char* negativePrompt);
    
    // Получить URL последнего сгенерированного изображения
    const char* getLastImageUrl();
    
    // Получить статус последней генерации
    int getLastHttpCode();
    
    // ========== СУЩЕСТВУЮЩИЕ МЕТОДЫ ==========
    
    // Получить баланс (в виде строки)
    const char* getBalance();
    
    // Получить статус подключения
    bool isWiFiConnected();
    
    // Получить последнюю ошибку
    const char* getLastError();
    
    // Тикер для обработки таймеров
    void tick();

private:
    // Буферы в PSRAM
    char* _apiKey;
    char* _balance;
    char* _lastError;
    char* _jsonBuffer;
    char* _lastImageUrl;      // NEW: URL последней картинки
    char* _model;             // NEW: модель
    char* _quality;           // NEW: качество
    
    uint16_t _width;          // NEW: ширина
    uint16_t _height;         // NEW: высота
    int _lastHttpCode;        // NEW: последний HTTP код
    
    bool _wifiConnected;
    
    // GyverHTTP клиент
    ghttp::Client* _http;
    
    // Таймер для повторных попыток
    GTimer _timer;
    
    // Константы
    static constexpr const char* HOST = "gen.pollinations.ai";
    static constexpr uint16_t PORT = 443;
    static constexpr size_t JSON_BUFFER_SIZE = 4096;
    static constexpr size_t URL_BUFFER_SIZE = 512;
    
    // Внутренние методы
    bool requestBalance();
    bool requestGeneration(const char* prompt, const char* negativePrompt = nullptr);
    void urlEncode(const char* src, char* dest, size_t destSize);
    void cleanupHttp();
    void allocateBuffers();
    
    // Флаги состояния
    struct {
        bool usePsram : 1;
        bool hasError : 1;
    } _flags;
};
