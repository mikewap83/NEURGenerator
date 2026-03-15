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
  .idle_core_mask = (1 << CONFIG_FREERTOS_NUMBER_OF_CORES) - 1,
  .trigger_panic = false,
};
#endif

// Создаем объект генератора
NEURGenerator* generator = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== NEURGenerator 1.2.0 - Генерация изображений ===");

#if USE_WDT
  // Инициализация WDT в проекте
  esp_task_wdt_deinit();
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);
  Serial.println("✅ WDT инициализирован в проекте");
#endif

  // Создаем объект генератора с ключами
  generator = new NEURGenerator(apiKey, privateKey, myMemoryEmail);

  // Настройка параметров
  generator->setUseHeads(true);      // Использовать заголовки
  generator->setUsePings(true);      // Использовать ping
  generator->setUseLoges(true);      // Выводить логи

#if USE_WDT
  generator->setUseTasks(true);      // Разрешить библиотеке вызывать esp_task_wdt_reset()
  generator->setWDT(10000, &twdt_config); // Передаем конфигурацию для WDT_SURPLUS
  Serial.println("✅ Библиотека будет сбрасывать WDT");
#else
  generator->setUseTasks(false);     // Запретить библиотеке вызывать esp_task_wdt_reset()
  Serial.println("ℹ️ Библиотека не будет трогать WDT");
#endif

  // Настройка таймаутов
  generator->setAttempts(30000, 15000, 5, 5);

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

    // Проверяем баланс
    Serial.println("\n💰 Запрос баланса...");

    if (generator->getApiPollen(apiKey)) {
      Serial.print("✅ Баланс: ");
      Serial.print(generator->getPollen());
      Serial.println(" pollen");
    } else {
      Serial.print("❌ Ошибка баланса: ");
      Serial.println(generator->getStateStatus(false));
    }

    // Готовим промпт
    Serial.println("\n📝 Подготовка промпта...");

    if (generator->data_prepare(
          "a beautiful cat in space, cyberpunk style",  // промпт
          "high quality, detailed",                     // суффикс
          "digital art",                                 // модификатор
          "ugly, blurry, bad quality",                  // negative prompt
          false                                          // перевод (false = не переводить)
        )) {
      Serial.print("✅ Промпт подготовлен: ");
      Serial.println(generator->getStateStatus(false));

      // Отправляем запрос
      Serial.println("\n📤 Отправка запроса...");

      if (generator->send_request()) {
        Serial.println("✅ Запрос отправлен, ожидание результата...");
      } else {
        Serial.print("❌ Ошибка отправки: ");
        Serial.println(generator->getStateStatus(false));
      }
    } else {
      Serial.print("❌ Ошибка подготовки промпта: ");
      Serial.println(generator->getStateStatus(false));
    }

  } else {
    Serial.println("\n❌ Ошибка подключения к WiFi");
  }
}

void loop() {
  // Тикаем генератор для обработки всех процессов
  if (generator) {
    generator->tick(WiFi.status() == WL_CONNECTED);

    // Проверяем статус генерации
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 2000) {
      lastStatus = millis();

      if (generator->isGenerating()) {
        Serial.print("⏳ Статус: ");
        Serial.println(generator->getStateStatus(true));
      }

      // Если изображение готово
      if (generator->state_gen == NEURGenerator::Status::OK_GENERATING_READILY) {
        Serial.println("\n✅ Изображение сгенерировано!");
        Serial.print("📊 Размер JPEG: ");
        Serial.print(generator->getImageDataSize());
        Serial.println(" байт");
      }
    }
  }

  delay(100);
}
