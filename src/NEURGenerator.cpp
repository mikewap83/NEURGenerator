#include "NEURGenerator.h"

// Определяем статический член
NEURGenerator* NEURGenerator::self = nullptr;

// Определяем статические члены
uint8_t* NEURGenerator::http_psram_buffer = nullptr;

// Расширение буфера
bool NEURGenerator::ExpandBuffer(char*& buffer, size_t& current_size, size_t needed,
                                 size_t expand_step, size_t max_size) {
  if (needed <= current_size) return true;
  if (needed > max_size) return false;

  size_t new_size = current_size + expand_step;
  while (new_size < needed) {
    new_size += expand_step;
  }
  if (new_size > max_size) new_size = max_size;
  if (new_size <= current_size) return false;

  char* new_buf = (char*)ps_realloc(buffer, new_size);
  if (!new_buf) return false;

  buffer = new_buf;
  current_size = new_size;
  return true;
}

// Получение размеров модели из конфига
bool NEURGenerator::getModelScale(const char* model_name, uint8_t level,
                                  uint16_t& _requestW, uint16_t& _requestH,
                                  uint16_t& max_dimensionW, uint16_t& max_dimensionH) {
  // 1. Проверяем, что конфиг загружен и модель существует
  if (!_config_loaded || !_models_config || !model_name) {
    return false;
  }

  // 2. Ищем модель по имени
  size_config* found = nullptr;
  for (uint8_t i = 0; i < _model_configs; i++) {
    if (_models_config[i].names && strcmp(_models_config[i].names, model_name) == 0) {
      found = &_models_config[i];
      break;
    }
  }

  // 3. Если модель не найдена
  if (!found) {
    if (flags.useloges) {
      Serial.printf("⚠️ Модель '%s' не найдена в config.json\n", model_name);
    }
    return false;
  }

  // 4. Берём максимальные размеры
  max_dimensionW = found->max_dimensionW;
  max_dimensionH = found->max_dimensionH;

  // 5. Ищем нужный уровень
  for (uint8_t i = 0; i < found->scales_count; i++) {
    if (found->scales[i].size_level == level) {
      _requestW = found->scales[i].dimensionW;
      _requestH = found->scales[i].dimensionH;
      return true;
    }
  }

  // 6. Уровень не найден - берём первый доступный
  if (found->scales_count > 0) {
    _requestW = found->scales[0].dimensionW;
    _requestH = found->scales[0].dimensionH;
    if (flags.useloges) {
      Serial.printf("⚠️ Уровень %d не найден для модели '%s', взят первый: %dx%d\n",
                    level, model_name, _requestW, _requestH);
    }
    return true;
  }

  // 7. Совсем ничего нет
  return false;
}

uint8_t NEURGenerator::load_config(const char* jsonData, size_t jsonSize) {
  if (!jsonData || jsonSize == 0) {
    _config_loaded = false;
    return 0;
  }

  gson::Parser json;
  if (!json.parse(jsonData, jsonSize)) {
    if (flags.useloges) Serial.println("❌ Ошибка парсинга config.json");
    _config_loaded = false;
    return 0;
  }

  if (!_models_config) {
    _models_config = (size_config*)ps_malloc(API_MODELS_COUNT * sizeof(size_config));
    if (!_models_config) {
      if (flags.useloges) Serial.println("❌ Не удалось выделить PSRAM для size_config");
      _config_loaded = false;
      return 0;
    }
  }

  memset(_models_config, 0, API_MODELS_COUNT * sizeof(size_config));
  _model_configs = 0;

  uint8_t models_len = json["models"].length();
  if (models_len == 0) {
    if (flags.useloges) Serial.println("⚠️ В config.json нет моделей");
    _config_loaded = false;
    return 0;
  }

  if (models_len > API_MODELS_COUNT) models_len = API_MODELS_COUNT;

  for (uint8_t i = 0; i < models_len; i++) {
    size_config* cfg = &_models_config[_model_configs];

    const char* name = json["models"][i]["name"].c_str();
    if (!name || name[0] == '\0') {
      continue;
    }

    cfg->names = (char*)ps_malloc(strlen(name) + 1);
    if (cfg->names) {
      strcpy(cfg->names, name);
    }

    cfg->max_dimensionW = json["models"][i]["max_dimensionW"].toInt();
    if (cfg->max_dimensionW == 0) cfg->max_dimensionW = 960;

    cfg->max_dimensionH = json["models"][i]["max_dimensionH"].toInt();
    if (cfg->max_dimensionH == 0) cfg->max_dimensionH = 640;

    uint8_t scales_len = json["models"][i]["scales"].length();
    if (scales_len > 3) scales_len = 3;
    cfg->scales_count = scales_len;

    for (uint8_t j = 0; j < scales_len; j++) {
      cfg->scales[j].size_level = json["models"][i]["scales"][j]["level"].toInt();
      cfg->scales[j].dimensionW = json["models"][i]["scales"][j]["width"].toInt();
      cfg->scales[j].dimensionH = json["models"][i]["scales"][j]["height"].toInt();
    }

    _model_configs++;

    if (flags.useloges) {
      Serial.printf("✅ Загружена модель: %s, max: %dx%d\n",
                    cfg->names ? cfg->names : "?",
                    cfg->max_dimensionW, cfg->max_dimensionH);
    }
  }

  _config_loaded = (_model_configs > 0);

  if (flags.useloges) {
    Serial.printf("📦 Загружено %d моделей из config.json\n", _model_configs);
  }

  return _model_configs;
}

uint8_t NEURGenerator::load_config_from_file(const char* filename) {
  if (!filename) filename = "/config.json";

  // Проверяем наличие файла
  if (!LittleFS.exists(filename)) {
    if (flags.useloges) Serial.printf("⚠️ Файл %s не найден\n", filename);
    return 0;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    if (flags.useloges) Serial.printf("❌ Не удалось открыть %s\n", filename);
    return 0;
  }

  size_t fileSize = file.size();
  if (fileSize == 0) {
    if (flags.useloges) Serial.printf("⚠️ Файл %s пустой\n", filename);
    file.close();
    return 0;
  }

  // Временный буфер для чтения
  char* buffer = (char*)ps_malloc(fileSize + 1);
  if (!buffer) {
    if (flags.useloges) Serial.println("❌ Не удалось выделить память для чтения config.json");
    file.close();
    return 0;
  }

  file.readBytes(buffer, fileSize);
  buffer[fileSize] = '\0';
  file.close();

  // Загружаем конфиг
  uint8_t result = load_config(buffer, fileSize);

  // Освобождаем временный буфер
  heap_caps_free(buffer);

  return result;
}

bool NEURGenerator::create_example_config(const char* filename) {
  if (!filename) filename = "/config.json";

  const char* config_json =
    "{\n"
    "  \"models\": [\n"
    "    {\n"
    "      \"name\": \"flux\",\n"
    "      \"max_dimensionW\": 1024,\n"
    "      \"max_dimensionH\": 768,\n"
    "      \"scales\": [\n"
    "        { \"level\": 0, \"width\": 512, \"height\": 384 },\n"
    "        { \"level\": 1, \"width\": 768, \"height\": 576 },\n"
    "        { \"level\": 2, \"width\": 1024, \"height\": 768 }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"sana\",\n"
    "      \"max_dimensionW\": 576,\n"
    "      \"max_dimensionH\": 384,\n"
    "      \"scales\": [\n"
    "        { \"level\": 0, \"width\": 480, \"height\": 320 },\n"
    "        { \"level\": 1, \"width\": 528, \"height\": 352 },\n"
    "        { \"level\": 2, \"width\": 576, \"height\": 384 }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"dreamshaper\",\n"
    "      \"max_dimensionW\": 576,\n"
    "      \"max_dimensionH\": 384,\n"
    "      \"scales\": [\n"
    "        { \"level\": 0, \"width\": 480, \"height\": 320 },\n"
    "        { \"level\": 1, \"width\": 528, \"height\": 352 },\n"
    "        { \"level\": 2, \"width\": 576, \"height\": 384 }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"name\": \"zimage\",\n"
    "      \"max_dimensionW\": 960,\n"
    "      \"max_dimensionH\": 640,\n"
    "      \"scales\": [\n"
    "        { \"level\": 0, \"width\": 480, \"height\": 320 },\n"
    "        { \"level\": 1, \"width\": 720, \"height\": 480 },\n"
    "        { \"level\": 2, \"width\": 960, \"height\": 640 }\n"
    "      ]\n"
    "    }\n"
    "  ]\n"
    "}";

  File file = LittleFS.open(filename, "w");
  if (!file) {
    if (flags.useloges) Serial.printf("❌ Не удалось создать %s\n", filename);
    return false;
  }

  size_t written = file.print(config_json);
  file.close();

  if (written == 0) {
    if (flags.useloges) Serial.printf("❌ Ошибка записи в %s\n", filename);
    return false;
  }

  if (flags.useloges) Serial.printf("✅ Создан пример конфига: %s (%d байт)\n", filename, written);

  // Автоматически загружаем созданный конфиг
  return load_config_from_file(filename) > 0;
}

void NEURGenerator::setStateStatus(Status new_state) {
  state_gen = new_state;
  state_upd = true;

  last_apicommands = millis();
  
  switch (new_state) {
    case Status::OK_INITIALIZATION_API:
      break;

    case Status::OK_WAITING_COMMAND:
      break;

    case Status::OK_PREPARING_DATA:
      // Начало подготовки данных
      neur_timer.stop();
      break;

    case Status::OK_SENDING_REQUEST:
      if (!flags.repeated && _run_cb) {
        _run_cb();
      }

      // Данные подготовлены, готовы к отправке
      _str_generations = millis();
      break;

    case Status::OK_SENDING_ATTEMPT:
      break;

    case Status::OK_RECEIVING_REQUEST:
      if (flags.repeated) {
        (url_images[0] != '\0') ? neur_timer.start() : neur_timer.stop();

        _str_generations = millis();
      }
      break;

    case Status::OK_RECEIVING_ATTEMPT:
      break;

    case Status::OK_WAITING_FOR_RESULT:
      if (!flags.repeated) {
        (url_images[0] != '\0') ? neur_timer.start() : neur_timer.stop();

        _str_generations = millis();
      }
      break;

    case Status::OK_GENERATING_READILY:
      created_image++;
      _end_generations = SafeMillis(_str_generations, millis());
      _end_generations = (_end_generations / TIME_PERIOD) * TIME_PERIOD;

      if (_tft_cb) {
        _tft_cb();
      }

      if (!flags.repeated && _end_cb) {
        _end_cb();
      }

      neur_timer.stop();

      flags.repeated = false;
      break;

    case Status::OK_TRANSLATE:
      break;

    case Status::OK_DOWNLOADING:
      break;

    case Status::OK_RETRY_DOWNLOADING:
      if (_ret_cb) {
        _ret_cb();
      }
      break;

    case Status::GET_API_POLLEN:
    case Status::GET_API_POLLEN_OK:
    case Status::GET_API_POLLEN_ERR:
      break;

    case Status::GET_API_MODELS:
    case Status::GET_API_MODELS_OK:
    case Status::GET_API_MODELS_ERR:
      break;

    case Status::ERROR_AIGENERATION:
      memset(url_images, 0, sz_url_images);
      error.request++;

      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_REQUESTS:
      memset(url_images, 0, sz_url_images);
      error.request++;

      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_RESPONSE:
      memset(url_images, 0, sz_url_images);
      error.receive++;

      if (jpegDataBuf) {
        memset(jpegDataBuf, 0, sz_jpegDataBuf);
      }

      neur_timer.stop();
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_RECEIVING:
      memset(url_images, 0, sz_url_images);
      error.receive++;

      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_DECODINGS:
      memset(url_images, 0, sz_url_images);
      error.decoder++;

      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_CONNECTION:
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_INITMEMORY:
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_OVERLOAD:
      _end_generations = 120000UL;
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_AUTHENTICATE:
      _end_generations = 300000UL;
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_BALANCEBUDGET:
      _end_generations = 600000UL;
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_ACCESSDENIED:
      _end_generations = 300000UL;
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_LOADEDOLDIMAGES:
      _end_generations = 5000UL;
      memset(url_images, 0, sz_url_images);

      if (flags.repeated && _del_cb) {
        _del_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_UNAVAILABLE:
      _end_generations = 600000UL;
      if (!flags.repeated && _err_cb) {
        _err_cb();
      }

      flags.repeated = false;
      break;

    case Status::ERROR_TRANSLATE:
      break;

    case Status::ERROR_TRANSLATE_LIMIT:
      break;

    default:
      break;
  }
}

// system
void NEURGenerator::cleanupHttp() {
  if (http) {
    // 1. Пытаемся корректно закрыть соединение
    http->stop();
    http->flush();

    // 2. Определяем как был создан объект
    if (http_psram_buffer && (void*)http == (void*)http_psram_buffer) {
      // Объект создан через placement new в PSRAM
      http->~EspInsecureClient();  // Только деструктор!
      // Очищаем память (опционально, но полезно для отладки)
      memset(http_psram_buffer, 0, sizeof(ghttp::EspInsecureClient));
      http_psram_buffer = nullptr;  // Сбрасываем указатель на буфер
    } else {
      // Объект создан обычным new
      delete http;  // Полное удаление
    }

    // 3. Сбрасываем указатель
    http = nullptr;
  } else if (http_psram_buffer) {
    // На всякий случай: если буфер есть, но указатель http потерян
    ghttp::EspInsecureClient* obj = (ghttp::EspInsecureClient*)http_psram_buffer;
    obj->~EspInsecureClient();
    memset(http_psram_buffer, 0, sizeof(ghttp::EspInsecureClient));
    http_psram_buffer = nullptr;
  }
}

bool NEURGenerator::request_query(States states, const char* host, uint16_t port, const char* path, const char* method, ghttp::Client::FormData* data) {
  // ВСЕГДА очищаем предыдущий объект
  cleanupHttp();

  if (!http_psram_buffer) {
    http_psram_buffer = (uint8_t*)ps_malloc(sizeof(ghttp::EspInsecureClient));
    if (!http_psram_buffer) {
      setStateStatus(Status::ERROR_INITMEMORY);
      return false;
    }
  }

  WDT_sTimeout();//Запуск WDT

  if (wdt_enlarge && (states == States::TRANSLATE || states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
    esp_task_wdt_config_t long_wdt_config = {
      .timeout_ms = WDT_SURPLUS + try_clients + _wdt_config->timeout_ms,
      .idle_core_mask = _wdt_config->idle_core_mask,
      .trigger_panic = _wdt_config->trigger_panic
    };

    esp_task_wdt_reconfigure(&long_wdt_config);
  }

  // Создаем объект в PSRAM
  http = new (http_psram_buffer) ghttp::EspInsecureClient(host, port);

  // Получаем ссылку на клиент и настраиваем
  NetworkClientSecure* secureClient = static_cast<NetworkClientSecure*>(&http->client);
  secureClient->setHandshakeTimeout(try_clients / 1000);
  secureClient->setInsecure();

  http->setTimeout(try_clients);

  if (!http->connect()) {
    http->~EspInsecureClient();
    WDT_eTimeout(true);//Сброс WDT
    return false;
  }

  WDT_eTimeout(true);//Сброс WDT

  ghttp::Client::Headers headers;

  if (states == States::DEPARTURE || states == States::RECEIVING) {
    if (flags.api_freely) {
      headers.add("Accept"         , "image/jpeg, image/png"     );
      headers.add("User-Agent"     , "NEURGenerator for ESP32-S3");
      headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8"   );
      headers.add("Cache-Control"  , "no-cache"                  );
      headers.add("Connection"     , "keep-alive"                );

      if (flags.useloges) Serial.println("🔓 Бесплатный режим: минимальные заголовки");
    }
    // ПЛАТНЫЙ РЕЖИМ - полные заголовки
    else {
      if (!flags.repeated && sk_secret[0] != '\0' && strncmp(sk_secret, "Bearer ", 7) == 0) {
        headers.add("Authorization", sk_secret);
      }

      headers.add("Accept"         , "image/jpeg, image/png"     );
      headers.add("User-Agent"     , "NEURGenerator for ESP32-S3");
      headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8"   );
      headers.add("Cache-Control"  , "no-cache"                  );
      headers.add("Connection"     , "keep-alive"                );
    }
  }
  else if (states == States::API_POLLEN || states == States::API_MODELS) {
    if (flags.useloges) Serial.printf("🔑 Используем API ключ: %s...\n", sk_secret);

    if (sk_secret[0] != '\0' && strncmp(sk_secret, "Bearer ", 7) == 0) {
      headers.add("Authorization", sk_secret);
    }

    headers.add("Accept"         , "application/json");
    headers.add("User-Agent"     , "NEURGenerator for ESP32-S3");
    headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8");
    headers.add("Cache-Control"  , "no-cache");
    headers.add("Connection"     , "keep-alive");
  }
  else if (states == States::TRANSLATE) {
    headers.add("Accept"         , "*/*");
    headers.add("User-Agent"     , "NEURGenerator for ESP32-S3");
    headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8");
    headers.add("Cache-Control"  , "no-cache");
    headers.add("Connection"     , "keep-alive");
  }

  bool ok_query = false;

  if (data) {
    ok_query = http->request(path, method, headers, *data);
  } else {
    ok_query = http->request(path, method, headers);
  }

  if (!ok_query) {
    http->~EspInsecureClient();
    WDT_eTimeout(false);

    if (wdt_enlarge && (states == States::TRANSLATE || states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
      esp_task_wdt_reconfigure(_wdt_config);
    }
    return false;
  }

  WDT_eTimeout(false);//Сброс WDT

  ghttp::Client::Response resp = http->getResponse();

  if (!resp) {
    http->~EspInsecureClient();
    WDT_eTimeout(false);

    if (wdt_enlarge && (states == States::TRANSLATE || states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
      esp_task_wdt_reconfigure(_wdt_config);
    }
    return false;
  }

  try_httpcode = resp.code();

  if (wdt_enlarge && (states == States::TRANSLATE || states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
    esp_task_wdt_reconfigure(_wdt_config);
  }

  if (states == States::DEPARTURE || states == States::RECEIVING) {
    if (try_httpcode != 200) {
      http->~EspInsecureClient();

      if (flags.repeated && (try_httpcode == 400 || try_httpcode == 401)) {
        if (flags.useloges) {
          Serial.println("❌ ОШИБКА 400-401 ПРИ ПОВТОРНОЙ ЗАГРУЗКЕ: изображение недоступно");
          Serial.println("🛑 Прекращаем все операции с этим изображением");
        }

        WDT_eTimeout(false);
        return false;
      }

      switch (try_httpcode) {
        case 400:
          break;

        case 401:
          break;

        case 402:
          break;

        case 403:
          break;

        case 404:
          break;

        case 429:
          break;

        case 500:
        case 502:
        case 503:
        case 504:
          break;

        default:
          if (try_httpcode >= 400) {
            if (flags.useloges) Serial.printf("❌ HTTP-ошибка: %d\n", try_httpcode);
          }
          break;
      }

      WDT_eTimeout(false);
      return false;
    }
  }
  else if (states == States::TRANSLATE) {
    if (try_httpcode != 200) {
      http->~EspInsecureClient();

      if (flags.useloges) Serial.printf("HTTP-ошибка перевода - Код: %d\n", try_httpcode);

      // Определяем тип ошибки перевода
      if (try_httpcode == 429) {
        if (flags.useloges) Serial.println("❌ Достигнут лимит переводов MyMemory");
        setStateStatus(Status::ERROR_TRANSLATE_LIMIT);
      } else {
        setStateStatus(Status::ERROR_TRANSLATE); // Другие ошибки перевода
      }

      WDT_eTimeout(false);
      return false;
    }
  } else if (states == States::API_POLLEN) {
    // Проверяем HTTP код
    if (try_httpcode != 200) {
      http->~EspInsecureClient();

      if (flags.useloges) Serial.printf("Ошибка запроса баланса - Код: %d\n", try_httpcode);

      // Проверяем ошибки авторизации
      if (try_httpcode == 401 || try_httpcode == 403) {
        memset(api_pollen, 0, sz_api_pollen);
        snprintf(api_pollen, sz_api_pollen - 1, API_NO_ACCESS);

        if (flags.useloges) Serial.println("❌ API ключ не имеет прав на запрос баланса");
      }

      WDT_eTimeout(false);
      return false;
    }
  } else if (states == States::API_MODELS) {
    // Проверяем HTTP код для запроса моделей
    if (try_httpcode != 200) {
      http->~EspInsecureClient();

      if (flags.useloges) Serial.printf("Ошибка запроса моделей - Код: %d\n", try_httpcode);

      // Проверяем ошибки авторизации
      if (try_httpcode == 401 || try_httpcode == 403) {
        memset(json_models.names, 0, sz_model_names);
        snprintf(json_models.names, sz_model_names - 1, API_MODELS_NAMES);

        memset(json_models.title, 0, sz_model_title);
        snprintf(json_models.title, sz_model_title - 1, API_MODELS_TITLE);

        memset(json_models.price, 0, sz_model_price);
        snprintf(json_models.price, sz_model_price - 1, API_MODELS_PRICE);

        if (flags.useloges) Serial.println("❌ API ключ не имеет прав на запрос моделей");
      }

      WDT_eTimeout(false);
      return false;
    }
  }

  WDT_eTimeout(false);

  bool success = false;
  switch (states) {
    case States::TRANSLATE:
      {
        memset(JsonBuffer, 0, sz_JsonBuffer);
        size_t bytesRead = resp.body().readBytes(JsonBuffer, sz_JsonBuffer - 1);
        JsonBuffer[bytesRead] = '\0';

        gson::Parser json;
        if (json.parse(JsonBuffer)) {
          success = ParserTranslate(json);
        } else {
          if (flags.useloges) Serial.printf("Ошибка парсинга JSON\n");
          success = false;
        }
      }
      break;

    case States::API_POLLEN:
      {
        memset(JsonBuffer, 0, sz_JsonBuffer);
        size_t bytesRead = resp.body().readBytes(JsonBuffer, sz_JsonBuffer - 1);
        JsonBuffer[bytesRead] = '\0';

        gson::Parser json;
        if (json.parse(JsonBuffer)) {
          success = ParserPollen(json);
        } else {
          if (flags.useloges) Serial.printf("Ошибка парсинга JSON\n");
          success = false;
        }
      }
      break;

    case States::API_MODELS:
      {
        memset(JsonBuffer, 0, sz_JsonBuffer);
        size_t bytesRead = resp.body().readBytes(JsonBuffer, sz_JsonBuffer - 1);
        JsonBuffer[bytesRead] = '\0';

        gson::Parser json;
        if (json.parse(JsonBuffer)) {
          success = ParserModels(json);
        } else {
          if (flags.useloges) Serial.printf("Ошибка парсинга JSON моделей\n");
          success = false;
        }
      }
      break;

    case States::DEPARTURE:
      success = true;
      break;

    case States::RECEIVING:
      success = ReaderJPG(resp.body());
      break;
  }

  http->~EspInsecureClient();
  return success;
}

void NEURGenerator::tick(bool WiFiState) {
  if (http_cleanup && http) {
    cleanupHttp();

    http_cleanup = false;
    http_stopped = false;
  }

  if (checkWaitState()) {
    if (url_images && url_images[0] != '\0') {
      if (SafeMillis(last_apicommands, millis()) >= WAIT_PERIOD) {
        setStateStatus(Status::OK_WAITING_COMMAND);
      }
    }
  }

  if (checkInfoState()) {
    if (SafeMillis(last_apicommands, millis()) >= INFO_PERIOD) {
      setStateStatus(Status::OK_WAITING_COMMAND);
    }
  }

  if (checkFailState()) {
    if (SafeMillis(last_apicommands, millis()) >= FAIL_PERIOD) {
      setStateStatus(Status::OK_WAITING_COMMAND);
    }
  }

  if (http_stopped) {
    return;
  }

  if (!WiFiState) return;

  if (state_gen == Status::OK_DOWNLOADING) {
    if (url_images && url_images[0] != '\0') {
      if (SafeMillis(last_apicommands, millis()) >= STAY_PERIOD) {
        if (!flags.repeated) {
          setStateStatus(Status::OK_WAITING_FOR_RESULT);
        } else {
          setStateStatus(Status::OK_RECEIVING_REQUEST);
          attempt_network_count = 0;
          if (flags.useloges) Serial.println("🚀 Начинаем попытки получения");

          if (flags.useloges && wdt_enlarge) {
            Serial.printf("   WDT таймаут %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
          }
        }
      }
    } else {
      // ⚠️ URL пустой - ошибка!
      if (flags.useloges) Serial.println("❌ Ошибка: URL для повторной загрузки пустой");
      setStateStatus(Status::ERROR_AIGENERATION);
    }
  }

  if (state_gen == Status::OK_PREPARING_DATA) {
    if (SafeMillis(last_apicommands, millis()) >= STAY_PERIOD) {
      if (url_images && url_images[0] != '\0') {
        setStateStatus(Status::OK_SENDING_REQUEST);
        attempt_network_count = 0;
        if (flags.useloges) Serial.println("🚀 Начинаем попытки отправки");

        if (flags.useloges && wdt_enlarge) {
          Serial.printf("   WDT таймаут %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
        }
      } else {
        // ⚠️ URL пустой - ошибка!
        if (flags.useloges) Serial.println("❌ Ошибка: URL изображения не сформирован");
        setStateStatus(Status::ERROR_AIGENERATION);
      }
    }
  }

  if (state_gen == Status::OK_WAITING_FOR_RESULT) {
    if (SafeMillis(last_apicommands, millis()) >= TIME_PERIOD) {
      setStateStatus(Status::OK_RECEIVING_REQUEST);
      attempt_network_count = 0;
      if (flags.useloges) Serial.println("🚀 Начинаем попытки получения");

      if (flags.useloges && wdt_enlarge) {
        Serial.printf("   WDT таймаут %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
      }
    }
  }

  if ((state_gen == Status::OK_SENDING_REQUEST || state_gen == Status::OK_SENDING_ATTEMPT) && url_images && url_images[0] != '\0') {
    if (state_gen == Status::OK_SENDING_REQUEST) {
      if (SafeMillis(last_apicommands, millis()) < STAY_PERIOD) {
        return;
      }
    }

    setStateStatus(Status::OK_SENDING_ATTEMPT);

    if (attempt_network_count < try_request) {
      if (flags.useloges) Serial.printf("   - отправка запроса [%d/%d]\n", attempt_network_count + 1, try_request);

      // Делаем паузу между попытками (кроме первой)
      if (attempt_network_count > 0) {
        vTaskDelay(pdMS_TO_TICKS(try_timeout));
      }

      WDT_eTimeout(true); // Сброс WDT
      bool ok_query = false;

      if (flags.api_freely) {
        ok_query = request_query(States::DEPARTURE, POLLIN_FREE, POLLIN_PORT, url_images, "GET");
      } else {
        ok_query = request_query(States::DEPARTURE, POLLIN_HOST, POLLIN_PORT, url_images, "GET");
      }
      WDT_eTimeout(true); // Сброс WDT

      if (ok_query) {
        // Успешная отправка
        if (flags.useloges) Serial.printf("✅ Запрос успешно отправлен [%d/%d]\n", attempt_network_count + 1, try_request);
        setStateStatus(Status::OK_WAITING_FOR_RESULT);
        attempt_network_count = 0;

        if (flags.useloges && wdt_enlarge) {
          Serial.printf("   WDT таймаут %d мс\n", _wdt_config->timeout_ms);
        }
      } else {
        // Ошибка отправки
        attempt_network_count++;

        flags.critical = false;

        switch (try_httpcode) {
          case 400:
            if (flags.repeated) {
              flags.critical = true;
            } else {
              flags.critical = (attempt_network_count >= (try_request - 1));
            }
            break;

          case 401:
            flags.critical = true;
            break;

          case 402:
            flags.critical = true;
            break;

          case 403:
            flags.critical = true;
            break;

          case 404:
            if (flags.repeated) {
              flags.critical = true;
            } else {
              flags.critical = (attempt_network_count >= (try_request - 1));
            }
            break;

          case 429:
            flags.critical = true;
            break;

          case 500:
          case 502:
          case 503:
          case 504:
            flags.critical = (attempt_network_count >= (try_request - 1));
            break;

          default:
            if (try_httpcode >= 400) {
              flags.critical = (attempt_network_count >= try_request);
            }
            break;
        }

        // Немедленная остановка при критических ошибках
        if (flags.critical) {
          if (flags.useloges) Serial.printf("❌ Критическая ошибка сервера (код %d). Прекращаем попытки.\n", try_httpcode);

          switch (try_httpcode) {
            case 400:
              if (flags.useloges) Serial.println("❌ ОШИБКА ЗАПРОСА (400): Неверный запрос");
              setStateStatus(Status::ERROR_REQUESTS);
              break;

            case 401:
              if (flags.useloges) Serial.println("❌ ОШИБКА АВТОРИЗАЦИИ (401): Неверный или просроченный ключ");
              setStateStatus(Status::ERROR_AUTHENTICATE);
              break;

            case 402:
              if (flags.useloges) Serial.println("❌ ОШИБКА БАЛАНСА (402): Закончилась пыльца");
              setStateStatus(Status::ERROR_BALANCEBUDGET);
              break;

            case 403:
              if (flags.useloges) Serial.println("❌ ДОСТУП ЗАПРЕЩЕН (403): Нет прав для запроса");
              setStateStatus(Status::ERROR_ACCESSDENIED);
              break;

            case 404:
              if (flags.useloges) Serial.println("❌ РЕСУРС НЕ НАЙДЕН (404): Неверный endpoint API");
              setStateStatus(Status::ERROR_REQUESTS);
              break;

            case 429:
              if (flags.useloges) Serial.printf("ПЕРЕГРУЗКА СЕРВЕРА: Слишком много запросов - пауза 120 секунд\n");
              setStateStatus(Status::ERROR_OVERLOAD);
              break;

            case 500:
            case 502:
            case 503:
            case 504:
              if (flags.useloges) Serial.printf("СЕРВЕР НЕДОСТУПЕН: Нет активных серверов генерации - пауза 600 секунд\n");
              setStateStatus(Status::ERROR_UNAVAILABLE);
              break;

            default:
              if (try_httpcode >= 400) {
                setStateStatus(Status::ERROR_RESPONSE);
              }
              break;
          }

          if (flags.useloges && wdt_enlarge) {
            Serial.printf("   WDT таймаут %d мс\n", _wdt_config->timeout_ms);
          }

          // Сбрасываем счетчики
          attempt_network_count = 0;
          attempt_decoder_count = 0;
          attempt_decoder = false;
          return; // Выходим из обработки
        }

        // Для некритических ошибок продолжаем попытки
        if (attempt_network_count >= try_request) {
          // Все попытки исчерпаны
          if (flags.useloges) Serial.printf("❌ Ошибка отправки после %d попыток\n", try_request);

          if (flags.useloges && wdt_enlarge) {
            Serial.printf("   WDT таймаут %d мс\n", _wdt_config->timeout_ms);
          }

          setStateStatus(Status::ERROR_REQUESTS);

          // Сбрасываем счетчики
          attempt_network_count = 0;
          attempt_decoder_count = 0;
          attempt_decoder = false;
        }
      }
    }
  }

  else if ((state_gen == Status::OK_RECEIVING_REQUEST || state_gen == Status::OK_RECEIVING_ATTEMPT) && url_images && url_images[0] != '\0' && neur_timer.period(TIME_PERIOD)) {
    if (state_gen == Status::OK_RECEIVING_REQUEST) {
      if (SafeMillis(last_apicommands, millis()) < STAY_PERIOD) {
        return;
      }
    }

    setStateStatus(Status::OK_RECEIVING_ATTEMPT);

    if (resp_receive()) {
      attempt_network_count = 0;
      attempt_decoder_count = 0; // Сброс при успехе
      attempt_decoder = false;
    } else {
      attempt_network_count++;

      if (flags.repeated && (try_httpcode == 400 || try_httpcode == 401)) {
        attempt_network_count = 0;
        attempt_decoder_count = 0;
        attempt_decoder = false;

        setStateStatus(Status::ERROR_LOADEDOLDIMAGES);
        return;
      }

      if (attempt_network_count >= try_receive) {
        setStateStatus(Status::ERROR_RESPONSE);
        _end_generations = SafeMillis(_str_generations, millis());
        _end_generations = (_end_generations / TIME_PERIOD) * TIME_PERIOD;

        attempt_network_count = 0;
        attempt_decoder_count = 0;
        attempt_decoder = false;
      }

      if (attempt_decoder && attempt_decoder_count >= try_receive) {
        setStateStatus(Status::ERROR_DECODINGS);
        _end_generations = SafeMillis(_str_generations, millis());
        _end_generations = (_end_generations / TIME_PERIOD) * TIME_PERIOD;

        attempt_network_count = 0;
        attempt_decoder_count = 0;
        attempt_decoder = false;
      }
    }
  }
}

bool NEURGenerator::isRussianText(const char* text) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }

  for (int i = 0; text[i] != '\0'; i++) {
    // Русские буквы в кодировке Windows-1251/CP1251
    if ((text[i] >= 0xC0 && text[i] <= 0xFF) ||  // А-я
        text[i] == 0xA8 || text[i] == 0xB8) {    // Ёё
      return true;
    }
  }
  return false;
}

// Вспомогательная функция для проверки валидности текста
bool NEURGenerator::isEnglishText(const char* text) {
  if (!text || strlen(text) == 0) {
    return false;
  }

  // Проверяем минимальную/максимальную длину
  size_t len = strlen(text);
  if (len < 2 || len > 1000) {
    return false;
  }

  // Проверяем, что текст содержит в основном буквы, цифры, пробелы и основные знаки препинания
  int letterCount = 0;
  int validCharCount = 0;

  for (size_t i = 0; i < len; i++) {
    char c = text[i];

    // Буквы (латиница и кириллица)
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= 'А' && c <= 'я') || c == 'Ё' || c == 'ё') {
      letterCount++;
      validCharCount++;
    }
    // Цифры
    else if (c >= '0' && c <= '9') {
      validCharCount++;
    }
    // Пробелы и основные знаки препинания
    else if (c == ' ' || c == ',' || c == '.' || c == '!' || c == '?' ||
             c == '-' || c == '_' || c == '(' || c == ')' || c == ':' ||
             c == ';' || c == '\'' || c == '"') {
      validCharCount++;
    }
  }

  // Текст должен содержать не менее 30% букв и 80% валидных символов
  float letterRatio = (float)letterCount / len;
  float validRatio = (float)validCharCount / len;

  return (letterRatio >= 0.3f && validRatio >= 0.8f);
}

bool NEURGenerator::PromptTranslate(const char* russian_text) {
  if (!russian_text || russian_text[0] == '\0') {
    return false;
  }

  // Проверяем, что HTTP не занят генерацией
  if (isGenerating()) {
    if (flags.useloges) Serial.printf("❌ Невозможно выполнить перевод: идет генерация изображения\n");
    return false;
  }

  // Проверяем, что текст действительно содержит русские символы
  if (!isRussianText(russian_text)) {
    if (flags.useloges) Serial.printf("❌ Текст не содержит русских символов: %s\n", russian_text);
    return false;
  }

  // ИНИЦИАЛИЗАЦИЯ БУФЕРОВ
  if (!JsonBuffer) {
    JsonBuffer = (char*)ps_malloc(sz_JsonBuffer);
    if (!JsonBuffer) {
      return false;
    }
  }

  memset(rus_prompt, 0, sz_rus_prompt);
  memset(eng_prompt, 0, sz_eng_prompt);

  strcpy(rus_prompt, russian_text);

  // Кодируем текст для URL
  memset(enc_prompt, 0, sz_enc_prompt);
  url_encode(russian_text, enc_prompt);

  // Формируем URL запроса с параметром email если установлен
  memset(url_transl, 0, sz_url_transl);

  if (mymemmory && mymemmory[0] != '\0') {
    snprintf(url_transl, sz_url_transl, "/get?q=%s&langpair=ru|en&de=%s",
             enc_prompt, mymemmory);

    if (flags.useloges) Serial.printf("✅ Используем зарегистрированный email: %s\n", mymemmory);
  } else {
    snprintf(url_transl, sz_url_transl, "/get?q=%s&langpair=ru|en", enc_prompt);
  }

  if (flags.useloges) Serial.printf("🔄 Перевод текста: %s\n", russian_text);

  // Выполняем запрос
  bool ok_query = request_query(States::TRANSLATE, TRANS_HOST, TRANS_PORT, url_transl, "GET");

  if (!ok_query) {
    if (flags.useloges) Serial.printf("❌ Ошибка запроса перевода\n");
    memset(rus_prompt, 0, sz_rus_prompt);
  } else {
    if (flags.useloges) Serial.printf("✅ Запрос перевода выполнен успешно\n");
  }

  return ok_query;
}

bool NEURGenerator::ParserTranslate(gson::Parser& json) {
  if (!JsonBuffer) {
    JsonBuffer = (char*)ps_malloc(sz_JsonBuffer);
    if (!JsonBuffer) {
      setStateStatus(Status::ERROR_INITMEMORY);
      return false;
    }
  }

  // Очищаем буфер перед каждым использованием
  memset(translate, 0, sz_translate);

  // 1. Пытаемся получить перевод через JSON парсер
  if (json["responseData"]["translatedText"].c_str()) {
    strncpy(translate, json["responseData"]["translatedText"].c_str(), sz_translate - 1);
    translate[sz_translate - 1] = '\0';

    if (flags.useloges) Serial.printf("✅ Перевод найден (способ 1): %s\n", translate);
  }

  // 2. Если не получилось через парсер, парсим вручную
  if (strlen(translate) == 0 && JsonBuffer && strlen(JsonBuffer) > 0) {
    const char* patterns[] = {
      "\"translatedText\":\"",
      "'translatedText':'",
      "translatedText\":\"",
      "translatedText':'"
    };

    const char* str_marker;
    const char* end_marker;
    for (const char* pattern : patterns) {
      str_marker = strstr(JsonBuffer, pattern);
      if (str_marker) {
        str_marker += strlen(pattern);
        end_marker = strchr(str_marker, '"');
        if (!end_marker) end_marker = strchr(str_marker, '\'');
        if (!end_marker) end_marker = strchr(str_marker, '}');
        if (!end_marker) end_marker = strchr(str_marker, ',');
        if (!end_marker) end_marker = str_marker + 100;

        if (end_marker && end_marker > str_marker) {
          size_t len = min((size_t)(end_marker - str_marker), sz_translate - 1);
          strncpy(translate, str_marker, len);
          translate[len] = '\0';

          if (isEnglishText(translate)) {
            if (flags.useloges) Serial.printf("✅ Перевод найден (способ 2): %s\n", translate);
            break;
          } else {
            memset(translate, 0, sz_translate);
          }
        }
      }
    }
  }

  // 3. Проверяем результат
  bool success = false;
  if (translate && strlen(translate) > 0 && isEnglishText(translate)) {
    strcpy(eng_prompt, translate);

    if (flags.useloges) Serial.printf("✅ Сохраняем перевод\n");
    if (_eng_cb) {
      _eng_cb();
    }
    success = true;
  } else {
    if (flags.useloges) Serial.printf("❌ Перевод не найден или невалидный\n");
    success = false;
  }

  return success;
}

void addValueToBuffer(char* buffer, const char* str) {
  if (buffer[0] != '\0') {
    strcat(buffer, ";");
  }
  strcat(buffer, str);
}

bool NEURGenerator::getApiPollen(const char* _sk_secret) {
  memset(sk_secret, 0, sz_sk_secret);

  if (_sk_secret && _sk_secret[0] != '\0') {
    snprintf(sk_secret, sz_sk_secret, "Bearer %s", _sk_secret);
  }

  if (!sk_secret || sk_secret[0] == '\0') {
    if (flags.useloges) Serial.println("❌ Нет API ключа для запроса баланса");
    return false;
  }

  if (isGenerating()) {
    if (flags.useloges) Serial.println("❌ Невозможно запросить баланс: идет генерация");
    return false;
  }

  // ИНИЦИАЛИЗАЦИЯ БУФЕРОВ
  if (!JsonBuffer) {
    JsonBuffer = (char*)ps_malloc(sz_JsonBuffer);
    if (!JsonBuffer) {
      return false;
    }
  }

  setStateStatus(Status::GET_API_POLLEN);

  WDT_sTimeout();
  bool ok_query = request_query(States::API_POLLEN, POLLIN_HOST, POLLIN_PORT, "/account/balance", "GET");
  WDT_eTimeout(true);

  if (!ok_query) {
    if (flags.useloges) Serial.println("❌ Ошибка запроса баланса");

    setStateStatus(Status::GET_API_POLLEN_ERR);
    return false;
  }

  if (flags.useloges) Serial.println("✅ Запрос баланса выполнен успешно");

  setStateStatus(Status::GET_API_POLLEN_OK);
  return true;
}

bool NEURGenerator::ParserPollen(gson::Parser& json) {
  char* _pollen = (char*)heap_caps_malloc(sz_api_pollen, MALLOC_CAP_SPIRAM);
  if (!_pollen) return false; // Проверка выделения памяти

  // Получаем баланс как строку
  if (json["balance"].c_str()) {
    strncpy(_pollen, json["balance"].c_str(), sz_api_pollen - 1);
    _pollen[sz_api_pollen - 1] = '\0';
  } else {
    _pollen[0] = '\0';
  }

  if (_pollen[0] != '\0') {
    // Преобразуем в double для точного форматирования
    double balance = atof(_pollen);

    // Форматируем с 4 знаками после запятой
    snprintf(api_pollen, sz_api_pollen, "%.4f", balance);

    if (flags.useloges) Serial.printf("✅ Баланс получен: %s pollen\n", api_pollen);

    heap_caps_free(_pollen);
    return true;
  } else {
    // НЕ ЗАПИСЫВАЕМ API_ERROR_JSON, ЕСЛИ БАЛАНС УЖЕ БЫЛ ПОЛУЧЕН
    if (strcmp(api_pollen, API_POLLEN_NO_DATA) == 0 ||
        strcmp(api_pollen, API_NO_ACCESS) == 0 ||
        strcmp(api_pollen, API_ERROR_JSON) == 0) {

      // Только для начальных состояний
      snprintf(api_pollen, sz_api_pollen, "%s", API_ERROR_JSON);

      if (flags.useloges) Serial.println("❌ Ошибка парсинга баланса");
    } else {
      // Баланс уже был получен ранее, сохраняем старое значение
      if (flags.useloges) Serial.printf("⚠️ Ошибка парсинга, но баланс уже был: %s pollen\n", api_pollen);
    }

    heap_caps_free(_pollen);
    return false;
  }
}

bool NEURGenerator::getApiModels(const char* _sk_secret) {
  memset(sk_secret, 0, sz_sk_secret);

  if (_sk_secret && _sk_secret[0] != '\0') {
    snprintf(sk_secret, sz_sk_secret, "Bearer %s", _sk_secret);
  }

  if (!sk_secret || sk_secret[0] == '\0') {
    if (flags.useloges) Serial.println("❌ Нет API ключа для запроса моделей");
    return false;
  }

  // ИНИЦИАЛИЗАЦИЯ БУФЕРОВ
  if (!JsonBuffer) {
    JsonBuffer = (char*)ps_malloc(sz_JsonBuffer);
    if (!JsonBuffer) {
      return false;
    }
  }

  if (isGenerating()) {
    if (flags.useloges) Serial.println("❌ Невозможно запросить модели: идет генерация");
    return false;
  }

  setStateStatus(Status::GET_API_MODELS);

  WDT_sTimeout(); // Запуск WDT
  bool ok_query = request_query(States::API_MODELS, POLLIN_HOST, POLLIN_PORT, "/image/models", "GET");
  WDT_eTimeout(true); // Сброс WDT

  if (!ok_query) {
    if (flags.useloges) Serial.println("❌ Ошибка запроса моделей");

    setStateStatus(Status::GET_API_MODELS_ERR);
    return false;
  }

  if (flags.useloges) Serial.println("✅ Запрос моделей выполнен успешно");

  setStateStatus(Status::GET_API_MODELS_OK);
  return true;
}

bool NEURGenerator::ParserModels(gson::Parser& json) {
  // ПРОВЕРЯЕМ, ЧТО ЭТО МАССИВ И НЕ ПУСТОЙ
  if (json.rootLength() == 0) {
    if (flags.useloges) Serial.println("❌ Ответ моделей пустой или не массив");

    // ✅ Обнуляем и ставим значения по умолчанию
    memset(json_models.names, 0, sz_model_names);
    snprintf(json_models.names, sz_model_names - 1, API_MODELS_NAMES);

    memset(json_models.title, 0, sz_model_title);
    snprintf(json_models.title, sz_model_title - 1, API_MODELS_TITLE);

    memset(json_models.price, 0, sz_model_price);
    snprintf(json_models.price, sz_model_price - 1, API_MODELS_PRICE);

    return false;
  }

  // ✅ Успешный парсинг - обнуляем и записываем новые данные
  memset(json_models.names, 0, sz_model_names);
  memset(json_models.title, 0, sz_model_title);
  memset(json_models.price, 0, sz_model_price);

  char* _names = (char*)heap_caps_malloc(sz_model_names, MALLOC_CAP_SPIRAM);
  char* _title = (char*)heap_caps_malloc(sz_model_title, MALLOC_CAP_SPIRAM);
  char* _price = (char*)heap_caps_malloc(sz_model_price, MALLOC_CAP_SPIRAM);
  char* _short = (char*)heap_caps_malloc(sz_model_title, MALLOC_CAP_SPIRAM);

  if (!_names || !_title || !_price || !_short) {
    if (flags.useloges) Serial.println("❌ Ошибка выделения памяти для парсинга моделей");
    if (_names) heap_caps_free(_names);
    if (_title) heap_caps_free(_title);
    if (_price) heap_caps_free(_price);
    if (_short) heap_caps_free(_short);
    return false;
  }

  // Ограничиваем количество моделей
  uint8_t models_to_process = min((int)json.rootLength(), API_MODELS_COUNT);

  for (uint8_t i = 0; i < models_to_process; ++i) {
    memset(_names, 0, sz_model_names);
    memset(_title, 0, sz_model_title);
    memset(_price, 0, sz_model_price);
    memset(_short, 0, sz_model_title);

    WDT_eTimeout(false);

    if (json[i]["name"].c_str()) {
      strncpy(_names, json[i]["name"].c_str(), sz_model_names - 1);
      _names[sz_model_names - 1] = '\0';
    }

    if (json[i]["title"].c_str()) {
      strncpy(_title, json[i]["title"].c_str(), sz_model_title - 1);
      _title[sz_model_title - 1] = '\0';
    }

    if (json[i]["pricing"]["completionImageTokens"].c_str()) {
      double price = atof(json[i]["pricing"]["completionImageTokens"].c_str());
      snprintf(_price, sz_model_price, "%.4f", price);
    } else {
      snprintf(_price, sz_model_price, "%.4f", 0.0);
    }

    if (_title[0] != '\0') {
      // Ищем разделитель " - " (дефис с пробелами)
      const char* separator = strstr(_title, " - ");

      if (separator) {
        // Копируем часть до разделителя " - "
        size_t len = separator - _title;
        strncpy(_short, _title, min(len, sz_model_title - 1));
      } else {
        // Если нет " - ", ищем ": "
        separator = strstr(_title, ": ");
        if (separator) {
          size_t len = separator - _title;
          strncpy(_short, _title, min(len, sz_model_title - 1));
        } else {
          // Если нет разделителей, берем первые 30 символов
          strncpy(_short, _title, min((size_t)30, strlen(_title)));
        }
      }

      // Убираем пробелы в конце
      size_t len = strlen(_short);
      while (len > 0 && _short[len - 1] == ' ') {
        _short[--len] = '\0';
      }

      strncpy(_title, _short, sz_model_title - 1);
      _title[sz_model_title - 1] = '\0';
    }

    if (_names[0] != '\0') {
      addValueToBuffer(json_models.names, _names);
    }

    if (_title[0] != '\0') {
      addValueToBuffer(json_models.title, _title);
    }

    if (_price[0] != '\0') {
      addValueToBuffer(json_models.price, _price);
    }
  }

  // Проверяем, что получили хоть какие-то данные
  if (json_models.names[0] == '\0') {
    strncpy(json_models.names, API_MODELS_NAMES, sz_model_names - 1);
    json_models.names[sz_model_names - 1] = '\0';
  }

  if (json_models.title[0] == '\0') {
    strncpy(json_models.title, API_MODELS_TITLE, sz_model_title - 1);
    json_models.title[sz_model_title - 1] = '\0';
  }

  if (json_models.price[0] == '\0') {
    strncpy(json_models.price, API_MODELS_PRICE, sz_model_price - 1);
    json_models.price[sz_model_price - 1] = '\0';
  }

  heap_caps_free(_names);
  heap_caps_free(_title);
  heap_caps_free(_price);
  heap_caps_free(_short);

  return true;
}

bool NEURGenerator::data_prepare(const char* prompt
                                 , const char* suffix
                                 , const char* modifi
                                 , const char* denial
                                 , bool translate) {

  flags.repeated = false;

  // Сброс предыдущих состояний
  attempt_decoder = false;
  attempt_network_count = 0;
  attempt_decoder_count = 0;

  // 1. Установить статус подготовки
  setStateStatus(Status::OK_PREPARING_DATA);

  // 2. Валидация базовых параметров
  if (!sk_secret || sk_secret[0] == '\0') {
    setStateStatus(Status::ERROR_AIGENERATION);
    return false;
  }

  if (prompt == nullptr || prompt[0] == '\0') {
    setStateStatus(Status::ERROR_AIGENERATION);
    return false;
  }

  uint16_t _requestW = 0;
  uint16_t _requestH = 0;
  uint16_t max_dimensionW = 0;
  uint16_t max_dimensionH = 0;

  if (flags.api_adjust) {
    // Пытаемся получить размеры из загруженного config.json
    if (getModelScale(api_models, (uint8_t)api_scales,
                      _requestW, _requestH,
                      max_dimensionW, max_dimensionH)) {
      // ✅ Успешно получили из конфига
      if (flags.useloges) {
        Serial.printf("📐 Размеры из конфига: %s -> %dx%d (max: %dx%d)\n",
                      api_models, _requestW, _requestH,
                      max_dimensionW, max_dimensionH);
      }
    } else {
      // ❌ Не нашли в конфиге - используем fallback
      if (flags.useloges) {
        Serial.printf("⚠️ Модель '%s' не найдена в конфиге, используем fallback\n", api_models);
      }

      // FALLBACK: FLUX
      if (strcmp(api_models, "flux") == 0) {
        switch (api_scales) {
          case APIScales::SCALE_LOW   : _requestW = 512; _requestH = 384; break;
          case APIScales::SCALE_MEDIUM: _requestW = 768; _requestH = 576; break;
          case APIScales::SCALE_HIGH  : _requestW = 1024; _requestH = 768; break;
          default                     : _requestW = 512; _requestH = 384; break;
        }
        max_dimensionW = 1024;
        max_dimensionH = 768;
      }
      // FALLBACK: SANA
      else if (strcmp(api_models, "sana") == 0) {
        switch (api_scales) {
          case APIScales::SCALE_LOW   : _requestW = 480; _requestH = 320; break;
          case APIScales::SCALE_MEDIUM: _requestW = 528; _requestH = 352; break;
          case APIScales::SCALE_HIGH  : _requestW = 576; _requestH = 384; break;
          default                     : _requestW = 480; _requestH = 320; break;
        }
        max_dimensionW = 576;
        max_dimensionH = 384;
      }
      // FALLBACK: DREAMSHAPER
      else if (strcmp(api_models, "dreamshaper") == 0) {
        switch (api_scales) {
          case APIScales::SCALE_LOW   : _requestW = 480; _requestH = 320; break;
          case APIScales::SCALE_MEDIUM: _requestW = 528; _requestH = 352; break;
          case APIScales::SCALE_HIGH  : _requestW = 576; _requestH = 384; break;
          default                     : _requestW = 480; _requestH = 320; break;
        }
        max_dimensionW = 576;
        max_dimensionH = 384;
      }
      // FALLBACK: ZIMAGE (обычный режим)
      else {
        switch (api_scales) {
          case APIScales::SCALE_LOW   : _requestW = 480; _requestH = 320; break;
          case APIScales::SCALE_MEDIUM: _requestW = 720; _requestH = 480; break;
          case APIScales::SCALE_HIGH  : _requestW = 960; _requestH = 640; break;
          default                     : _requestW = 480; _requestH = 320; break;
        }
        max_dimensionW = 960;
        max_dimensionH = 640;
      }
    }
  }
  // ПРОВЕРКА ПРОГРЕССИВНОГО JPEG
  else if (isProgressive[api_number]._flag) {
    _requestW = 480 * 2;
    _requestH = 320 * 2;
    max_dimensionW = 480 * 2;
    max_dimensionH = 320 * 2;
  }
  // ОБЫЧНЫЙ РЕЖИМ
  else {
    switch (api_scales) {
      case APIScales::SCALE_LOW   : _requestW = 480; _requestH = 320; break;
      case APIScales::SCALE_MEDIUM: _requestW = 720; _requestH = 480; break;
      case APIScales::SCALE_HIGH  : _requestW = 960; _requestH = 640; break;
      default                     : _requestW = 480; _requestH = 320; break;
    }
    max_dimensionW = 960;
    max_dimensionH = 640;
  }

  if (_requestW == 0 || _requestH == 0 || _requestW > max_dimensionW || _requestH > max_dimensionH) {
    setStateStatus(Status::ERROR_AIGENERATION);
    return false;
  }

  // 3. Инициализация буферов (если нужно)
  if (!jpegDataBuf) {
    jpegDataBuf = (char*)ps_malloc(sz_jpegDataBuf);
    if (!jpegDataBuf) {
      setStateStatus(Status::ERROR_INITMEMORY);
      return false;
    }
  }

  if (!JsonBuffer) {
    JsonBuffer = (char*)ps_malloc(sz_JsonBuffer);
    if (!JsonBuffer) {
      setStateStatus(Status::ERROR_INITMEMORY);
      return false;
    }
  }

  flags.translate = translate;

  // 4. Очистка буферов
  memset(jpegDataBuf, 0, sz_jpegDataBuf);

  memset(url_images, 0, sz_url_images);
  memset(tmp_prompt, 0, sz_tmp_prompt);

  memset(enc_prompt, 0, sz_enc_prompt);
  memset(enc_denial, 0, sz_enc_denial);

  memset(rus_prompt, 0, sz_rus_prompt);
  memset(eng_prompt, 0, sz_eng_prompt);

  // 5. Подготовка основного промпта (с переводом если нужно)
  if (flags.api_switch && isRussianText(prompt)) {
    strcpy(rus_prompt, prompt);

    // Пытаемся перевести
    url_encode(prompt, enc_prompt);
    memset(url_transl, 0, sz_url_transl);

    if (mymemmory && mymemmory[0] != '\0') {
      snprintf(url_transl, sz_url_transl, "/get?q=%s&langpair=ru|en&de=%s",
               enc_prompt, mymemmory);
      if (flags.useloges) Serial.printf("✅ Используем зарегистрированный email: %s\n", mymemmory);
    } else {
      snprintf(url_transl, sz_url_transl, "/get?q=%s&langpair=ru|en", enc_prompt);
    }

    bool translate_ok = request_query(States::TRANSLATE, TRANS_HOST, TRANS_PORT, url_transl, "GET");

    if (translate_ok && eng_prompt[0] != '\0') {
      strcpy(tmp_prompt, eng_prompt);
      if (flags.useloges) Serial.println("✅ Использован переведенный промпт");
    } else {
      strcpy(tmp_prompt, prompt);
      if (flags.useloges) Serial.println("⚠️  Использован оригинальный промпт (перевод не удался)");
    }
  } else {
    // Перевод не нужен
    strcpy(tmp_prompt, prompt);
    if (flags.useloges) Serial.println("✅ Использован оригинальный промпт (перевод не нужен)");
  }

  // 6. Добавляем суффикс и модификаторы
  if (suffix && suffix[0] != '\0') {
    if (strlen(tmp_prompt) > 0) strcat(tmp_prompt, ", ");
    strcat(tmp_prompt, suffix);
  }

  if (modifi && modifi[0] != '\0') {
    if (strlen(tmp_prompt) > 0) strcat(tmp_prompt, ", ");
    strcat(tmp_prompt, modifi);
  }

  if (flags.useloges) {
    Serial.print("📝 Финальный промпт: ");
    Serial.println(tmp_prompt);
  }

  // 7. URL-кодирование промпта
  url_encode(tmp_prompt, enc_prompt);

  // 8. Генерация случайного seed
  uint32_t seed = esp_random() % 100000000;

  // 9. Получение строковых значений настроек
  const char* api_models_str = getAPIModelsString();
  const char* api_levels_str = getAPILevelsString();
  const char* api_scales_str = getAPIScalesString();
  const char* api_enhanc_str = api_enhanc ? "true" : "false";
  const char* api_filter_str = api_filter ? "true" : "false";

  // 10. Формирование URL
  if (flags.api_freely) {
    uint16_t _requestFreeW = 480;
    uint16_t _requestFreeH = 320;

    switch (api_scales) {
      case APIScales::SCALE_LOW   : _requestFreeW = 480; _requestFreeH = 320; break;
      case APIScales::SCALE_MEDIUM: _requestFreeW = 672; _requestFreeH = 448; break;
      case APIScales::SCALE_HIGH  : _requestFreeW = 864; _requestFreeH = 576; break;
      default                     : _requestFreeW = 480; _requestFreeH = 320; break;
    }

    if (denial && denial[0] != '\0') {
      url_encode(denial, enc_denial);
      snprintf(url_images, sz_url_images,
               "https://%s/prompt/%s?negative_prompt=%s&enhance=%s&nologo=true&quality=%s&seed=%d&width=%d&height=%d&safe=%s",
               POLLIN_FREE, enc_prompt, enc_denial, api_enhanc_str, api_levels_str, seed, _requestFreeW, _requestFreeH, api_filter_str);
    } else {
      snprintf(url_images, sz_url_images,
               "https://%s/prompt/%s?enhance=%s&nologo=true&quality=%s&seed=%d&width=%d&height=%d&safe=%s",
               POLLIN_FREE, enc_prompt, api_enhanc_str, api_levels_str, seed, _requestFreeW, _requestFreeH, api_filter_str);
    }

    if (flags.useloges) {
      Serial.println("🔓 БЕСПЛАТНЫЙ РЕЖИМ (image.pollinations.ai)");
      Serial.printf("📐 Размеры: %dx%d\n", _requestFreeW, _requestFreeH);
    }
  }
  else {
    if (denial && denial[0] != '\0') {
      url_encode(denial, enc_denial);

      if (pk_secret && pk_secret[0] != '\0') {
        snprintf(url_images, sz_url_images,
                 "https://%s/image/%s?negative_prompt=%s&model=%s&enhance=%s&nologo=true&quality=%s&seed=%d&%s&safe=%s&%s",
                 POLLIN_HOST, enc_prompt, enc_denial, api_models_str, api_enhanc_str, api_levels_str, seed, api_scales_str, api_filter_str, pk_secret);
      } else {
        snprintf(url_images, sz_url_images,
                 "https://%s/image/%s?negative_prompt=%s&model=%s&enhance=%s&nologo=true&quality=%s&seed=%d&%s&safe=%s",
                 POLLIN_HOST, enc_prompt, enc_denial, api_models_str, api_enhanc_str, api_levels_str, seed, api_scales_str, api_filter_str);
      }
    } else {
      if (pk_secret && pk_secret[0] != '\0') {
        snprintf(url_images, sz_url_images,
                 "https://%s/image/%s?model=%s&enhance=%s&nologo=true&quality=%s&seed=%d&%s&safe=%s&%s",
                 POLLIN_HOST, enc_prompt, api_models_str, api_enhanc_str, api_levels_str, seed, api_scales_str, api_filter_str, pk_secret);
      } else {
        snprintf(url_images, sz_url_images,
                 "https://%s/image/%s?model=%s&enhance=%s&nologo=true&quality=%s&seed=%d&%s&safe=%s",
                 POLLIN_HOST, enc_prompt, api_models_str, api_enhanc_str, api_levels_str, seed, api_scales_str, api_filter_str);
      }
    }
  }

  // 11. Логирование сформированного URL
  if (flags.useloges) {
    Serial.print("🔗 Сформирован URL: ");
    Serial.println(url_images);
  }

  // 12. Установить статус "подготовка к отправке"
  setStateStatus(Status::OK_SENDING_REQUEST);

  return true;
}

bool NEURGenerator::send_request() {
  // Проверяем, что URL сформирован
  if (url_images == nullptr || url_images[0] == '\0') {
    setStateStatus(Status::ERROR_AIGENERATION);
    return false;
  }

  // Проверяем пинг сервера
  const char* ping_host = flags.api_freely ? POLLIN_FREE : POLLIN_HOST;
  bool ok_pings = getPingServer(ping_host);
  if (!ok_pings) {
    if (flags.useloges) Serial.printf("❌ Ошибка связи с сервером %s\n", ping_host);
    setStateStatus(Status::ERROR_CONNECTION);
    return false;
  } else {
    if (flags.useloges) Serial.printf("✅ Связь с сервером %s установлена\n", ping_host);
  }

  // Начинаем процесс отправки
  setStateStatus(Status::OK_SENDING_ATTEMPT);
  attempt_network_count = 0; // Сброс счетчика попыток

  if (flags.useloges) {
    Serial.printf("📤 Начинаем отправку запроса Pollination\n");
  }

  return true;
}

bool NEURGenerator::resp_receive() {
  if (url_images == nullptr || url_images[0] == '\0') {
    return false;
  }

  bool ok_query = false;
  if (flags.useloges) {
    Serial.printf("📥 Получение ответа Pollination\n");
  }

  if (flags.useloges && wdt_enlarge) {
    Serial.printf("   WDT таймаут %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
  }

  if (flags.useloges) Serial.printf("   - получение ответа [%d/%d]\n", attempt_network_count + 1, try_receive);
  WDT_sTimeout();//Запуск WDT

  const char* target_host = POLLIN_HOST;

  // Ищем хост в URL
  const char* start = strstr(url_images, "://");
  if (start) {
    start += 3; // Пропускаем "://"
    const char* end = strchr(start, '/');
    if (end) {
      size_t host_len = end - start;

      // Сравниваем с POLLIN_FREE
      if (strncmp(start, POLLIN_FREE, host_len) == 0 && host_len == strlen(POLLIN_FREE)) {
        target_host = POLLIN_FREE;
        if (flags.useloges) Serial.println("🔓 Определен бесплатный хост из URL: image.pollinations.ai");
      }
      // Сравниваем с POLLIN_HOST
      else if (strncmp(start, POLLIN_HOST, host_len) == 0 && host_len == strlen(POLLIN_HOST)) {
        target_host = POLLIN_HOST;
        if (flags.useloges) Serial.println("🔒 Определен платный хост из URL: gen.pollinations.ai");
      } else {
        if (flags.useloges) Serial.printf("⚠️ Неизвестный хост: %.*s, используем gen.pollinations.ai\n", (int)host_len, start);
      }
    }
  } else {
    if (flags.useloges) Serial.println("⚠️ Не удалось определить хост из URL, используем gen.pollinations.ai");
  }

  ok_query = request_query(States::RECEIVING, target_host, POLLIN_PORT, url_images);
  WDT_eTimeout(true); // Сброс WDT

  if (ok_query) {
    if (flags.useloges) {
      Serial.printf("✅ Ответ успешно получен [%d/%d]\n", attempt_network_count + 1, try_receive);
      Serial.printf("📊 Всего байт прочитано: %d\n", jpegDataSum);
      Serial.printf("💾 Использование буфера: %.1f%%\n", (jpegDataSum * 100.0) / sz_jpegDataBuf);
    }

    if (flags.useloges && wdt_enlarge) {
      Serial.printf("   WDT таймаут %d мс\n", _wdt_config->timeout_ms);
    }
  }
  else {
    attempt_decoder = true;
  }

  return ok_query;
}

bool NEURGenerator::stop_request() {
  if (http) {
    http->stop();
    http->flush();

    http_stopped = true;
    http_cleanup = true;
  }

  // Полный сброс состояния отправки
  attempt_network_count = 0;
  attempt_decoder_count = 0;
  attempt_decoder = false;

  // Очистка URL и таймеров
  memset(url_images, 0, sz_url_images);
  neur_timer.stop();

  setStateStatus(Status::OK_INITIALIZATION_API);
  return true;
}

bool NEURGenerator::stop_receive() {
  if (http) {
    http->stop();
    http->flush();

    http_stopped = true;
    http_cleanup = true;
  }

  // Полный сброс состояния получения
  attempt_network_count = 0;
  attempt_decoder_count = 0;
  attempt_decoder = false;

  // Очистка буферов
  if (jpegDataBuf) {
    memset(jpegDataBuf, 0, sz_jpegDataBuf);
  }
  jpegDataSum = 0;

  // Очистка URL и таймеров
  memset(url_images, 0, sz_url_images);
  neur_timer.stop();

  // ТОЛЬКО ЕСЛИ ГЕНЕРАЦИЯ ДЕЙСТВИТЕЛЬНО ИДЕТ
  if (isGenerating() && _str_generations > 0) {
    // Как при ошибках сервера
    _end_generations = SafeMillis(_str_generations, millis());
    _end_generations = (_end_generations / POLLIN_PERIOD) * POLLIN_PERIOD;
    _end_generations = max(_end_generations, 60000UL); // минимум 60 секунд

    if (flags.useloges) {
      Serial.printf("⏸️ Остановка во время генерации. Ожидание: %d сек\n",
                    _end_generations / 1000);
    }

    if (_und_cb) {
      _und_cb();
    }
  } else {
    if (flags.useloges) {
      Serial.println("⏸️ Остановка в простое. Callback не вызывается");
    }
  }

  setStateStatus(Status::OK_INITIALIZATION_API);
  return true;
}

bool NEURGenerator::ReaderJPG(Stream& stream) {
  // Лямбда для проверки прогрессивного JPEG
  auto isProgressiveJPEG = [&](const uint8_t* buffer, size_t size) -> bool {
    for (size_t i = 0; i < size - 1; ++i) {
      if (buffer[i] == 0xFF && buffer[i + 1] == 0xC2) {
        return true;
      }
    }
    return false;
  };

  // Лямбда для поиска конца первого слоя
  auto isFirstLayerComplete = [&](const uint8_t* buffer, size_t size) -> bool {
    size_t sos_pos = 0;
    for (size_t i = 0; i < size - 1; ++i) {
      if (buffer[i] == 0xFF && buffer[i + 1] == 0xDA) {
        sos_pos = i;
        break;
      }
    }
    if (sos_pos == 0) return false;

    // Ищем следующий маркер после SOS
    for (size_t j = sos_pos + 4; j < size - 1; ++j) {
      if (buffer[j] == 0xFF && buffer[j + 1] != 0x00) {
        uint8_t marker = buffer[j + 1];
        if (marker >= 0xD0 && marker <= 0xD7) continue;
        return true;
      }
    }
    return false;
  };

  if (!jpegDataBuf) {
    jpegDataBuf = (char*)ps_malloc(sz_jpegDataBuf);
    if (!jpegDataBuf) {
      setStateStatus(Status::ERROR_INITMEMORY);

      memset(url_images, 0, sz_url_images);
      neur_timer.stop();

      attempt_decoder = false;
      return false;
    }
  }

  WDT_sTimeout();

  jpegDataSum = 0;
  stream.setTimeout(1000);

  memset(jpegDataBuf, 0, sz_jpegDataBuf);

  bool type_detect = false;
  bool is_progress = false;
  bool is_complete = false;

  bool is_flux_adjust = (flags.api_adjust && strcmp(api_models, "flux") == 0);
  bool is_sana_adjust = (flags.api_adjust && strcmp(api_models, "sana") == 0);
  bool is_dreamshaper_adjust = (flags.api_adjust && strcmp(api_models, "dreamshaper") == 0);

  // Читаем данные
  while (stream.available() > 0) {
    WDT_eTimeout(false);

    size_t needed = jpegDataSum + 512 + 1;
    if (!ExpandBuffer(jpegDataBuf, sz_jpegDataBuf, needed, BUF_EXPAND_INT, BUF_EXPAND_MAX)) {
      setStateStatus(Status::ERROR_INITMEMORY);
      if (flags.useloges) Serial.printf("❌ Не удалось расширить буфер до %d байт\n", needed);
      break;
    }

    size_t to_read = min((size_t)512, sz_jpegDataBuf - jpegDataSum);
    size_t bytes_read = stream.readBytes((jpegDataBuf + jpegDataSum), to_read);
    if (bytes_read == 0) {
      break;
    }

    jpegDataSum += bytes_read;

    if (!flags.usescreen) {
      // Проверяем наличие заголовка JPEG во всём буфере
      bool validJPEG_start = false;
      for (size_t i = 0; i < jpegDataSum - 1; ++i) {
        if ((uint8_t)jpegDataBuf[i] == 0xFF &&
            (uint8_t)jpegDataBuf[i + 1] == 0xD8) {
          validJPEG_start = true;

          if (flags.useloges) Serial.printf("📴 Экран отключен, заголовок JPEG найден (байт %d)\n", i);
          break;
        }
      }

      if (validJPEG_start) {
        setStateStatus(Status::OK_GENERATING_READILY);

        WDT_eTimeout(true);
        return true;
      }

      // Защита: если прочитали много, а заголовка нет - ошибка
      if (jpegDataSum > 16384) {
        if (flags.useloges) Serial.println("❌ Заголовок JPEG не найден");

        attempt_decoder = true;
        WDT_eTimeout(true);
        return false;
      }

      continue;
    } else {
      // ПЕРВОЕ УСЛОВИЕ: FLUX/SANA/DREAMSHAPER С АДАПТИВНЫМ РАЗМЕРОМ
      if (is_flux_adjust || is_sana_adjust || is_dreamshaper_adjust) {
        if (!type_detect && jpegDataSum >= 16384) {
          is_progress = isProgressiveJPEG((uint8_t*)jpegDataBuf, jpegDataSum);
          type_detect = true;

          if (!flags.repeated) {
            isProgressive[api_number]._flag = is_progress;
            if (flags.useloges) {
              Serial.println(is_progress ? "📸 сервер вернул прогрессивный JPEG" : "📸 сервер вернул базовый JPEG");
            }
          } else if (flags.useloges) {
            Serial.println(is_progress ? "📸 сервер вернул прогрессивный JPEG" : "📸 сервер вернул базовый JPEG");
          }
        }

        if (is_progress) {
          if (isFirstLayerComplete((uint8_t*)jpegDataBuf, jpegDataSum)) {
            if (flags.useloges) Serial.println("⏹️ Конец первого слоя (прогрессивный), изображение готово");
            is_complete = true;
            break;
          }
        } else {
          if (jpegDataSum >= 2) {
            if ((uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
                (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9) {
              if (flags.useloges) Serial.println("⏹️ Конец файла (базовый), изображение готово");
              is_complete = true;
              break;
            }
          }
        }
      }
      // ВТОРОЕ УСЛОВИЕ: Прогрессивный JPEG
      else if (isProgressive[api_number]._flag || flags.repeated) {
        if (flags.repeated && !isProgressive[api_number]._flag) {
          if (!type_detect && jpegDataSum >= 16384) {
            is_progress = isProgressiveJPEG((uint8_t*)jpegDataBuf, jpegDataSum);
            type_detect = true;

            if (flags.useloges) {
              Serial.println(is_progress ? "📸 Повторная загрузка: обнаружен прогрессивный JPEG"
                             : "📸 Повторная загрузка: базовый JPEG");
            }
          }
        }

        if (isProgressive[api_number]._flag || is_progress) {
          if (isFirstLayerComplete((uint8_t*)jpegDataBuf, jpegDataSum)) {
            if (flags.useloges) Serial.println("⏹️ Конец первого слоя, изображение готово");
            is_complete = true;
            break;
          }
        } else {
          if (jpegDataSum >= 2) {
            if ((uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
                (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9) {
              if (flags.useloges) Serial.println("⏹️ Конец файла, изображение готово");
              is_complete = true;
              break;
            }
          }
        }
      }
      // ТРЕТЬЕ УСЛОВИЕ: Определяем тип для новой генерации
      else {
        if (!type_detect && jpegDataSum >= 16384) {
          is_progress = isProgressiveJPEG((uint8_t*)jpegDataBuf, jpegDataSum);

          if (is_progress) {
            type_detect = true;

            if (flags.useloges) Serial.println("📸 Прогрессивный JPEG, нужен повторный запрос");
            isProgressive[api_number]._flag = 1;

            setStateStatus(Status::OK_RETRY_DOWNLOADING);

            return false;
          } else {
            type_detect = true;
            if (flags.useloges) Serial.println("📸 Базовый JPEG, читаем до конца");
            isProgressive[api_number]._flag = 0;
          }
        }

        if (!is_progress && jpegDataSum >= 2) {
          if ((uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
              (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9) {
            if (flags.useloges) Serial.println("⏹️ Конец файла, изображение готово");
            is_complete = true;
            break;
          }
        }
      }
    }
  }

  if (is_complete) {
    WDT_eTimeout(true);
    setStateStatus(Status::OK_GENERATING_READILY);
    return true;
  }

  bool validJPEG_start = (jpegDataSum >= 2 &&
                          (uint8_t)jpegDataBuf[0] == 0xFF &&
                          (uint8_t)jpegDataBuf[1] == 0xD8);

  if (jpegDataSum == 0 || !validJPEG_start) {
    if (flags.useloges) Serial.printf("❌ Данные изображения не получены или невалидны\n");
    WDT_eTimeout(false);
    attempt_decoder = false;
    return false;
  }

  if (is_flux_adjust || is_sana_adjust || is_dreamshaper_adjust) {
    if (is_progress) {
      if (jpegDataSum < 100) {
        if (flags.useloges) Serial.println("❌ Недостаточно данных для первого слоя");
        WDT_eTimeout(false);
        attempt_decoder = true;
        return false;
      }
      if (flags.useloges) Serial.printf("✅ Прогрессивный JPEG (1 слой): %d байт\n", jpegDataSum);
    } else {
      bool validJPEG_end = (jpegDataSum >= 2 &&
                            (uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
                            (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9);
      if (!validJPEG_end) {
        if (flags.useloges) Serial.println("❌ Нет маркера конца JPEG");
        WDT_eTimeout(false);
        attempt_decoder = true;
        return false;
      }
      if (flags.useloges) Serial.printf("✅ Базовый JPEG: %d байт\n", jpegDataSum);
    }
  }
  else if (isProgressive[api_number]._flag) {
    if (jpegDataSum < 100) {
      if (flags.useloges) Serial.println("❌ Недостаточно данных для первого слоя");
      WDT_eTimeout(false);
      attempt_decoder = true;
      return false;
    }
    if (flags.useloges) Serial.printf("✅ Прогрессивный JPEG (1 слой): %d байт\n", jpegDataSum);
  }
  else {
    bool validJPEG_end = (jpegDataSum >= 2 &&
                          (uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
                          (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9);
    if (!validJPEG_end) {
      if (flags.useloges) Serial.println("❌ Нет маркера конца JPEG");
      WDT_eTimeout(false);
      attempt_decoder = true;
      return false;
    }
    if (flags.useloges) Serial.printf("✅ Базовый JPEG: %d байт\n", jpegDataSum);
  }

  WDT_eTimeout(true);
  setStateStatus(Status::OK_GENERATING_READILY);
  return true;
}

void NEURGenerator::PresentImage(const char* _url_images) {
  // Проверка входных данных
  if (!_url_images || _url_images[0] == '\0') {
    if (flags.useloges) Serial.println("❌ Пустой URL для загрузки");
    return;
  }

  // Проверка, что не идет генерация
  if (isGenerating()) {
    if (flags.useloges) Serial.println("❌ Невозможно загрузить: идет генерация");
    return;
  }

  memset(url_images, 0, sz_url_images);
  snprintf(url_images, sz_url_images, "%s", _url_images);

  // Очищаем предыдущие данные
  if (jpegDataBuf) {
    memset(jpegDataBuf, 0, sz_jpegDataBuf);
  }
  jpegDataSum = 0;

  // Устанавливаем статус загрузки
  setStateStatus(Status::OK_DOWNLOADING);

  flags.repeated = true;
}
