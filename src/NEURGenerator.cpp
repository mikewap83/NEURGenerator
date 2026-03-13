#include "NEURGenerator.h"

NEURGenerator::NEURGenerator() {
    _wifiConnected = false;
    _balance = "0";
}

void NEURGenerator::connectWiFi(const char* ssid, const char* password) {
    Serial.print("Подключение к WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\n✅ WiFi подключен");
        Serial.print("IP адрес: ");
        Serial.println(WiFi.localIP());
    } else {
        _wifiConnected = false;
        _lastError = "Ошибка подключения к WiFi";
        Serial.println("\n❌ Ошибка WiFi");
    }
}

void NEURGenerator::setApiKey(const char* key) {
    _apiKey = "Bearer ";
    _apiKey += key;
    Serial.println("✅ API ключ установлен");
}

bool NEURGenerator::checkBalance() {
    if (!_wifiConnected) {
        _lastError = "Нет подключения к WiFi";
        return false;
    }
    
    if (_apiKey.length() == 0) {
        _lastError = "Не установлен API ключ";
        return false;
    }
    
    HTTPClient http;
    String url = "https://";
    url += _host;
    url += "/account/balance";
    
    http.begin(url);
    http.addHeader("Authorization", _apiKey);
    http.addHeader("Accept", "application/json");
    
    Serial.println("📡 Запрос баланса...");
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.print("Ответ: ");
            Serial.println(payload);
            
            // Парсим JSON
            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                const char* balance = doc["balance"];
                if (balance) {
                    _balance = String(balance);
                    Serial.print("💰 Баланс: ");
                    Serial.print(_balance);
                    Serial.println(" pollen");
                    http.end();
                    return true;
                } else {
                    _lastError = "Поле balance не найдено в ответе";
                }
            } else {
                _lastError = "Ошибка парсинга JSON";
            }
        } else {
            _lastError = "HTTP ошибка: ";
            _lastError += httpCode;
        }
    } else {
        _lastError = "Ошибка соединения";
    }
    
    http.end();
    return false;
}

String NEURGenerator::getBalance() {
    return _balance;
}

bool NEURGenerator::isWiFiConnected() {
    return _wifiConnected;
}

String NEURGenerator::getLastError() {
    return _lastError;
}