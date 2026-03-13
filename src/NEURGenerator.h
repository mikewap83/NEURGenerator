#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

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
    String getBalance();
    
    // Получить статус подключения
    bool isWiFiConnected();
    
    // Получить последнюю ошибку
    String getLastError();

private:
    String _apiKey;
    String _balance;
    String _lastError;
    bool _wifiConnected;
    
    // Константы
    const char* _host = "gen.pollinations.ai";
    const int _port = 443;
};