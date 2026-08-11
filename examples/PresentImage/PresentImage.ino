#include <NEURGenerator.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <JPEGDEC.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключи из Pollinations
const char* apiKey = "YOUR_SK_API_KEY_HERE";
const char* privateKey = "YOUR_PK_API_KEY_HERE";
const char* myMemoryEmail = "YOUR_EMAIL@DOMAIN.COM";

// ========== НАСТРОЙКИ WDT ==========
#define USE_WDT 0

#if USE_WDT
#include <esp_err.h>
#include <esp_task_wdt.h>

esp_task_wdt_config_t twdt_config = {
  .timeout_ms = 10000,
  .idle_core_mask = (1 << 2) - 1,
  .trigger_panic = false,
};
#endif

// Создаем объект генератора
NEURGenerator generator;

TFT_eSPI tft = TFT_eSPI();
JPEGDEC jpeg;

// Callback для отрисовки JPEG
int JPEGDraw(JPEGDRAW *pDraw) {
  tft.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}

// CALLBACK при загрузке существующего изображения
void onRenderRun() {
  Serial.println("🎨 Начало загрузки изображения...");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Loading...", 10, 10);
}

// CALLBACK при успешной загрузке
void onRenderEnd() {
  Serial.println("✅ Изображение загружено!");

  // Получаем данные изображения
  uint8_t* jpegData = generator.getImageData();
  size_t jpegSize = generator.getImageDataSize();

  if (jpegData && jpegSize > 0) {
    Serial.printf("📊 Размер JPEG: %d байт\n", jpegSize);

    tft.fillScreen(TFT_BLACK);
    tft.drawString("Decoding...", 10, 10);

    // Декодируем и отображаем JPEG
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
  }
}

// CALLBACK при ошибке загрузки
void onRenderErr() {
  Serial.print("❌ Ошибка загрузки: ");
  Serial.println(generator.getStateStatus(true));
  tft.fillScreen(TFT_RED);
  tft.drawString("Load Error", 10, 10);
}

// CALLBACK при принудительной остановке
void onRenderUnd() {
  Serial.println("⏸️ Загрузка прервана");
  tft.fillScreen(TFT_ORANGE);
  tft.drawString("Stopped", 10, 10);
}

// CALLBACK при успешном переводе
void onRenderEng() {
  Serial.println("🌐 Перевод выполнен");
}

// НОВЫЕ CALLBACK'и (для PresentImage)
void onRenderTft() {
  Serial.println("🖥️ Изображение готово для TFT");
}

void onRenderRet() {
  Serial.println("🔄 Повторная загрузка изображения");
}

void onRenderDel() {
  Serial.println("🗑️ Изображение удалено (недоступно)");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator 2.0.0 - PresentImage Example ===");

  // Инициализация LittleFS
  if (!LittleFS.begin()) {
    Serial.println("❌ Ошибка монтирования LittleFS");
    return;
  }
  Serial.println("✅ LittleFS смонтирована");

  // Инициализация TFT
  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Starting...", 10, 10);

#if USE_WDT
  esp_task_wdt_deinit();
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("✅ WDT инициализирован");
#endif

  // Устанавливаем ключи
  generator.setKeySecret(apiKey, privateKey);
  generator.setMyMemmory(myMemoryEmail);

  // Устанавливаем callback'и
  generator.onRenderRun(onRenderRun);
  generator.onRenderEnd(onRenderEnd);
  generator.onRenderErr(onRenderErr);
  generator.onRenderUnd(onRenderUnd);
  generator.onRenderEng(onRenderEng);
  generator.onRenderTft(onRenderTft);
  generator.onRenderRet(onRenderRet);
  generator.onRenderDel(onRenderDel);

  // Настройка параметров
  generator.setUsePings(true);
  generator.setUseLoges(true);
  generator.setUseScreen(true);

  // (для PresentImage не обязательны, но нужны для работы)
  generator.setAPIFreely(false);
  generator.setAPIAdjust(false);
  generator.setAPISwitch(true);

#if USE_WDT
  generator.setUseTasks(true);
  generator.setWDT(10000, &twdt_config);
  Serial.println("✅ Библиотека будет сбрасывать WDT");
#else
  generator.setUseTasks(false);
  Serial.println("ℹ️ Библиотека не будет трогать WDT");
#endif

  // Настройка таймаутов
  generator.setAttempts(30000, 15000, 5, 5);

  // Подключаемся к WiFi
  Serial.print("\n📡 Подключение к WiFi");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting...", 10, 10);

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;

#if USE_WDT
    esp_task_wdt_reset();
#endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi подключен");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("WiFi OK", 10, 10);
    tft.drawString(WiFi.localIP().toString().c_str(), 10, 30);

    Serial.println("\n📦 Загрузка конфига моделей...");
    if (generator.load_config_from_file("/config.json") == 0) {
      Serial.println("⚠️ Конфиг не найден, создаем пример...");
      generator.create_example_config("/config.json");
    }

    // Проверяем баланс
    Serial.println("\n💰 Запрос баланса...");
    tft.drawString("Balance...", 10, 50);

    if (generator.getApiPollen(apiKey)) {
      Serial.print("✅ Баланс: ");
      Serial.print(generator.getPollen());
      Serial.println(" pollen");

      tft.fillScreen(TFT_BLACK);
      tft.drawString("Balance OK", 10, 10);
      tft.drawString(generator.getPollen(), 10, 30);
      tft.drawString("pollen", 10, 50);
    } else {
      Serial.print("❌ Ошибка баланса: ");
      Serial.println(generator.getStateStatus(false));
      tft.fillScreen(TFT_RED);
      tft.drawString("Balance Error", 10, 10);
      return;
    }

    // ГЛАВНОЕ: ЗАГРУЗКА СУЩЕСТВУЮЩЕГО ИЗОБРАЖЕНИЯ
    // Это может быть URL из истории, из файла или из любого другого источника

    const char* imageUrl = "https://gen.pollinations.ai/image/beautiful_sunset?model=dreamshaper&seed=12345";

    Serial.println("\n📥 Загрузка изображения по URL:");
    Serial.println(imageUrl);

    tft.fillScreen(TFT_BLACK);
    tft.drawString("Loading Image...", 10, 10);

    generator.PresentImage(imageUrl);

    // Ждем завершения загрузки
    uint32_t timeout = millis() + 30000;
    while (generator.isGenerating() && millis() < timeout) {
      generator.tick(WiFi.status() == WL_CONNECTED);
      delay(100);

      // Показываем статус
      static uint32_t lastStatus = 0;
      if (millis() - lastStatus > 2000) {
        lastStatus = millis();
        Serial.printf("⏳ Статус: %s\n", generator.getStateStatus(true));
      }

#if USE_WDT
      esp_task_wdt_reset();
#endif
    }

    if (millis() >= timeout) {
      Serial.println("❌ Таймаут загрузки");
      generator.stop_receive();
    }
  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
    tft.fillScreen(TFT_RED);
    tft.drawString("WiFi Error", 10, 10);
  }
}

void loop() {
  // Тикаем генератор для обработки таймеров
  generator.tick(WiFi.status() == WL_CONNECTED);

#if USE_WDT
  esp_task_wdt_reset();
#endif

  delay(10);
}
