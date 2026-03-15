#include <NEURGenerator.h>
#include <SD.h>
#include <FS.h>
#include <JPEGDEC.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключ из Pollinations
const char* apiKey = "YOUR_API_KEY_HERE";

// Email для MyMemory (необязательно)
const char* myMemoryEmail = "your_email@gmail.com";

// Пины SD карты (из вашей конфигурации)
#define SD_MISO   13
#define SD_MOSI   11
#define SD_SCK    12
#define SD_CS     10

// Создаем объект генератора
NEURGenerator generator;

// Callback для декодирования (необязательно, для проверки)
int JPEGCheck(JPEGDRAW *pDraw) {
    return 1; // Просто пропускаем
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== NEURGenerator 1.2.0 - SD Card Save Example ===");
    
    // Инициализация SD карты
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        Serial.println("❌ Ошибка инициализации SD карты");
        return;
    }
    Serial.println("✅ SD карта инициализирована");
    
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
    
    // Проверяем баланс
    if (generator.checkBalance()) {
        Serial.print("💰 Баланс: ");
        Serial.println(generator.getBalance());
    }
    
    // Массив промптов для генерации
    const char* prompts[] = {
        "красивый закат над морем",
        "киберпанк город будущего",
        "космический корабль в туманности",
        "фантастический лес с грибами",
        "портрет девушки в стиле аниме"
    };
    
    int numPrompts = sizeof(prompts) / sizeof(prompts[0]);
    
    for (int i = 0; i < numPrompts; i++) {
        Serial.printf("\n--- Генерация %d/%d ---\n", i + 1, numPrompts);
        
        // Готовим промпт
        if (!generator.preparePrompt(prompts[i])) {
            Serial.println("❌ Ошибка подготовки промпта");
            continue;
        }
        
        Serial.print("📝 Промпт: ");
        Serial.println(generator.getPreparedPrompt());
        
        // Генерируем изображение
        if (generator.generate()) {
            Serial.println("✅ Изображение сгенерировано!");
            Serial.printf("📊 Размер JPEG: %d байт\n", generator.getImageDataSize());
            
            // Проверяем, что это валидный JPEG
            JPEGDEC jpeg;
            if (jpeg.openRAM(generator.getImageData(), generator.getImageDataSize(), JPEGCheck)) {
                Serial.printf("🖼️ Размер изображения: %dx%d\n", jpeg.getWidth(), jpeg.getHeight());
                jpeg.close();
            } else {
                Serial.println("⚠️ Предупреждение: Невалидный JPEG");
            }
            
            // Создаем имя файла
            char filename[64];
            snprintf(filename, sizeof(filename), "/image_%d.jpg", i + 1);
            
            // Сохраняем на SD карту
            File file = SD.open(filename, FILE_WRITE);
            if (file) {
                size_t written = file.write(generator.getImageData(), generator.getImageDataSize());
                file.close();
                
                if (written == generator.getImageDataSize()) {
                    Serial.printf("✅ Файл сохранен: %s (%d байт)\n", filename, written);
                } else {
                    Serial.printf("❌ Ошибка записи: записано %d из %d\n", written, generator.getImageDataSize());
                }
            } else {
                Serial.println("❌ Не удалось создать файл на SD карте");
            }
            
            // Очищаем буфер для следующего изображения
            generator.clearImageData();
            
        } else {
            Serial.print("❌ Ошибка генерации: ");
            Serial.println(generator.getLastError());
        }
        
        // Небольшая пауза между генерациями
        delay(2000);
    }
    
    Serial.println("\n✅ Все генерации завершены");
}

void loop() {
    generator.tick();
    generator.resetWDT();
    delay(10);
}