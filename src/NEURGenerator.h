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
    
    // Проверка баланса (обновленная версия)
    bool checkBalance();
    
    // Получить баланс (в виде строки)
    const char* getBalance();
    
    // Получить статус подключения
    bool isWiFiConnected();
    
    // Получить последнюю ошибку
    const char* getLastError();
    
    // Тикер для обработки таймеров (нужен для Gyver)
    void tick();

private:
    // Буферы в PSRAM (если доступна)
    char* _apiKey;
    char* _balance;
    char* _lastError;
    char* _jsonBuffer;
    
    bool _wifiConnected;
    
    // GyverHTTP клиент
    ghttp::Client* _http;
    
    // Таймер для повторных попыток
    GTimer _timer;
    
    // Константы
    static constexpr const char* HOST = "gen.pollinations.ai";
    static constexpr uint16_t PORT = 443;
    static constexpr size_t JSON_BUFFER_SIZE = 4096;
    
    // Внутренние методы
    bool requestBalance();
    void cleanupHttp();
    void allocateBuffers();
    
    // Флаги состояния
    struct {
        bool usePsram : 1;
        bool hasError : 1;
    } _flags;
};
