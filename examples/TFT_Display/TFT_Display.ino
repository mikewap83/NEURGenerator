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
#define USE_WDT 0  // 1 - использовать WDT, 0 - не использовать WDT

#if USE_WDT
#include <esp_err.h>
#include <esp_task_wdt.h>

esp_task_wdt_config_t twdt_config = {
  .timeout_ms = 10000,  // 10 секунд
  .idle_core_mask = (1 << 2) - 1,
  .trigger_panic = false,
};
#endif

// Создаем объект генератора
NEURGenerator generator;

TFT_eSPI tft = TFT_eSPI();

// Для декодирования JPEG
JPEGDEC jpeg;

// Callback для отрисовки JPEG
int JPEGDraw(JPEGDRAW *pDraw) {
  tft.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}

// CALLBACK при начале генерации
void onRenderRun() {
  Serial.println("🎨 Начало генерации...");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Generating...", 10, 10);
}

// CALLBACK при успешном завершении генерации
void onRenderEnd() {
  Serial.println("✅ Генерация завершена!");

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

    // Очищаем буфер (опционально)
    // generator.clearImageData();
  }
}

// CALLBACK при ошибке генерации
void onRenderErr() {
  Serial.print("❌ Ошибка генерации: ");
  Serial.println(generator.getStateStatus(true));
  tft.fillScreen(TFT_RED);
  tft.drawString("Gen Error", 10, 10);
}

// CALLBACK при принудительной остановке
void onRenderUnd() {
  Serial.println("⏸️ Генерация прервана");
  tft.fillScreen(TFT_ORANGE);
  tft.drawString("Stopped", 10, 10);
}

// CALLBACK при успешном переводе
void onRenderEng() {
  Serial.println("🌐 Перевод выполнен");
  tft.drawString("Translation OK", 10, 50);
}

// НОВЫЕ CALLBACK'и
void onRenderTft() {
  Serial.println("🖥️ Изображение готово для TFT");
  // Можно сразу отобразить, но onRenderEnd уже делает это
}

void onRenderRet() {
  Serial.println("🔄 Повторная загрузка изображения");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Retry...", 10, 10);
}

void onRenderDel() {
  Serial.println("🗑️ Изображение удалено (недоступно)");
  tft.fillScreen(TFT_RED);
  tft.drawString("Image Deleted", 10, 10);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator 2.0.0 - TFT Display Example ===");

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
  // Инициализация WDT
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
  generator.setUsePings(true);      // Использовать ping
  generator.setUseLoges(true);      // Выводить логи
  generator.setUseScreen(true);     // включить обработку экрана

  generator.setAPIFreely(false);    // платный режим
  generator.setAPIAdjust(false);    // отключить адаптивные размеры
  generator.setAPISwitch(true);     // включить перевод

  // Настройка модели
  generator.setAPINumber(0);             // индекс модели
  generator.setAPIModels("dreamshaper"); // модель
  generator.setAPILevels(1);             // качество (0-низкое, 1-среднее, 2-высокое)
  generator.setAPIScales(1);             // размер (0-маленький, 1-средний, 2-большой)
  generator.setAPIEnhanc(false);         // улучшение
  generator.setAPIFilter(false);         // фильтр

#if USE_WDT
  generator.setUseTasks(true);      // Разрешить сброс WDT
  generator.setWDT(10000, &twdt_config);
  Serial.println("✅ Библиотека будет сбрасывать WDT");
#else
  generator.setUseTasks(false);     // Запретить сброс WDT
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
    tft.drawString("Loading config...", 10, 50);
    if (generator.load_config_from_file("/config.json") == 0) {
      Serial.println("⚠️ Конфиг не найден, создаем пример...");
      generator.create_example_config("/config.json");
    }

    // Проверяем баланс
    Serial.println("\n💰 Запрос баланса...");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Balance...", 10, 10);

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

    if (generator.getAPIModelsNamesCount() > 0) {
      Serial.println("\n📋 Доступные модели:");
      for (uint8_t i = 0; i < generator.getAPIModelsNamesCount(); i++) {
        Serial.printf("  %d: %s - %s (%s)\n",
                      i,
                      generator.getAPIModelsNamesByIndex(i),
                      generator.getAPIModelsTitleByIndex(i),
                      generator.getAPIModelsPriceByIndex(i));
      }
    }

    // Готовим промпт
    Serial.println("\n📝 Подготовка промпта...");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Preparing...", 10, 10);

    if (generator.data_prepare(
          "красивый закат над морем, пальмы, песок",  // промпт
          "high quality, detailed",                   // суффикс
          "digital art",                               // модификатор
          "ugly, blurry"                               // negative prompt
        )) {
      Serial.println("✅ Промпт подготовлен");
      tft.drawString("Prompt OK", 10, 30);

      // Отправляем запрос
      Serial.println("\n📤 Отправка запроса...");
      tft.drawString("Sending...", 10, 50);

      if (generator.send_request()) {
        Serial.println("✅ Запрос отправлен");
        // Дальше работают callback'и
      } else {
        Serial.print("❌ Ошибка отправки: ");
        Serial.println(generator.getStateStatus(false));
        tft.fillScreen(TFT_RED);
        tft.drawString("Send Error", 10, 10);
      }
    } else {
      Serial.print("❌ Ошибка подготовки промпта: ");
      Serial.println(generator.getStateStatus(false));
      tft.fillScreen(TFT_RED);
      tft.drawString("Prepare Error", 10, 10);
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
