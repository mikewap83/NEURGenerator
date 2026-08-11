#include <NEURGenerator.h>
#include <FS.h>
#include <LittleFS.h>

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

NEURGenerator generator;

// ⭐ CALLBACK'и
void onRenderRun() {
  Serial.println("🎨 Начало генерации...");
}

void onRenderEnd() {
  Serial.println("✅ Генерация завершена!");
  Serial.printf("📊 Размер JPEG: %d байт\n", generator.getImageDataSize());
}

void onRenderErr() {
  Serial.printf("❌ Ошибка генерации: %s\n", generator.getStateStatus(true));
}

void onRenderEng() {
  Serial.println("🌐 Перевод выполнен");
}

void onRenderUnd() {
  Serial.println("⏸️ Генерация прервана");
}

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

  Serial.println("\n=== NEURGenerator 2.0.0 - Генерация изображений ===");

  // ⭐ ИНИЦИАЛИЗАЦИЯ LittleFS
  if (!LittleFS.begin()) {
    Serial.println("❌ Ошибка монтирования LittleFS");
    return;
  }
  Serial.println("✅ LittleFS смонтирована");

#if USE_WDT
  esp_task_wdt_deinit();
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("✅ WDT инициализирован в проекте");
#endif

  // Устанавливаем ключи
  generator.setKeySecret(apiKey, privateKey);
  generator.setMyMemmory(myMemoryEmail);

  // ⭐ CALLBACK'и
  generator.onRenderRun(onRenderRun);
  generator.onRenderEnd(onRenderEnd);
  generator.onRenderErr(onRenderErr);
  generator.onRenderEng(onRenderEng);
  generator.onRenderUnd(onRenderUnd);
  generator.onRenderTft(onRenderTft);
  generator.onRenderRet(onRenderRet);
  generator.onRenderDel(onRenderDel);

  // Настройка параметров
  generator.setUseLoges(true);
  generator.setUseScreen(true);

  // ⭐ НОВЫЕ НАСТРОЙКИ API
  generator.setAPIFreely(false);
  generator.setAPIAdjust(false);
  generator.setAPISwitch(true);

  // Настройка модели
  generator.setAPINumber(0);
  generator.setAPIModels("dreamshaper");
  generator.setAPILevels(1);
  generator.setAPIScales(1);
  generator.setAPIEnhanc(false);
  generator.setAPIFilter(false);

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
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi подключен");
    Serial.print("📊 IP адрес: ");
    Serial.println(WiFi.localIP());

    // ⭐⭐ ЗАГРУЖАЕМ КОНФИГ ОДНОЙ СТРОКОЙ!
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
      return;
    }

    // ⭐ ПОКАЗЫВАЕМ ЗАГРУЖЕННЫЕ МОДЕЛИ
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

    if (generator.data_prepare(
          "красивый кот в космосе, киберпанк стиль",
          "high quality, detailed",
          "digital art",
          "ugly, blurry, bad quality"
        )) {
      Serial.print("✅ Промпт подготовлен: ");
      Serial.println(generator.getStateStatus(false));

      // Отправляем запрос
      Serial.println("\n📤 Отправка запроса...");

      if (generator.send_request()) {
        Serial.println("✅ Запрос отправлен, ожидание результата...");
      } else {
        Serial.print("❌ Ошибка отправки: ");
        Serial.println(generator.getStateStatus(false));
      }
    } else {
      Serial.print("❌ Ошибка подготовки промпта: ");
      Serial.println(generator.getStateStatus(false));
    }

  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
  }
}

void loop() {
  generator.tick(WiFi.status() == WL_CONNECTED);

  static uint32_t lastStatus = 0;
  if (millis() - lastStatus > 2000) {
    lastStatus = millis();

    if (generator.isGenerating()) {
      Serial.print("⏳ Статус: ");
      Serial.println(generator.getStateStatus(true));
    }

    if (generator.state_gen == NEURGenerator::Status::OK_GENERATING_READILY) {
      Serial.println("\n✅ Изображение сгенерировано!");
      Serial.print("📊 Размер JPEG: ");
      Serial.print(generator.getImageDataSize());
      Serial.println(" байт");
    }
  }

#if USE_WDT
  esp_task_wdt_reset();
#endif

  delay(100);
}
