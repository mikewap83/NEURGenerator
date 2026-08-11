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

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator Basic Example ===");
  Serial.println("Версия библиотеки: 2.0.0");

  if (!LittleFS.begin()) {
    Serial.println("❌ Ошибка монтирования LittleFS");
    return;
  }
  Serial.println("✅ LittleFS смонтирована");

#if USE_WDT
  esp_task_wdt_deinit();
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("✅ WDT инициализирован");
#endif

  // Устанавливаем ключи
  generator.setKeySecret(apiKey, privateKey);
  generator.setMyMemmory(myMemoryEmail);

  // Настройка параметров
  generator.setUsePings(true);
  generator.setUseLoges(true);
  generator.setUseScreen(false);

  generator.setAPIFreely(false);
  generator.setAPIAdjust(false);
  generator.setAPISwitch(true);

#if USE_WDT
  generator.setUseTasks(true);
  generator.setWDT(10000, &twdt_config);
  Serial.println("✅ WDT передан в библиотеку");
#else
  generator.setUseTasks(false);
  Serial.println("ℹ️ WDT не используется");
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
      Serial.print("❌ Ошибка получения баланса: ");
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

  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
  }
}

void loop() {
  generator.tick(WiFi.status() == WL_CONNECTED);

#if USE_WDT
  esp_task_wdt_reset();
#endif

  delay(100);
}
