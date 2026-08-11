#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <functional>
#include <algorithm>
#include <vector>

#define GSON_NO_LIMITS
#include <GSON.h>
#include <GyverHTTP.h>
#include <GTimer.h>
#include <ESP32Ping.h>

#include <esp_err.h>
#include <esp_task_wdt.h>

namespace NEURGeneratorConstants {
constexpr char     POLLIN_HOST[] = "gen.pollinations.ai";
constexpr char     POLLIN_FREE[] = "image.pollinations.ai";
constexpr uint16_t POLLIN_PORT   =  443;
constexpr uint16_t POLLIN_PERIOD = 5000;

constexpr char     TRANS_HOST[]  = "api.mymemory.translated.net";
constexpr uint16_t TRANS_PORT    =  443;

constexpr uint16_t WDT_SURPLUS   = 20000;
constexpr uint16_t TIME_PERIOD   =  5000;
constexpr uint16_t WAIT_PERIOD   = 15000;
constexpr uint16_t STAY_PERIOD   =  2000;

constexpr uint16_t PING_DELAYS   =   500;
constexpr uint8_t  PING_TRYING   =     5;

constexpr size_t    sz_sk_secret =   128;
constexpr size_t    sz_pk_secret =   128;
constexpr size_t    sz_mymemmory =   128;
constexpr size_t    sz_translate =  1024;

constexpr size_t   sz_wrk_status =   128;
constexpr size_t   sz_url_images =  2048;
constexpr size_t   sz_url_transl =  2048;
constexpr size_t   sz_tmp_prompt =  4096;

constexpr size_t   sz_rus_prompt =  2048;
constexpr size_t   sz_eng_prompt =  2048;

constexpr size_t   sz_enc_prompt =  8192;
constexpr size_t   sz_enc_denial =  2048;
constexpr size_t   sz_JsonBuffer =  4096;

constexpr size_t  sz_api_models  =    32;
constexpr size_t  sz_api_pollen  =    32;
constexpr size_t  sz_model_names =   256;
constexpr size_t  sz_model_title =   512;
constexpr size_t  sz_model_price =   128;

constexpr size_t  BUF_EXPAND_INT =  65536;
constexpr size_t  BUF_EXPAND_MAX = 786432;
}

#define API_POLLEN_NO_DATA    "нет данных"
#define API_NO_ACCESS         "нет доступа"
#define API_ERROR_JSON        "ошибка чтения"

#define API_MODELS_NAMES      "dreamshaper"
#define API_MODELS_TITLE      "DreamShaper 8 LCM"
#define API_MODELS_PRICE      "0.0001"
#define API_MODELS_COUNT      5

using namespace NEURGeneratorConstants;

class NEURGenerator {
  public:
    enum class States : uint8_t {
      DEPARTURE,
      RECEIVING,
      TRANSLATE,

      API_POLLEN,
      API_MODELS
    };

    enum class Status : uint8_t {
      OK_INITIALIZATION_API,     // 0 - инициализация
      OK_WAITING_COMMAND   ,     // 1 - ожидание команды
      OK_PREPARING_DATA    ,     // 2 - подготовка данных

      OK_SENDING_REQUEST   ,     // 3 - отправка запроса
      OK_SENDING_ATTEMPT   ,     // 4 - попытка отправки
      OK_RECEIVING_REQUEST ,     // 5 - получение запроса
      OK_RECEIVING_ATTEMPT ,     // 6 - получение ответа

      OK_WAITING_FOR_RESULT,     // 7 - ожидание результата
      OK_GENERATING_READILY,     // 8 - готово
      OK_TRANSLATE         ,     // 9 - перевод промта
      OK_DOWNLOADING       ,     // 10 - загрузка картинки
      OK_RETRY_DOWNLOADING ,     // 11 - повторная загрузка
      GET_API_POLLEN       ,     // 12 - загрузка баланса
      GET_API_POLLEN_OK    ,     // 13 - загрузка баланса успех
      GET_API_POLLEN_ERR   ,     // 14 - загрузка баланса ошибка
      GET_API_MODELS       ,     // 15 - загрузка моделей
      GET_API_MODELS_OK    ,     // 16 - загрузка моделей успех
      GET_API_MODELS_ERR   ,     // 17 - загрузка моделей ошибка

      ERROR_REQUESTS       ,     // 18 - ошибка отправки запроса
      ERROR_RESPONSE       ,     // 19 - ошибка получения ответа
      ERROR_AIGENERATION   ,     // 20 - ошибка AI генерации
      ERROR_RECEIVING      ,     // 21 - ошибка получения данных
      ERROR_DECODINGS      ,     // 22 - ошибка декодирования
      ERROR_CONNECTION     ,     // 23 - ошибка подключения
      ERROR_INITMEMORY     ,     // 24 - ошибка памяти PSRAM
      ERROR_OVERLOAD       ,     // 25 - ошибка много запросов (429)
      ERROR_AUTHENTICATE   ,     // 26 - ошибка авторизации (401)
      ERROR_BALANCEBUDGET  ,     // 27 - ошибка недостаточно баланса (402)
      ERROR_ACCESSDENIED   ,     // 28 - ошибка доступа (403)
      ERROR_LOADEDOLDIMAGES,     // 29 - изображение недоступно
      ERROR_UNAVAILABLE    ,     // 30 - Сервер недоступен (500)
      ERROR_CONVERT        ,     // 31 - Ошибка перевода
      ERROR_CONVERT_LIMIT        // 32 - Лимит переводов
    };

    enum class APILevels : uint8_t {
      LEVEL_LOW   ,
      LEVEL_MEDIUM,
      LEVEL_HIGH
    };

    enum class APIScales : uint8_t {
      SCALE_LOW   ,
      SCALE_MEDIUM,
      SCALE_HIGH
    };

    typedef std::function<void()> RenderRunCallback;
    typedef std::function<void()> RenderTftCallback;
    typedef std::function<void()> RenderEndCallback;
    typedef std::function<void()> RenderErrCallback;
    typedef std::function<void()> RenderEngCallback;
    typedef std::function<void()> RenderUndCallback;
    typedef std::function<void()> RenderRetCallback;
    typedef std::function<void()> RenderDelCallback;

    void onRenderRun(RenderRunCallback cb) {
      _run_cb = cb;
    }

    void onRenderTft(RenderTftCallback cb) {
      _tft_cb = cb;
    }

    void onRenderEnd(RenderEndCallback cb) {
      _end_cb = cb;
    }

    void onRenderErr(RenderErrCallback cb) {
      _err_cb = cb;
    }

    void onRenderEng(RenderEngCallback cb) {
      _eng_cb = cb;
    }

    void onRenderUnd(RenderUndCallback cb) {
      _und_cb = cb;
    }

    void onRenderRet(RenderRetCallback cb) {
      _ret_cb = cb;
    }

    void onRenderDel(RenderDelCallback cb) {
      _del_cb = cb;
    }

    // Конструктор
    NEURGenerator() {
      self = this;

      for (uint8_t i = 0; i < API_MODELS_COUNT; ++i) {
        isProgressive[i].model = 0;
        isProgressive[i]._flag = 0;
      }

      setVAL();
    }

    void setKeySecret(const char* _sk_secret, const char* _pk_secret) {
      memset(sk_secret, 0, sz_sk_secret);
      memset(pk_secret, 0, sz_pk_secret);

      if (_sk_secret && _sk_secret[0] != '\0') {
        snprintf(sk_secret, sz_sk_secret, "Bearer %s", _sk_secret);
      }

      if (_pk_secret && _pk_secret[0] != '\0') {
        snprintf(pk_secret, sz_pk_secret, "key=%s", _pk_secret);
      }
    }

    void setMyMemmory(const char* _mymemmory) {
      if (_mymemmory == nullptr || _mymemmory[0] == '\0') {
        memset(mymemmory, 0, sz_mymemmory);
        return;
      }

      memset(mymemmory, 0, sz_mymemmory);
      snprintf(mymemmory, sz_mymemmory, "%s", _mymemmory);
    }

    void setBUF(char* ext_jpegDataBuf = nullptr) {
      if (ext_jpegDataBuf) {
        if (jpegDataBuf && jpegDataBuf != ext_jpegDataBuf) {
          heap_caps_free(jpegDataBuf);
        }
        jpegDataBuf = ext_jpegDataBuf;
      } else if (!jpegDataBuf) {
        jpegDataBuf = (char*)ps_malloc(sz_jpegDataBuf);
      }
    }

    void setVAL() {
      memset(wrk_status, 0, sz_wrk_status);
      memset(url_images, 0, sz_url_images);
      memset(url_transl, 0, sz_url_transl);

      memset(tmp_prompt, 0, sz_tmp_prompt);

      memset(rus_prompt, 0, sz_rus_prompt);
      memset(eng_prompt, 0, sz_eng_prompt);

      memset(enc_prompt, 0, sz_enc_prompt);
      memset(enc_denial, 0, sz_enc_denial);

      memset(api_models, 0, sz_api_models);
      snprintf(api_models, sz_api_models - 1, API_MODELS_NAMES);

      memset(api_pollen, 0, sz_api_pollen);
      snprintf(api_pollen, sz_api_pollen - 1, API_POLLEN_NO_DATA);

      memset(json_models.names, 0, sz_model_names);
      snprintf(json_models.names, sz_model_names - 1, API_MODELS_NAMES);

      memset(json_models.title, 0, sz_model_title);
      snprintf(json_models.title, sz_model_title - 1, API_MODELS_TITLE);

      memset(json_models.price, 0, sz_model_price);
      snprintf(json_models.price, sz_model_price - 1, API_MODELS_PRICE);

      memset(temp_models.names, 0, 64);
      memset(temp_models.title, 0, 64);
      memset(temp_models.price, 0, 64);

      state_gen = Status::OK_INITIALIZATION_API;
      state_upd = false;
    }

    void setWDT(const uint16_t& wdt_timeout = 5000, esp_task_wdt_config_t* wdt_config = nullptr) {
      _timeoutMs = wdt_timeout;

      if (wdt_config) {
        _wdt_config = wdt_config;
      }
    }

    void setUsePings(const bool& use) {
      flags.usepings = use;
    }
    void setUseTasks(const bool& use) {
      flags.usetasks = use;
    }
    void setUseLoges(const bool& use) {
      flags.useloges = use;
    }

    void setUseScreen(const bool& use) {
      flags.usescreen = use;
    }

    void setAPINumber(const uint8_t _api_number = 0) {
      api_number = _api_number;
    }

    void setAPIModels(const char* _api_models = API_MODELS_NAMES) {
      memset(api_models, 0, sz_api_models);
      snprintf(api_models, sz_api_models - 1, _api_models);
    }

    void setAPILevels(const uint8_t _api_levels = 0) {
      switch (_api_levels) {
        case 0:
          api_levels = APILevels::LEVEL_LOW   ;
          break;

        case 1:
          api_levels = APILevels::LEVEL_MEDIUM;
          break;

        case 2:
          api_levels = APILevels::LEVEL_HIGH  ;
          break;

        default:
          api_levels = APILevels::LEVEL_LOW   ;
          break;
      }
    }

    void setAPIScales(const uint8_t _api_scales = 0) {
      switch (_api_scales) {
        case 0:
          api_scales = APIScales::SCALE_LOW   ;
          break;

        case 1:
          api_scales = APIScales::SCALE_MEDIUM;
          break;

        case 2:
          api_scales = APIScales::SCALE_HIGH  ;
          break;

        default:
          api_scales = APIScales::SCALE_LOW   ;
          break;
      }
    }

    void setAPIFreely(const bool _api_freely = false) {
      flags.api_freely = _api_freely;
    }

    void setAPIAdjust(const bool _api_adjust = false) {
      flags.api_adjust = _api_adjust;
    }

    void setAPISwitch(const bool _api_switch = false) {
      flags.api_switch = _api_switch;
    }

    void setAPIEnhanc(const bool _api_enhanc = false) {
      api_enhanc = _api_enhanc;
    }

    void setAPIFilter(const bool _api_filter = false) {
      api_filter = _api_filter;
    }

    void setAttempts(const uint16_t& _clients = 5000, const uint16_t& _timeout = 2500, const uint8_t& _request = 5, const uint8_t& _receive = 5) {
      try_clients = _clients;
      try_timeout = _timeout;
      try_request = _request;
      try_receive = _receive;

      // Автоматическое определение, нужно ли увеличивать WDT таймаут
      // Если время работы клиента (_clients) больше чем текущий таймаут WDT (_timeoutMs),
      // то нужно увеличить таймаут WDT
      wdt_enlarge = (try_clients > _timeoutMs);
    }

    bool getPingServer(const char* host) {
      if (!flags.usepings) {
        return true;
      }

      if (_hostIP == INADDR_NONE || _hostIP[0] == 0) {
        if (!WiFi.hostByName(host, _hostIP)) {
          return false;
        }
      }

      bool ok_pings = false;
      for (uint8_t trying = 0; trying < PING_TRYING; ++trying) {
        WDT_eTimeout(true); // Сброс WDT

        ok_pings = Ping.ping(_hostIP, 1);
        if (ok_pings) {
          break;
        }

        vTaskDelay(pdMS_TO_TICKS(PING_DELAYS));
      }

      return ok_pings;
    }

    bool data_prepare(const char* prompt = "", const char* suffix = "", const char* modifi = "", const char* denial = "", bool translate = false);

    bool send_request();
    bool resp_receive();

    bool stop_request();
    bool stop_receive();

    void PresentImage(const char* _url_images);

    bool PromptTranslate(const char* russian_text);
    bool ParserTranslate(gson::Parser& json);

    bool getApiPollen(const char* _sk_secret);  // запрос баланса
    bool ParserPollen(gson::Parser& json);

    bool getApiModels(const char* _sk_secret);  // запрос моделей
    bool ParserModels(gson::Parser& json);

    uint8_t load_config_from_file(const char* filename);
    bool    create_example_config(const char* filename);
    uint8_t load_config(const char* jsonData, size_t jsonSize);

    const char* getPollen() const {
      return api_pollen;  // возвращает строку с балансом
    }

    const char* getAPIModelsNames() const {
      return json_models.names;  // возвращает строку с моделями
    }
    const char* getAPIModelsTitle() const {
      return json_models.title;  // возвращает строку заголовков
    }
    const char* getAPIModelsPrice() const {
      return json_models.price;  // возвращает строку стоимости
    }

    uint8_t getAPIModelsNamesCount() const {
      if (!json_models.title || json_models.title[0] == '\0') {
        return 0;
      }

      return Text(json_models.title).count(';');
    }

    uint8_t getAPIModelsTitleCount() const {
      if (!json_models.title || json_models.title[0] == '\0') {
        return 0;
      }

      return Text(json_models.title).count(';');
    }

    uint8_t getAPIModelsPriceCount() const {
      if (!json_models.price || json_models.price[0] == '\0') {
        return 0;
      }

      return Text(json_models.price).count(';');
    }

    const char* getAPIModelsNamesByIndex(uint8_t index) const {
      if (!json_models.names || json_models.names[0] == '\0') {
        return API_MODELS_NAMES;
      }

      if (index >= getAPIModelsNamesCount()) {
        return API_MODELS_NAMES;
      }

      strlcpy(temp_models.names, Text(json_models.names).getSub(index, ';').c_str(), 64);
      return temp_models.names;
    }

    const char* getAPIModelsTitleByIndex(uint8_t index) const {
      if (!json_models.title || json_models.title[0] == '\0') {
        return API_MODELS_TITLE;
      }

      if (index >= getAPIModelsTitleCount()) {
        return API_MODELS_TITLE;
      }

      strlcpy(temp_models.title, Text(json_models.title).getSub(index, ';').c_str(), 64);
      return temp_models.title;
    }

    const char* getAPIModelsPriceByIndex(uint8_t index) const {
      if (!json_models.price || json_models.price[0] == '\0') {
        return API_MODELS_PRICE;
      }

      if (index >= getAPIModelsPriceCount()) {
        return API_MODELS_PRICE;
      }

      strlcpy(temp_models.price, Text(json_models.price).getSub(index, ';').c_str(), 64);
      return temp_models.price;
    }

    const char* getAPIModelsDisplay() const {
      return getAPIModelsTitleByIndex(api_number);
    }

    const char* getAPILevelsDisplay() {
      switch (api_levels) {
        case APILevels::LEVEL_LOW   : return "низкое" ;
        case APILevels::LEVEL_MEDIUM: return "среднее";
        case APILevels::LEVEL_HIGH  : return "высокое";
        default                     : return "низкое" ;
      }
    }

    const char* getAPIScalesDisplay() {
      switch (api_scales) {
        case APIScales::SCALE_LOW   : return "маленькие";
        case APIScales::SCALE_MEDIUM: return "средние"  ;
        case APIScales::SCALE_HIGH  : return "большие"  ;
        default                     : return "маленькие";
      }
    }

    const char* getAPILevelsSelect() {
      return "низкое;среднее;высокое";
    }

    const char* getAPIScalesSelect() {
      return "маленькие;средние;большие";
    }

    const char* getAPIModelsString() {
      return api_models;
    }

    const char* getAPILevelsString() {
      switch (api_levels) {
        case APILevels::LEVEL_LOW   : return "low"   ;
        case APILevels::LEVEL_MEDIUM: return "medium";
        case APILevels::LEVEL_HIGH  : return "high"  ;
        default                     : return "low"   ;
      }
    }

    const char* getAPIScalesString() {
      static char _scale_buffer[32];

      // ДЛЯ МОДЕЛЕЙ С ВКЛЮЧЁННЫМ МАСШТАБИРОВАНИЕМ
      if (flags.api_adjust) {
        uint16_t _requestW = 0;
        uint16_t _requestH = 0;
        uint16_t max_dimensionW = 0;
        uint16_t max_dimensionH = 0;

        // Пытаемся получить размеры из конфига
        if (getModelScale(api_models, (uint8_t)api_scales, _requestW, _requestH, max_dimensionW, max_dimensionH)) {
          snprintf(_scale_buffer, sizeof(_scale_buffer), "width=%d&height=%d", _requestW, _requestH);
          return _scale_buffer;
        }

        // ❌ Не нашли в конфиге - используем fallback
        // FALLBACK: FLUX
        if (strcmp(api_models, "flux") == 0) {
          switch (api_scales) {
            case APIScales::SCALE_LOW   : return "width=512&height=384" ;
            case APIScales::SCALE_MEDIUM: return "width=768&height=576" ;
            case APIScales::SCALE_HIGH  : return "width=1024&height=768";
            default                     : return "width=512&height=384" ;
          }
        }
        // FALLBACK: SANA
        else if (strcmp(api_models, "sana") == 0) {
          switch (api_scales) {
            case APIScales::SCALE_LOW   : return "width=480&height=320";
            case APIScales::SCALE_MEDIUM: return "width=528&height=352";
            case APIScales::SCALE_HIGH  : return "width=576&height=384";
            default                     : return "width=480&height=320";
          }
        }
        // FALLBACK: DREAMSHAPER
        else if (strcmp(api_models, "dreamshaper") == 0) {
          switch (api_scales) {
            case APIScales::SCALE_LOW   : return "width=480&height=320";
            case APIScales::SCALE_MEDIUM: return "width=528&height=352";
            case APIScales::SCALE_HIGH  : return "width=576&height=384";
            default                     : return "width=480&height=320";
          }
        }
        // FALLBACK: ZIMAGE (обычный режим)
        else {
          switch (api_scales) {
            case APIScales::SCALE_LOW   : return "width=480&height=320";
            case APIScales::SCALE_MEDIUM: return "width=720&height=480";
            case APIScales::SCALE_HIGH  : return "width=960&height=640";
            default                     : return "width=480&height=320";
          }
        }
      }

      // ПРОВЕРКА ПРОГРЕССИВНОГО JPEG
      if (isProgressive[api_number]._flag) {
        snprintf(_scale_buffer, sizeof(_scale_buffer), "width=%d&height=%d",
                 480 * 2, 320 * 2);
        return _scale_buffer;
      }

      // ОБЫЧНЫЙ РЕЖИМ
      switch (api_scales) {
        case APIScales::SCALE_LOW   : return "width=480&height=320";
        case APIScales::SCALE_MEDIUM: return "width=720&height=480";
        case APIScales::SCALE_HIGH  : return "width=960&height=640";
        default                     : return "width=480&height=320";
      }
    }

    void setStateStatus(Status new_state);

    // НОВЫЙ МЕТОД: проверка обновления состояния
    bool getStateUpdate() {
      if (state_upd) {
        state_upd = false;
        return true;
      }
      return false;
    }

    uint8_t getStateNumber() const {
      return (uint8_t)state_gen;
    }

    const char* getStateStatus(bool expand = false) {
      switch (state_gen) {
        case Status::OK_INITIALIZATION_API:          return expand ? "инициализация API нейросети"                : "..."                   ;
        case Status::OK_WAITING_COMMAND   :          return expand ? "ожидание команды для нейросети"             : "ожидание команды"      ;
        case Status::OK_PREPARING_DATA    :          return expand ? "подготовка данных для отправки запроса"     : "подготовка данных"     ;

        case Status::OK_SENDING_REQUEST   :          return expand ? "подготовка к отправке запроса в нейросеть"  : "подготовка к отправке" ;
        case Status::OK_SENDING_ATTEMPT   :
          snprintf(wrk_status, sz_wrk_status,
                   expand ? "отправка запроса в нейросеть (попытка %2d/%2d)" : "отправка (попытка %2d/%2d)",
                   attempt_network_count + 1, try_request);
          return wrk_status;

        case Status::OK_RECEIVING_REQUEST :          return expand ? "подготовка к загрузке ответа от нейросети"  : "подготовка к загрузке" ;
        case Status::OK_RECEIVING_ATTEMPT :
          snprintf(wrk_status, sz_wrk_status,
                   expand ? "загрузка ответа от нейросети (попытка %2d/%2d)" : "загрузка (попытка %2d/%2d)",
                   attempt_network_count + 1, try_receive);
          return wrk_status;

        case Status::OK_WAITING_FOR_RESULT:          return expand ? "ожидание ответа от нейросети"               : "ожидание результата"   ;
        case Status::OK_GENERATING_READILY:          return expand ? "генерация завершена успешно"                : "изображение получено"  ;
        case Status::OK_TRANSLATE         :          return expand ? "перевод промпта на английский язык"         : "перевод промпта"       ;
        case Status::OK_DOWNLOADING       :          return expand ? "получение сгенерированного изображения"     : "получение изображения" ;
        case Status::OK_RETRY_DOWNLOADING :          return expand ? "повторное получение изображения"            : "повторное получение"   ;
        case Status::GET_API_POLLEN       :          return expand ? "обновление остатка баланса"                 : "обновление баланса"    ;
        case Status::GET_API_POLLEN_OK    :          return expand ? "успешное обновление баланса"                : "баланс обновлён"       ;
        case Status::GET_API_POLLEN_ERR   :          return expand ? "ошибка обновления баланса"                  : "ошибка баланса"        ;
        case Status::GET_API_MODELS       :          return expand ? "обновление доступных моделей"               : "обновление моделей"    ;
        case Status::GET_API_MODELS_OK    :          return expand ? "успешное обновление моделей"                : "модели обновлены"      ;
        case Status::GET_API_MODELS_ERR   :          return expand ? "ошибка обновления моделей"                  : "ошибка моделей"        ;
        case Status::ERROR_REQUESTS       :          return expand ? "не удалось отправить запрос к нейросети"    : "ошибка отправки"       ;
        case Status::ERROR_RESPONSE       :          return expand ? "нейросеть вернула некорректный ответ"       : "ошибка ответа"         ;
        case Status::ERROR_AIGENERATION   :          return expand ? "нейросеть не смогла сгенерировать контент"  : "ошибка AI"             ;
        case Status::ERROR_RECEIVING      :          return expand ? "проблема при получении данных от сервера"   : "ошибка получения"      ;
        case Status::ERROR_DECODINGS      :          return expand ? "сбой при декодировании изображения"         : "ошибка декодирования"  ;
        case Status::ERROR_CONNECTION     :          return expand ? "нет подключения к интернету"                : "нет связи"             ;
        case Status::ERROR_INITMEMORY     :          return expand ? "недостаточно памяти PSRAM для генерации"    : "нет PSRAM"             ;
        case Status::ERROR_OVERLOAD       :          return expand ? "слишком частые запросы, подождите"          : "перегрузка"            ;
        case Status::ERROR_AUTHENTICATE   :          return expand ? "ошибка ключа доступа, проверьте ключ API"   : "ошибка ключа доступа"  ;
        case Status::ERROR_BALANCEBUDGET  :          return expand ? "ошибка недостаточный баланс пыльцы"         : "недостаточный баланс"  ;
        case Status::ERROR_ACCESSDENIED   :          return expand ? "ошибка доступа нет необходимых разрешений"  : "доступ запрещен"       ;
        case Status::ERROR_LOADEDOLDIMAGES:          return expand ? "срок хранения изображения истек"            : "изображение недоступно";
        case Status::ERROR_UNAVAILABLE    :          return expand ? "серверы генерации недоступны"               : "сервер не доступен"    ;
        case Status::ERROR_CONVERT        :          return expand ? "ошибка при переводе промпта"                : "ошибка перевода"       ;
        case Status::ERROR_CONVERT_LIMIT  :          return expand ? "достигнут лимит доступных переводов"        : "лимит переводов"       ;
        default                           :          return expand ? "неизвестна, попробуйте позже"               : "неизвестно"            ;
      }
    }

    bool isWorkApiNow() {
      return state_gen == Status::OK_PREPARING_DATA
             || state_gen == Status::OK_SENDING_REQUEST
             || state_gen == Status::OK_SENDING_ATTEMPT
             || state_gen == Status::OK_RECEIVING_REQUEST
             || state_gen == Status::OK_RECEIVING_ATTEMPT
             || state_gen == Status::OK_WAITING_FOR_RESULT
             || state_gen == Status::OK_TRANSLATE;
    }

    bool isGenerating() {
      return state_gen == Status::OK_PREPARING_DATA
             || state_gen == Status::OK_SENDING_REQUEST
             || state_gen == Status::OK_SENDING_ATTEMPT
             || state_gen == Status::OK_RECEIVING_REQUEST
             || state_gen == Status::OK_RECEIVING_ATTEMPT
             || state_gen == Status::OK_WAITING_FOR_RESULT
             || state_gen == Status::OK_TRANSLATE;
    }

    bool isRequestError() const {
      return state_gen == Status::ERROR_AIGENERATION
             || state_gen == Status::ERROR_CONNECTION
             || state_gen == Status::ERROR_UNAVAILABLE
             || state_gen == Status::ERROR_REQUESTS;
    }

    bool isReceiveError() const {
      return state_gen == Status::ERROR_REQUESTS
             || state_gen == Status::ERROR_RESPONSE
             || state_gen == Status::ERROR_RECEIVING
             || state_gen == Status::ERROR_DECODINGS
             || state_gen == Status::ERROR_OVERLOAD
             || state_gen == Status::ERROR_AUTHENTICATE
             || state_gen == Status::ERROR_BALANCEBUDGET
             || state_gen == Status::ERROR_ACCESSDENIED
             || state_gen == Status::ERROR_UNAVAILABLE;
    }

    int8_t isPollenState() {
      if (state_gen == Status::GET_API_POLLEN_OK ) return  1;
      if (state_gen == Status::GET_API_POLLEN_ERR) return -1;
      return 0;
    }

    int8_t isModelsState() {
      if (state_gen == Status::GET_API_MODELS_OK ) return  1;
      if (state_gen == Status::GET_API_MODELS_ERR) return -1;
      return 0;
    }

    uint16_t getGeneration() {
      return _end_generations / 1000;
    }

    uint32_t getImageCount() {
      return created_image;
    }

    uint16_t getErrRequest() {
      return error.request;
    }

    uint16_t getErrReceive() {
      return error.receive;
    }

    uint16_t getErrDecoder() {
      return error.decoder;
    }

    // ПОЛУЧЕНИЕ ДАННЫХ ИЗОБРАЖЕНИЯ
    uint8_t* getImageData() {
      return (uint8_t*)jpegDataBuf;
    }

    size_t getImageDataSize() {
      return jpegDataSum;
    }

    bool hasImageData() {
      return jpegDataBuf != nullptr && jpegDataSum > 0;
    }

    void clearImageData() {
      if (jpegDataBuf) {
        memset(jpegDataBuf, 0, sz_jpegDataBuf);
      }
      jpegDataSum = 0;
    }

    void setStateErrDecoder() {
      setStateStatus(Status::ERROR_DECODINGS);
    }

    void setStateErrReceive() {
      setStateStatus(Status::ERROR_RECEIVING);
    }

    void setStateErrResponse() {
      setStateStatus(Status::ERROR_RESPONSE);
    }

    void tick(bool WiFiState);

    Status state_gen = Status::OK_INITIALIZATION_API;
    bool state_upd = false;

    uint32_t created_image = 0;

    char*     jpegDataBuf = nullptr;
    size_t sz_jpegDataBuf =  262144;
    size_t    jpegDataSum =       0;

    static uint8_t* http_psram_buffer;

    uint8_t api_number = 0;
    APILevels api_levels = APILevels::LEVEL_LOW;
    APIScales api_scales = APIScales::SCALE_LOW;

    bool api_enhanc = false;
    bool api_filter = false;

    char* api_models = (char*)heap_caps_malloc(sz_api_models, MALLOC_CAP_SPIRAM);
    char* api_pollen = (char*)heap_caps_malloc(sz_api_pollen, MALLOC_CAP_SPIRAM);

    bool    attempt_decoder : 1;
    uint8_t attempt_decoder_count = 0;
    uint8_t attempt_network_count = 0;

    char* wrk_status = (char*)heap_caps_malloc(sz_wrk_status, MALLOC_CAP_SPIRAM);
    char* url_images = (char*)heap_caps_malloc(sz_url_images, MALLOC_CAP_SPIRAM);
    char* url_transl = (char*)heap_caps_malloc(sz_url_transl, MALLOC_CAP_SPIRAM);

    char* tmp_prompt = (char*)heap_caps_malloc(sz_tmp_prompt, MALLOC_CAP_SPIRAM);

    char* rus_prompt = (char*)heap_caps_malloc(sz_rus_prompt, MALLOC_CAP_SPIRAM);
    char* eng_prompt = (char*)heap_caps_malloc(sz_eng_prompt, MALLOC_CAP_SPIRAM);

    char* enc_prompt = (char*)heap_caps_malloc(sz_enc_prompt, MALLOC_CAP_SPIRAM);
    char* enc_denial = (char*)heap_caps_malloc(sz_enc_denial, MALLOC_CAP_SPIRAM);

    struct {
      char* names = (char*)heap_caps_malloc(sz_model_names, MALLOC_CAP_SPIRAM);
      char* title = (char*)heap_caps_malloc(sz_model_title, MALLOC_CAP_SPIRAM);
      char* price = (char*)heap_caps_malloc(sz_model_price, MALLOC_CAP_SPIRAM);
    } json_models;

    struct size_models {
      uint8_t  size_level;
      uint16_t dimensionW;
      uint16_t dimensionH;
    };

    struct size_config {
      char*   names = nullptr;
      uint16_t max_dimensionW;
      uint16_t max_dimensionH;

      size_models scales[3];
      uint8_t  scales_count;
    };

    struct {
      uint8_t model;
      bool    _flag;
    } isProgressive[API_MODELS_COUNT];

    bool isRussianText(const char* text);
    bool isEnglishText(const char* text);

  private:
    uint16_t _requestWidth  = 0;
    uint16_t _requestHeight = 0;

    ghttp::EspInsecureClient* http = nullptr;
    bool http_cleanup = false;
    bool http_stopped = false;

    int32_t try_httpcode =    0;

    uint16_t try_clients = 5000;
    uint16_t try_timeout = 2500;
    uint8_t  try_request =    5;
    uint8_t  try_receive =    5;
    bool     wdt_enlarge = true;

    char* sk_secret = (char*)heap_caps_malloc(sz_sk_secret, MALLOC_CAP_SPIRAM);
    char* pk_secret = (char*)heap_caps_malloc(sz_pk_secret, MALLOC_CAP_SPIRAM);
    char* mymemmory = (char*)heap_caps_malloc(sz_mymemmory, MALLOC_CAP_SPIRAM);
    char* translate = (char*)heap_caps_malloc(sz_translate, MALLOC_CAP_SPIRAM);

    struct {
      char* names = (char*)heap_caps_malloc(64, MALLOC_CAP_SPIRAM);
      char* title = (char*)heap_caps_malloc(64, MALLOC_CAP_SPIRAM);
      char* price = (char*)heap_caps_malloc(64, MALLOC_CAP_SPIRAM);
    } temp_models;

    struct {
      bool translate : 1;

      bool usetasks  : 1;
      bool useloges  : 1;
      bool usepings  : 1;
      bool critical  : 1;

      bool usescreen : 1;
      bool repeated  : 1;
	  
      bool api_freely: 1;
      bool api_adjust: 1;
      bool api_switch: 1;
    } flags;

    struct {
      uint16_t request = 0;//Ошибки отправки запроса
      uint16_t receive = 0;//Ошибки получения ответа
      uint16_t decoder = 0;//Ошибки декодирования
    } error;

    uint32_t _timeoutMs = 0;
    uint32_t _triggerMs = 0;
    esp_task_wdt_config_t* _wdt_config = nullptr;

    uint32_t last_apicommands = 0;
    uint32_t _str_generations = 0;
    uint32_t _end_generations = 0;

    RenderRunCallback _run_cb = nullptr;
    RenderTftCallback _tft_cb = nullptr;
    RenderEndCallback _end_cb = nullptr;
    RenderErrCallback _err_cb = nullptr;
    RenderEngCallback _eng_cb = nullptr;
    RenderUndCallback _und_cb = nullptr;
    RenderRetCallback _ret_cb = nullptr;
    RenderDelCallback _del_cb = nullptr;

    IPAddress  _hostIP;
    char*    JsonBuffer;    const size_t sz_JsonBuffer  =  4096;

    size_config* _models_config = nullptr;
    uint8_t      _model_configs = 0;
    bool         _config_loaded = 0;

    // static
    static NEURGenerator* self;

    uTimer16<millis> neur_timer;

    bool getModelScale(const char* model_name, uint8_t level,
                       uint16_t& _requestW, uint16_t& _requestH,
                       uint16_t& max_dimensionW, uint16_t& max_dimensionH);

    bool ExpandBuffer(char*& buffer, size_t& current_size, size_t needed,
                      size_t expand_step, size_t max_size);

    void url_encode(const char *src, char *dest) {
      const char *hex = "0123456789ABCDEF";
      while (*src) {
        if ((*src >= 'a' && *src <= 'z') ||
            (*src >= 'A' && *src <= 'Z') ||
            (*src >= '0' && *src <= '9') ||
            *src == '-' || *src == '_' || *src == '.' || *src == '~') {
          *dest++ = *src;
        } else if (*src == ' ') {
          *dest++ = '%';
          *dest++ = '2';
          *dest++ = '0';
        } else {
          *dest++ = '%';
          *dest++ = hex[(*src >> 4) & 0xF];
          *dest++ = hex[*src & 0xF];
        }
        src++;
      }
      *dest = '\0';
    }

    bool ReaderJPG(Stream& stream);

    // system
    void cleanupHttp();
    bool request_query(States states, const char* host, const uint16_t port, const char* path, const char* method = "GET", ghttp::Client::FormData* data = nullptr);

    uint32_t SafeMillis(uint32_t millis_s, uint32_t millis_e) {
      return (millis_e >= millis_s) ? (millis_e - millis_s) : (0xFFFFFFFF - millis_s + millis_e + 1);
    }

    void WDT_sTimeout() {
      _triggerMs = millis();
    }

    void WDT_eTimeout(bool force = false) {
      if (force) {
        if (flags.usetasks) {
          esp_task_wdt_reset();
        }
        _triggerMs = millis();
        return;
      }

      if (SafeMillis(_triggerMs, millis()) >= _timeoutMs) {
        _triggerMs = millis();
        if (flags.usetasks) {
          esp_task_wdt_reset();
        }
      }
    }
};
