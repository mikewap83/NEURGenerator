#include <NEURGenerator.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключ из Pollinations
const char* apiKey = "YOUR_API_KEY_HERE";

// Создаем объект генератора
NEURGenerator generator;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NEURGenerator Example ===");
    
    // Подключаемся к WiFi
    generator.connectWiFi(ssid, password);
    
    // Устанавливаем API ключ
    generator.setApiKey(apiKey);
    
    // Проверяем баланс
    if (generator.checkBalance()) {
        Serial.print("✅ Баланс успешно получен: ");
        Serial.println(generator.getBalance());
    } else {
        Serial.print("❌ Ошибка: ");
        Serial.println(generator.getLastError());
    }
}

void loop() {
    // Пока ничего не делаем
    delay(10000);
}