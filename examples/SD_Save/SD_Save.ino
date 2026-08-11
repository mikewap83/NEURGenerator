#include <NEURGenerator.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>
#include <JPEGDEC.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключи из Pollinations
const char* apiKey = "YOUR_SK_API_KEY_HERE";
const char* privateKey = "YOUR_PK_API_KEY_HERE";
const char* myMemoryEmail = "YOUR_EMAIL@DOMAIN.COM";

// Пины SD карты
#define SD_MISO   13
#define SD_MOSI   11
#define SD_SCK    12
#define SD_CS     10

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

// Счетчик для имен файлов
int imageCounter = 0;

// CALLBACK при начале генерации
void onRenderRun() {
  Serial.println("🎨 Начало генерации...");
}

// CALLBACK при успешном завершении генерации
void onRenderEnd() {
  Serial.println("✅ Генерация завершена!");

  // Получаем данные изображения
  uint8_t* imageData = generator.getImageData();
  size_t imageSize = generator.getImageDataSize();

  if (imageData && imageSize > 0) {
    // Создаем имя файла
    char filename[64];
    snprintf(filename, sizeof(filename), "/image_%d.jpg", ++imageCounter);

    // Сохраняем на SD карту
    File file = SD.open(filename, FILE_WRITE);
    if (file) {
      size_t written = file.write(imageData, imageSize);
      file.close();

      if (written == imageSize) {
        Serial.printf("✅ Файл сохранен: %s (%d байт)\n", filename, written);
      } else {
        Serial.printf("❌ Ошибка записи: записано %d из %d\n", written, imageSize);
      }
    } else {
      Serial.println("❌ Не удалось создать файл на SD карте");
    }

    // Очищаем буфер (опционально)
    // generator.clearImageData();
  }
}

// CALLBACK при ошибке генерации
void onRenderErr() {
  Serial.print("❌ Ошибка генерации: ");
  Serial.println(generator.getStateStatus(true));
}

// CALLBACK при принудительной остановке
void onRenderUnd() {
  Serial.println("⏸️ Генерация прервана");
}

// CALLBACK при успешном переводе
void onRenderEng() {
  Serial.println("🌐 Перевод выполнен");
}

// CALLBACK'и
void onRenderTft() {
  Serial.println("🖥️ Изображение готово для TFT");
}

void onRenderRet() {
  Serial.println("🔄 Повторная загрузка изображения");
}

void onRenderDel() {
  Serial.println("🗑️ Изображение удалено (недоступно)");
}

// Callback для проверки JPEG
int JPEGCheck(JPEGDRAW *pDraw) {
  return 1; // Просто пропускаем
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator 2.0.0 - SD Card Save Example ===");

  if (!LittleFS.begin()) {
    Serial.println("❌ Ошибка монтирования LittleFS");
    return;
  }
  Serial.println("✅ LittleFS смонтирована");

  // Инициализация SD карты
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ Ошибка инициализации SD карты");
    return;
  }
  Serial.println("✅ SD карта инициализирована");

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
  generator.setUseScreen(false);    // отключить обработку экрана

  // НАСТРОЙКИ API
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
    Serial.print("📊 IP адрес: ");
    Serial.println(WiFi.localIP());

    Serial.println("\n📦 Загрузка конфига моделей...");
    if (generator.load_config_from_file("/config.json") == 0) {
      Serial.println("⚠️ Конфиг не найден, создаем пример...");
      generator.create_example_config("/config.json");
    }

    // Проверяем баланс
    Serial.println("\n💰 Запрос баланса...");

    if (generator.getApiPollen(apiKey)) {
      Serial.print("✅ Баланс: ");
      Serial.print(generator.getPollen());
      Serial.println(" pollen");
    } else {
      Serial.print("❌ Ошибка баланса: ");
      Serial.println(generator.getStateStatus(false));
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

      if (!generator.data_prepare(
            prompts[i],           // промпт
            "high quality",       // суффикс
            "detailed",           // модификатор
            "ugly, blurry"        // negative prompt
          )) {
        Serial.print("❌ Ошибка подготовки промпта: ");
        Serial.println(generator.getStateStatus(false));
        continue;
      }

      Serial.print("📝 Промпт подготовлен");

      // Отправляем запрос на генерацию
      if (generator.send_request()) {
        Serial.println(" - запрос отправлен");
        Serial.println("⏳ Ожидание генерации...");

        // Ждем завершения генерации (callback onRenderEnd сработает автоматически)
        uint32_t timeout = millis() + 30000; // 30 секунд таймаут
        while (generator.isGenerating() && millis() < timeout) {
          generator.tick(WiFi.status() == WL_CONNECTED);
          delay(100);

#if USE_WDT
          esp_task_wdt_reset();
#endif
        }

        if (millis() >= timeout) {
          Serial.println("❌ Таймаут генерации");
          generator.stop_receive();
        }

      } else {
        Serial.print("❌ Ошибка отправки запроса: ");
        Serial.println(generator.getStateStatus(false));
      }

      // Пауза между генерациями
      delay(2000);
    }

    Serial.println("\n✅ Все генерации завершены");

  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
  }
}

void loop() {
  // Тикаем генератор для обработки таймеров
  generator.tick(WiFi.status() == WL_CONNECTED);

#if USE_WDT
  esp_task_wdt_reset();
#endif

  delay(100);
}
