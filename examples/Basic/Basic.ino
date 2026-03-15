#include <NEURGenerator.h>

// Настройки WiFi
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API ключ из Pollinations
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
NEURGenerator* generator = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator Basic Example ===");
  Serial.println("Версия библиотеки: 1.2.5");

#if USE_WDT
  // Инициализация WDT (как в вашем AIFrameNEUR.ino)
  esp_task_wdt_deinit();           // wdt is enabled by default, so we need to deinit it first
  esp_task_wdt_init(&twdt_config); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);
  Serial.println("✅ WDT инициализирован");
#endif

  // Создаем объект генератора с ключами
  generator = new NEURGenerator(apiKey, privateKey, myMemoryEmail);

  // Настройка параметров (опционально)
  generator->setUseHeads(true);      // Использовать заголовки
  generator->setUsePings(true);      // Использовать ping
  generator->setUseLoges(true);      // Выводить логи

#if USE_WDT
  generator->setUseTasks(true);      // Разрешить библиотеке вызывать esp_task_wdt_reset()
  generator->setWDT(10000, &twdt_config); // Передаем конфигурацию WDT
  Serial.println("✅ WDT передан в библиотеку");
#else
  generator->setUseTasks(false);     // Запретить библиотеке вызывать esp_task_wdt_reset()
  Serial.println("ℹ️ WDT не используется");
#endif

  // Настройка таймаутов (опционально)
  generator->setAttempts(30000, 15000, 5, 5);

  // Подключаемся к WiFi
  Serial.print("\n📡 Подключение к WiFi");
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;

#if USE_WDT
    esp_task_wdt_reset(); // Сбрасываем WDT
#endif
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi подключен");
    Serial.print("📊 IP адрес: ");
    Serial.println(WiFi.localIP());

    // Проверяем баланс через API
    Serial.println("\n💰 Запрос баланса...");

    if (generator->getApiPollen(apiKey)) {
      Serial.print("✅ Баланс: ");
      Serial.print(generator->getPollen());
      Serial.println(" pollen");
    } else {
      Serial.print("❌ Ошибка получения баланса: ");
      Serial.println(generator->getStateStatus(false));
    }

  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
  }
}

void loop() {
  // Тикаем генератор для обработки таймеров
  if (generator) {
    generator->tick(WiFi.status() == WL_CONNECTED);
  }

#if USE_WDT
  esp_task_wdt_reset(); // Сбрасываем WDT в основном цикле
#endif

  delay(100);
}
