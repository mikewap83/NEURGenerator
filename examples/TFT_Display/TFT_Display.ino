#include <NEURGenerator.h>
#include <TFT_eSPI.h>
#include <JPEGDEC.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключ из Pollinations
const char* apiKey = "YOUR_API_KEY_HERE";

// Email для MyMemory (необязательно)
const char* myMemoryEmail = "your_email@gmail.com";

// Создаем объекты
NEURGenerator generator;
TFT_eSPI tft = TFT_eSPI();

// Для декодирования JPEG
JPEGDEC jpeg;
uint16_t* tftBuffer = nullptr;

// Callback для отрисовки JPEG
int JPEGDraw(JPEGDRAW *pDraw) {
    tft.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NEURGenerator 1.2.0 - TFT Display Example ===");
    
    // Инициализация TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Connecting...", 10, 10);
    
    // Подключаемся к WiFi
    generator.connectWiFi(ssid, password);
    
    // Устанавливаем API ключ
    generator.setApiKey(apiKey);
    
    // Устанавливаем email для переводов
    generator.setMyMemoryEmail(myMemoryEmail);
    
    // Настраиваем параметры
    generator.setModel("flux");
    generator.setSize(512, 512);
    generator.setQuality("medium");
    
    // Включаем перевод
    generator.setTranslate(true);
    
    // Устанавливаем суффикс и модификатор
    generator.setSuffix("high quality, detailed");
    generator.setModifier("digital art");
    
    // Проверяем баланс
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Checking balance...", 10, 10);
    
    if (generator.checkBalance()) {
        Serial.print("💰 Баланс: ");
        Serial.println(generator.getBalance());
    }
    
    // Готовим промпт
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Preparing prompt...", 10, 10);
    
    if (generator.preparePrompt("красивый закат над морем, пальмы, песок")) {
        Serial.print("📝 Промпт: ");
        Serial.println(generator.getPreparedPrompt());
        
        // Генерируем изображение
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Generating...", 10, 10);
        
        if (generator.generate()) {
            Serial.println("✅ Изображение сгенерировано!");
            Serial.printf("📊 Размер JPEG: %d байт\n", generator.getImageDataSize());
            
            // Декодируем и отображаем JPEG
            uint8_t* jpegData = generator.getImageData();
            size_t jpegSize = generator.getImageDataSize();
            
            if (jpeg.openRAM(jpegData, jpegSize, JPEGDraw)) {
                jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
                
                tft.fillScreen(TFT_BLACK);
                jpeg.decode(0, 0, 0);
                jpeg.close();
                
                Serial.println("✅ Изображение отображено на TFT");
            } else {
                Serial.println("❌ Ошибка декодирования JPEG");
                tft.fillScreen(TFT_RED);
                tft.drawString("JPEG Error", 10, 10);
            }
        } else {
            Serial.print("❌ Ошибка генерации: ");
            Serial.println(generator.getLastError());
            tft.fillScreen(TFT_RED);
            tft.drawString("Generation Error", 10, 10);
        }
    }
}

void loop() {
    generator.tick();
    generator.resetWDT();
    delay(10);
}