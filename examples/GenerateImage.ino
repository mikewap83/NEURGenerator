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
    
    Serial.println("\n=== NEURGenerator 1.1.0 - Генерация изображений ===");
    
    // Подключаемся к WiFi
    generator.connectWiFi(ssid, password);
    
    // Устанавливаем API ключ
    generator.setApiKey(apiKey);
    
    // Проверяем баланс
    if (generator.checkBalance()) {
        Serial.print("💰 Баланс: ");
        Serial.println(generator.getBalance());
    } else {
        Serial.print("❌ Ошибка баланса: ");
        Serial.println(generator.getLastError());
        return;
    }
    
    // Настраиваем параметры генерации
    generator.setModel("flux");           // Модель: flux, turbo, etc
    generator.setSize(512, 512);          // Размер изображения
    generator.setQuality("medium");       // Качество: low, medium, high
    
    // Генерируем изображение
    Serial.println("\n🎨 Генерация изображения...");
    
    if (generator.generateImage("a beautiful cat in space, cyberpunk style")) {
        Serial.println("✅ Изображение сгенерировано!");
        Serial.print("🔗 URL: ");
        Serial.println(generator.getLastImageUrl());
    } else {
        Serial.print("❌ Ошибка генерации: ");
        Serial.println(generator.getLastError());
        Serial.print("HTTP код: ");
        Serial.println(generator.getLastHttpCode());
    }
}

void loop() {
    generator.tick();
    delay(1000);
}