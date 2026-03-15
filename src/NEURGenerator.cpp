#include   "NEURGenerator.h"

// Определяем статический член
NEURGenerator* NEURGenerator::self = nullptr;

// Определяем статические члены
uint8_t* NEURGenerator::http_psram_buffer = nullptr;

void NEURGenerator::setStateStatus(Status new_state) {
  state_gen = new_state;

  switch (new_state) {
    case Status::OK_INITIALIZATION_API:
      break;

    case Status::OK_PREPARING_DATA:
      // Начало подготовки данных
      neur_timer.stop();
      break;

    case Status::OK_READY_TO_SEND:
      if (!flags.useolder && _run_cb) {
        _run_cb();
      }

      // Данные подготовлены, готовы к отправке
      _str_generations = millis();
      break;

    case Status::OK_SENDING_ATTEMPT:
      // Начата попытка отправки
      break;

    case Status::OK_SENDING_REQUEST:
      break;

    case Status::OK_WAITING_FOR_RESULT:
      (url_images[0] != '\0') ? neur_timer.start() : neur_timer.stop();
      break;

    case Status::OK_GENERATING_READILY:
      created_image++;
      _end_generations = SafeMillis(_str_generations, millis());
      _end_generations = (_end_generations / POLLIN_PERIOD) * POLLIN_PERIOD;

      if (!flags.useolder && _end_cb) {
        _end_cb();
      }

      neur_timer.stop();

      flags.useolder = false;
      break;

    case Status::OK_TRANSLATE:
      break;

    case Status::OK_DOWNLOADING:
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

      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_REQUESTS:
      memset(url_images, 0, sz_url_images);
      error.request++;

      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_RESPONSE:
      memset(url_images, 0, sz_url_images);
      error.receive++;

      if (jpegDataBuf) {
        memset(jpegDataBuf, 0, sz_jpegDataBuf);
      }

      neur_timer.stop();
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_RECEIVING:
      memset(url_images, 0, sz_url_images);
      error.receive++;

      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_DECODINGS:
      memset(url_images, 0, sz_url_images);
      error.decoder++;

      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_CONNECTION:
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_INITMEMORY:
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_OVERLOAD:
      _end_generations = 120000UL;
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_AUTHENTICATE:
      _end_generations = 300000UL;
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_UNAVAILABLE:
      _end_generations = 600000UL;
      if (!flags.useolder && _err_cb) {
        _err_cb();
      }

      flags.useolder = false;
      break;

    case Status::ERROR_CONVERT:
      break;

    case Status::ERROR_CONVERT_LIMIT:
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
    if (sk_secret[0] != '\0' && strncmp(sk_secret, "Bearer ", 7) == 0) {
      headers.add("Authorization", sk_secret);
    }

    if (flags.useheads) {
      headers.add("Accept"         , "*/*");
      headers.add("User-Agent"     , useragent);
      headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8");
      headers.add("Cache-Control"  , "no-cache");
      headers.add("Connection"     , "keep-alive");
    }
  }
  else if (states == States::API_POLLEN || states == States::API_MODELS) {
    if (flags.useloges) Serial.printf("🔑 Используем API ключ: %s...\n", sk_secret);

    if (sk_secret[0] != '\0' && strncmp(sk_secret, "Bearer ", 7) == 0) {
      headers.add("Authorization", sk_secret);
    }

    if (flags.useheads) {
      headers.add("Accept"         , "application/json");
      headers.add("User-Agent"     , useragent);
      headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8");
      headers.add("Cache-Control"  , "no-cache");
      headers.add("Connection"     , "keep-alive");
    }
  }
  else if (states == States::TRANSLATE) {
    if (flags.useheads) {
      headers.add("Accept"         , "*/*");
      headers.add("User-Agent"     , useragent);
      headers.add("Accept-Language", "ru-RU,ru;q=0.9,en;q=0.8");
      headers.add("Cache-Control"  , "no-cache");
      headers.add("Connection"     , "keep-alive");
    }
  }

  bool ok_query = false;

  if (flags.usetasks && wdt_enlarge && (states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
    esp_task_wdt_config_t long_wdt_config = {
      .timeout_ms = WDT_SURPLUS + try_clients + _wdt_config->timeout_ms,
      .idle_core_mask = _wdt_config->idle_core_mask,
      .trigger_panic = _wdt_config->trigger_panic
    };

    esp_task_wdt_reconfigure(&long_wdt_config);
  }

  if (data) {
    ok_query = http->request(path, method, headers, *data);
  } else {
    ok_query = http->request(path, method, headers);
  }

  if (!ok_query) {
    http->~EspInsecureClient();
    WDT_eTimeout(false);

    if (flags.usetasks && wdt_enlarge && (states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
      esp_task_wdt_reconfigure(_wdt_config);
    }
    return false;
  }

  WDT_eTimeout(false);//Сброс WDT

  ghttp::Client::Response resp = http->getResponse();

  if (!resp) {
    http->~EspInsecureClient();
    WDT_eTimeout(false);

    if (flags.usetasks && wdt_enlarge && (states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
      esp_task_wdt_reconfigure(_wdt_config);
    }
    return false;
  }

  try_httpcode = resp.code();

  // ⭐ ВОССТАНАВЛИВАЕМ ОРИГИНАЛЬНЫЙ ТАЙМАУТ WDT
  if (flags.usetasks && wdt_enlarge && (states == States::DEPARTURE || states == States::RECEIVING || states == States::API_POLLEN || states == States::API_MODELS)) {
    esp_task_wdt_reconfigure(_wdt_config);
  }

  if (states == States::DEPARTURE || states == States::RECEIVING) {
    if (try_httpcode != 200) {
      http->~EspInsecureClient();

      switch (try_httpcode) {
        case 400:
          break;

        case 401:
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
        setStateStatus(Status::ERROR_CONVERT_LIMIT);
      } else {
        setStateStatus(Status::ERROR_CONVERT); // Другие ошибки перевода
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

  if (http_stopped) {
    return;
  }

  if (!WiFiState) return;

  if (state_gen == Status::OK_DOWNLOADING && url_images && url_images[0] != '\0') {
    setStateStatus(Status::OK_WAITING_FOR_RESULT);
    attempt_network_count = 0;
    if (flags.useloges) Serial.println("🚀 Начинаем попытки получения");

    if (flags.useloges && flags.usetasks && wdt_enlarge) {
      Serial.printf("   WDT таймаут будет увеличен до %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
    }
  }

  // ⭐ ОБРАБОТКА: готов к отправке -> начинаем попытки отправки
  if (state_gen == Status::OK_READY_TO_SEND) {
    setStateStatus(Status::OK_SENDING_ATTEMPT);
    attempt_network_count = 0;
    if (flags.useloges) Serial.println("🚀 Начинаем попытки отправки");

    if (flags.useloges && flags.usetasks && wdt_enlarge) {
      Serial.printf("   WDT таймаут будет увеличен до %d мс\n", WDT_SURPLUS + try_clients + _wdt_config->timeout_ms);
    }
  }

  // ⭐ ОБРАБОТКА ОТПРАВКИ ЗАПРОСА
  if (state_gen == Status::OK_SENDING_ATTEMPT && url_images && url_images[0] != '\0') {
    if (attempt_network_count < try_request) {
      if (flags.useloges) Serial.printf("   - отправка запроса [%d/%d]\n", attempt_network_count + 1, try_request);

      // Делаем паузу между попытками (кроме первой)
      if (attempt_network_count > 0) {
        vTaskDelay(pdMS_TO_TICKS(try_timeout));
      }

      WDT_eTimeout(true); // Сброс WDT
      bool ok_query = request_query(States::DEPARTURE, POLLIN_HOST, POLLIN_PORT, url_images, "GET");
      WDT_eTimeout(true); // Сброс WDT

      if (ok_query) {
        // Успешная отправка
        if (flags.useloges) Serial.printf("✅ Запрос успешно отправлен [%d/%d]\n", attempt_network_count + 1, try_request);
        setStateStatus(Status::OK_WAITING_FOR_RESULT);
        attempt_network_count = 0;

        if (flags.useloges && flags.usetasks && wdt_enlarge) {
          Serial.printf("   WDT таймаут восстановлен до %d мс\n", _wdt_config->timeout_ms);
        }
      } else {
        // Ошибка отправки
        attempt_network_count++;

        flags.critical = false;

        switch (try_httpcode) {
          case 400:
            flags.critical = (attempt_network_count >= (try_request - 1));
            break;

          case 401:
            flags.critical = true;
            break;

          case 403:
            flags.critical = true;
            break;

          case 404:
            flags.critical = (attempt_network_count >= (try_request - 1));
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

        // ⭐ ДОБАВЛЕНО: Немедленная остановка при критических ошибках
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

            case 403:
              if (flags.useloges) Serial.println("❌ ДОСТУП ЗАПРЕЩЕН (403): Нет прав для запроса");
              setStateStatus(Status::ERROR_AUTHENTICATE);
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

          if (flags.useloges && flags.usetasks && wdt_enlarge) {
            Serial.printf("   WDT таймаут восстановлен до %d мс\n", _wdt_config->timeout_ms);
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

          if (flags.useloges && flags.usetasks && wdt_enlarge) {
            Serial.printf("   WDT таймаут восстановлен до %d мс\n", _wdt_config->timeout_ms);
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

  // ⭐ ОБРАБОТКА ПОЛУЧЕНИЯ ОТВЕТА (существующая логика)
  else if (state_gen == Status::OK_WAITING_FOR_RESULT && url_images && url_images[0] != '\0' && neur_timer.period(POLLIN_PERIOD)) {
    if (resp_receive()) {
      attempt_network_count = 0;
      attempt_decoder_count = 0; // Сброс при успехе
      attempt_decoder = false;
    } else {
      attempt_network_count++;

      // ⭐ ПРОВЕРКА СЕТЕВЫХ ОШИБОК
      if (attempt_network_count >= try_receive) {
        setStateStatus(Status::ERROR_RESPONSE);
        _end_generations = SafeMillis(_str_generations, millis());
        _end_generations = (_end_generations / POLLIN_PERIOD) * POLLIN_PERIOD;

        attempt_network_count = 0;
        attempt_decoder_count = 0;
        attempt_decoder = false;
      }

      // ⭐ ПРОВЕРКА ОШИБОК ДЕКОДИРОВАНИЯ
      if (attempt_decoder && attempt_decoder_count >= try_receive) {
        setStateStatus(Status::ERROR_DECODINGS);
        _end_generations = SafeMillis(_str_generations, millis());
        _end_generations = (_end_generations / POLLIN_PERIOD) * POLLIN_PERIOD;

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

  // ⭐ Очищаем буфер перед каждым использованием
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

  if (json["balance"].c_str()) {
    memset(_pollen, 0, sz_api_pollen);

    strncpy(_pollen, json["balance"].c_str(), sz_api_pollen - 1);
    _pollen[sz_api_pollen - 1] = '\0';
  } else {
    _pollen[0] = '\0';
  }

  if (_pollen[0] != '\0') {
    memset(api_pollen, 0, sz_api_pollen);

    // Ищем точку в числе
    char* dot_ptr = strchr(_pollen, '.');

    if (dot_ptr) {
      // Копируем часть до точки
      size_t int_len = dot_ptr - _pollen;
      strncpy(api_pollen, _pollen, int_len);
      api_pollen[int_len] = '.';

      // Копируем 4 цифры после точки
      const char* fraction_ptr = dot_ptr + 1;
      for (int i = 0; i < 4; i++) {
        if (fraction_ptr[i] >= '0' && fraction_ptr[i] <= '9') {
          api_pollen[int_len + 1 + i] = fraction_ptr[i];
        } else {
          api_pollen[int_len + 1 + i] = '0'; // Дополняем нулями если недостаточно цифр
        }
      }
      api_pollen[int_len + 5] = '\0'; // Завершаем строку
    } else {
      // Если точки нет, добавляем ".0000"
      snprintf(api_pollen, sz_api_pollen, "%s.0000", _pollen);
    }

    if (flags.useloges) Serial.printf("✅ Баланс получен: %s pollen\n", api_pollen);

    heap_caps_free(_pollen);
    return true;
  }
  else {
    // НЕ ЗАПИСЫВАЕМ API_ERROR_JSON, ЕСЛИ БАЛАНС УЖЕ БЫЛ ПОЛУЧЕН
    if (strcmp(api_pollen, API_POLLEN_NO_DATA) == 0 ||
        strcmp(api_pollen, API_NO_ACCESS) == 0 ||
        strcmp(api_pollen, API_ERROR_JSON) == 0) {

      // Только для начальных состояний
      memset(api_pollen, 0, sz_api_pollen);
      snprintf(api_pollen, sz_api_pollen - 1, API_ERROR_JSON);

      if (flags.useloges) Serial.println("❌ Ошибка парсинга баланса");
    } else {
      // Баланс уже был получен ранее, сохраняем старое значение
      if (flags.useloges) Serial.printf("⚠️  Ошибка парсинга, но баланс уже был: %s pollen\n", api_pollen);
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
    snprintf(json_models.names, sz_model_names - 1, API_MODELS_NAMES); // "zimage"

    memset(json_models.title, 0, sz_model_title);
    snprintf(json_models.title, sz_model_title - 1, API_MODELS_TITLE); // "Z-Image Turbo"

    memset(json_models.price, 0, sz_model_price);
    snprintf(json_models.price, sz_model_price - 1, API_MODELS_PRICE); // "0.0002"

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

    if (json[i]["description"].c_str()) {
      strncpy(_title, json[i]["description"].c_str(), sz_model_title - 1);
      _title[sz_model_title - 1] = '\0';
    }

    if (json[i]["pricing"]["completionImageTokens"].c_str()) {
      strncpy(_price, json[i]["pricing"]["completionImageTokens"].c_str(), sz_model_price - 1);
      _price[sz_model_price - 1] = '\0';
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

  switch (api_scales) {
    case APIScales::SCALE_LOW   :
      _requestWidth  = 480;
      _requestHeight = 320;
      break;

    case APIScales::SCALE_MEDIUM:
      _requestWidth  = 640;
      _requestHeight = 480;
      break;

    case APIScales::SCALE_HIGH  :
      _requestWidth  = 960;
      _requestHeight = 640;
      break;

    default                    :
      _requestWidth  = 480;
      _requestHeight = 320;
      break;
  }

  const uint16_t max_dimension = 960;
  if (_requestWidth == 0 || _requestHeight == 0 || _requestWidth > max_dimension || _requestHeight > max_dimension) {
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

  // Проверяем пинг сервера
  bool possible = true;

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
  if (flags.translate && isRussianText(prompt)) {
    if (possible) {
      // Перевод возможен (есть связь)
      strcpy(rus_prompt, prompt);

      // Пытаемся перевести
      url_encode(prompt, enc_prompt);
      memset(url_transl, 0, sz_url_transl);

      // Формируем URL запроса с параметром email если установлен
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
      // Перевод нужен, но связи нет - используем оригинальный
      strcpy(tmp_prompt, prompt);
      if (flags.useloges) Serial.println("⚠️  Использован оригинальный промпт (нет связи с переводчиком)");
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
  if (denial && denial[0] != '\0') {
    // Кодирование negative_prompt
    url_encode(denial, enc_denial);

    if (pk_secret && pk_secret[0] != '\0') {
      // Есть negative_prompt И есть ключ
      snprintf(url_images, sz_url_images,
               "https://%s/image/%s?seed=%d"
               "&negative_prompt=%s&model=%s&enhance=%s&private=true&nologo=true&quality=%s&%s&safe=%s"
               "&%s",
               POLLIN_HOST,
               enc_prompt, seed,
               enc_denial,
               api_models_str,
               api_enhanc_str,
               api_levels_str,
               api_scales_str,
               api_filter_str,
               pk_secret);
    } else {
      // Есть negative_prompt, НО нет ключа
      snprintf(url_images, sz_url_images,
               "https://%s/image/%s?seed=%d"
               "&negative_prompt=%s&model=%s&enhance=%s&private=true&nologo=true&quality=%s&%s&safe=%s",
               POLLIN_HOST,
               enc_prompt, seed,
               enc_denial,
               api_models_str,
               api_enhanc_str,
               api_levels_str,
               api_scales_str,
               api_filter_str);
    }
  } else {
    if (pk_secret && pk_secret[0] != '\0') {
      // Нет negative_prompt, НО есть ключ
      snprintf(url_images, sz_url_images,
               "https://%s/image/%s?seed=%d"
               "&model=%s&enhance=%s&private=true&nologo=true&quality=%s&%s&safe=%s"
               "&%s",
               POLLIN_HOST,
               enc_prompt, seed,
               api_models_str,
               api_enhanc_str,
               api_levels_str,
               api_scales_str,
               api_filter_str,
               pk_secret);
    } else {
      // Нет negative_prompt И нет ключа
      snprintf(url_images, sz_url_images,
               "https://%s/image/%s?seed=%d"
               "&model=%s&enhance=%s&private=true&nologo=true&quality=%s&%s&safe=%s",
               POLLIN_HOST,
               enc_prompt, seed,
               api_models_str,
               api_enhanc_str,
               api_levels_str,
               api_scales_str,
               api_filter_str);
    }
  }

  // 11. Логирование сформированного URL
  if (flags.useloges) {
    Serial.print("🔗 Сформирован URL: ");
    Serial.println(url_images);
  }

  // 12. Установить статус "готов к отправке"
  setStateStatus(Status::OK_READY_TO_SEND);

  return true;
}

bool NEURGenerator::send_request() {
  // Проверяем, что URL сформирован
  if (url_images == nullptr || url_images[0] == '\0') {
    setStateStatus(Status::ERROR_AIGENERATION);
    return false;
  }

  // Проверяем пинг сервера
  bool ok_pings = getPingServer(POLLIN_HOST);
  if (!ok_pings) {
    if (flags.useloges) Serial.printf("❌ Ошибка связи с сервером\n");
    setStateStatus(Status::ERROR_CONNECTION);
    return false;
  } else {
    if (flags.useloges) Serial.printf("✅ Связь с сервером установлена\n");
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

  WDT_sTimeout();//Запуск WDT
  if (flags.useloges) Serial.printf("   - получение ответа [%d/%d]\n", attempt_network_count + 1, try_receive);

  ok_query = request_query(States::RECEIVING, POLLIN_HOST, POLLIN_PORT, url_images);
  WDT_eTimeout(true); // Сброс WDT


  if (ok_query) {
    if (flags.useloges) {
      Serial.printf("✅ Ответ успешно получен [%d/%d]\n", attempt_network_count + 1, try_receive);
      Serial.printf("📊 Всего байт прочитано: %d\n", jpegDataSum);
      Serial.printf("💾 Использование буфера: %.1f%%\n", (jpegDataSum * 100.0) / sz_jpegDataBuf);
    }

    if (flags.useloges && flags.usetasks && wdt_enlarge) {
      Serial.printf("   WDT таймаут восстановлен до %d мс\n", _wdt_config->timeout_ms);
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

  // ⭐ ПРОВЕРКА: ТОЛЬКО ЕСЛИ ГЕНЕРАЦИЯ ДЕЙСТВИТЕЛЬНО ИДЕТ
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

  memset(jpegDataBuf, 0, sz_jpegDataBuf);//очищаем буфер перед каждым чтением

  while (stream.available() > 0 && jpegDataSum < sz_jpegDataBuf) {
    WDT_eTimeout(false);

    size_t to_read = min((size_t)512, sz_jpegDataBuf - jpegDataSum);
    size_t bytes_read = stream.readBytes((jpegDataBuf + jpegDataSum), to_read);
    if (bytes_read == 0) {
      break;
    }

    jpegDataSum += bytes_read;

    if (jpegDataSum >= 2) {
      if ((uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF &&
          (uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9) {
        break;
      }
    }
  }

  bool validJPEG_one = ((uint8_t)jpegDataBuf[0] == 0xFF && (uint8_t)jpegDataBuf[1] == 0xD8);
  bool validJPEG_two = (jpegDataSum >= 2) &&
                       ((uint8_t)jpegDataBuf[jpegDataSum - 2] == 0xFF) &&
                       ((uint8_t)jpegDataBuf[jpegDataSum - 1] == 0xD9);

  if (jpegDataSum == 0) {
    if (flags.useloges) Serial.printf("Данные изображения не получены\n");
    WDT_eTimeout(false);

    attempt_decoder = false;
    return false;
  }

  if (!validJPEG_one || !validJPEG_two) {
    WDT_eTimeout(false);

    attempt_decoder = true;
    return false;
  }

  // Успешно получили данные JPEG в буфер jpegDataBuf, размер jpegDataSum
  if (flags.useloges) {
    Serial.printf("✅ JPEG получен, размер: %d байт\n", jpegDataSum);
  }

  setStateStatus(Status::OK_GENERATING_READILY);
  attempt_decoder_count = 0;
  attempt_decoder = false;

  WDT_eTimeout(true);

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

  flags.useolder = true;
}
