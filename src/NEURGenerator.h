#pragma once

#include <Arduino.h>
#include <functional>
#include <algorithm>
#include <vector>

#include <GSON.h>
#include <GyverHTTP.h>
#include <GTimer.h>
#include <ESP32Ping.h>

#include <esp_err.h>
#include <esp_task_wdt.h>

namespace NEURGeneratorConstants {
constexpr char     POLLIN_HOST[] = "gen.pollinations.ai";
constexpr uint16_t POLLIN_PORT   =  443;
constexpr uint16_t POLLIN_PERIOD = 5000;

constexpr char     TRANS_HOST[]  = "api.mymemory.translated.net";
constexpr uint16_t TRANS_PORT    =  443;

constexpr uint16_t WDT_SURPLUS   = 20000;

constexpr uint16_t PING_DELAYS   =   500;
constexpr uint8_t  PING_TRYING   =     5;

constexpr size_t    sz_useragent =   128;
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

constexpr size_t  sz_api_models  =    32;
constexpr size_t  sz_api_pollen  =    32;
constexpr size_t  sz_model_names =   256;
constexpr size_t  sz_model_title =   512;
constexpr size_t  sz_model_price =   128;
}

#define API_POLLEN_NO_DATA    "нет данных"
#define API_NO_ACCESS         "нет доступа"
#define API_ERROR_JSON        "ошибка чтения"

#define API_MODELS_NAMES      "flux"
#define API_MODELS_TITLE      "Flux Schnell"
#define API_MODELS_PRICE      "0.001"
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
      OK_PREPARING_DATA    ,     // 1 - подготовка данных
      OK_READY_TO_SEND     ,     // 2 - готов к отправке
      OK_SENDING_ATTEMPT   ,     // 3 - попытка отправки
      OK_SENDING_REQUEST   ,     // 4 - отправка запроса
      OK_WAITING_FOR_RESULT,     // 5 - ожидание результата
      OK_GENERATING_READILY,     // 6 - готово
      OK_TRANSLATE         ,     // 7 - перевод промта
      OK_DOWNLOADING       ,     // 8 - загрузка картинки
      GET_API_POLLEN       ,     // 9 - загрузка баланса
      GET_API_POLLEN_OK    ,     // 10 - загрузка баланса успех
      GET_API_POLLEN_ERR   ,     // 11 - загрузка баланса ошибка
      GET_API_MODELS       ,     // 12 - загрузка моделей
      GET_API_MODELS_OK    ,     // 13 - загрузка моделей успех
      GET_API_MODELS_ERR   ,     // 14 - загрузка моделей ошибка

      ERROR_REQUESTS       ,     // 15 - ошибка отправки запроса
      ERROR_RESPONSE       ,     // 16 - ошибка получения ответа
      ERROR_AIGENERATION   ,     // 17 - ошибка AI генерации
      ERROR_RECEIVING      ,     // 18 - ошибка получения данных
      ERROR_DECODINGS      ,     // 19 - ошибка декодирования
      ERROR_CONNECTION     ,     // 20 - ошибка подключения
      ERROR_INITMEMORY     ,     // 21 - ошибка памяти PSRAM
      ERROR_OVERLOAD       ,     // 22 - Слишком много запросов (429)
      ERROR_AUTHENTICATE   ,     // 23 - Слишком много запросов (401-403)
      ERROR_UNAVAILABLE    ,     // 24 - Сервер недоступен (500)
      ERROR_CONVERT        ,     // 25 - Ошибка перевода
      ERROR_CONVERT_LIMIT        // 26 - Лимит переводов
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
    typedef std::function<void()> RenderEndCallback;
    typedef std::function<void()> RenderErrCallback;
    typedef std::function<void()> RenderEngCallback;
    typedef std::function<void()> RenderUndCallback;

    void onRenderRun(RenderRunCallback cb) {
      _run_cb = cb;
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

    // Конструкторы
    NEURGenerator() = delete;

    // Конструктор без TFT
    NEURGenerator(const char* _sk_secret, const char* _pk_secret, const char* _mymemmory) {
      self = this;

      setKeySecret(_sk_secret, _pk_secret);
      setMyMemmory(_mymemmory);

      setWDT();
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

    void setVAL(const char* ProjDevName, const char* ProjDevVers) {
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

      memset(useragent, 0, sz_useragent);
      snprintf(useragent, sz_useragent, "%s %s", ProjDevName, ProjDevVers);

      state_gen = Status::OK_INITIALIZATION_API;
    }

    void setWDT(const uint16_t& wdt_timeout = 5000, esp_task_wdt_config_t* wdt_config = nullptr) {
      _timeoutMs = wdt_timeout;

      if (wdt_config) {
        _wdt_config = wdt_config;
      }
    }

    void setUseHeads(const bool& use) {
      flags.useheads = use;
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
        case APIScales::SCALE_LOW   : return "480x320";
        case APIScales::SCALE_MEDIUM: return "720x480";
        case APIScales::SCALE_HIGH  : return "960x640";
        default                     : return "480x320";
      }
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
      switch (api_scales) {
        case APIScales::SCALE_LOW   : return "width=480&height=320";
        case APIScales::SCALE_MEDIUM: return "width=720&height=480";
        case APIScales::SCALE_HIGH  : return "width=960&height=640";
        default                    : return "width=480&height=320";
      }
    }

    void setStateStatus(Status new_state);
    const char* getStateStatus(bool expand = false) {
      switch (state_gen) {
        case Status::OK_INITIALIZATION_API:
          return (!expand) ? "..."                         : "инициализация API нейросети"              ;

        case Status::OK_PREPARING_DATA:
          return (!expand) ? "подготовка данных"           : "подготовка данных для запроса"            ;

        case Status::OK_READY_TO_SEND:
          return (!expand) ? "готово к отправке"           : "данные подготовлены, ожидание отправки"   ;

        case Status::OK_SENDING_ATTEMPT:
          snprintf(wrk_status, sz_wrk_status,
                   (!expand) ? "отправка запроса %2d/%2d"  : "попытка отправки запроса к нейросети",
                   attempt_network_count + 1, try_request);
          return wrk_status;

        case Status::OK_SENDING_REQUEST:
          return (!expand) ? "отправка запроса"            : "отправка запроса в нейросеть"             ;

        case Status::OK_WAITING_FOR_RESULT:
          return (!expand) ? "ожидание результата"         : "ожидание ответа от нейросети"             ;

        case Status::OK_GENERATING_READILY:
          return (!expand) ? "готово"                      : "генерация завершена успешно"              ;

        case Status::OK_TRANSLATE:
          return (!expand) ? "перевод промпта"             : "перевод промпта на английский язык"       ;

        case Status::OK_DOWNLOADING:
          return (!expand) ? "загрузка изображения"        : "загрузка сгенерированого изображения"     ;

        case Status::GET_API_POLLEN:
          return (!expand) ? "обновление баланса"          : "обновление остатка баланса"               ;

        case Status::GET_API_POLLEN_OK:
          return (!expand) ? "успешное обновление баланса" : "успешное обновление баланса"              ;

        case Status::GET_API_POLLEN_ERR:
          return (!expand) ? "ошибка обновления баланса"   : "ошибка обновления баланса"                ;

        case Status::GET_API_MODELS:
          return (!expand) ? "обновление моделей"          : "обновление доступных моделей"             ;

        case Status::GET_API_MODELS_OK:
          return (!expand) ? "успешное обновление моделей" : "успешное обновление моделей"              ;

        case Status::GET_API_MODELS_ERR:
          return (!expand) ? "ошибка обновления моделей"   : "ошибка обновления моделей"                ;

        case Status::ERROR_REQUESTS:
          return (!expand) ? "ошибка отправки запроса"     : "не удалось отправить запрос к нейросети"  ;

        case Status::ERROR_RESPONSE:
          return (!expand) ? "ошибка получения ответа"     : "нейросеть вернула некорректный ответ"     ;

        case Status::ERROR_AIGENERATION:
          return (!expand) ? "ошибка AI генерации"         : "нейросеть не смогла сгенерировать контент";

        case Status::ERROR_RECEIVING:
          return (!expand) ? "ошибка получения данных"     : "проблема при получении данных от сервера" ;

        case Status::ERROR_DECODINGS:
          return (!expand) ? "ошибка декодирования"        : "сбой при декодировании изображения"       ;

        case Status::ERROR_CONNECTION:
          return (!expand) ? "ошибка подключения"          : "нет подключения к интернету"              ;

        case Status::ERROR_INITMEMORY:
          return (!expand) ? "ошибка памяти PSRAM"         : "недостаточно памяти для генерации"        ;

        case Status::ERROR_OVERLOAD:
          return (!expand) ? "слишком частые запросы"      : "слишком частые запросы, подождите"        ;

        case Status::ERROR_AUTHENTICATE:
          return (!expand) ? "ошибка авторизации"          : "ошибка авторизации API, проверьте ключ"   ;

        case Status::ERROR_UNAVAILABLE:
          return (!expand) ? "серверы недоступны"          : "серверы генерации недоступны"             ;

        case Status::ERROR_CONVERT:
          return (!expand) ? "ошибка перевода"             : "ошибка при переводе промпта"              ;

        case Status::ERROR_CONVERT_LIMIT:
          return (!expand) ? "лимит переводов"             : "достигнут лимит доступных переводов"      ;

        default:
          return (!expand) ? "неизвестный статус"          : "неизвестна, попробуйте позже"             ;
      }
    }

    bool isGenerating() {
      return state_gen == Status::OK_PREPARING_DATA ||
             state_gen == Status::OK_READY_TO_SEND ||
             state_gen == Status::OK_SENDING_ATTEMPT ||
             state_gen == Status::OK_SENDING_REQUEST ||
             state_gen == Status::OK_WAITING_FOR_RESULT ||
             state_gen == Status::OK_TRANSLATE;
    }

    bool isRequestError() const {
      return state_gen == Status::ERROR_AIGENERATION ||
             state_gen == Status::ERROR_CONNECTION ||
             state_gen == Status::ERROR_UNAVAILABLE ||
             state_gen == Status::ERROR_REQUESTS;
    }

    bool isReceiveError() const {
      return state_gen == Status::ERROR_REQUESTS ||
             state_gen == Status::ERROR_RESPONSE ||
             state_gen == Status::ERROR_RECEIVING ||
             state_gen == Status::ERROR_DECODINGS ||
             state_gen == Status::ERROR_OVERLOAD ||
             state_gen == Status::ERROR_AUTHENTICATE ||
             state_gen == Status::ERROR_UNAVAILABLE;
    }

    int8_t isPollenState() {
      if (state_gen == Status::GET_API_POLLEN_OK) {
        return 1;
      } else if (state_gen == Status::GET_API_POLLEN_ERR) {
        return -1;
      } else {
        return 0;
      }
    }

    int8_t isModelsState() {
      if (state_gen == Status::GET_API_MODELS_OK) {
        return 1;
      } else if (state_gen == Status::GET_API_MODELS_ERR) {
        return -1;
      } else {
        return 0;
      }
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

    void tick(bool WiFiState);

    Status state_gen = Status::OK_INITIALIZATION_API;//статус генерации

    uint32_t created_image = 0;

    char*           jpegDataBuf = nullptr;
    const size_t sz_jpegDataBuf =  262144;
    size_t          jpegDataSum =  0;

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

    char* useragent = (char*)heap_caps_malloc(sz_useragent, MALLOC_CAP_SPIRAM);
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
      bool useheads  : 1;
      bool usepings  : 1;
      bool useolder  : 1;
      bool critical  : 1;
    } flags;

    struct {
      uint16_t request = 0;//Ошибки отправки запроса
      uint16_t receive = 0;//Ошибки получения ответа
      uint16_t decoder = 0;//Ошибки декодирования
    } error;

    uint32_t _timeoutMs = 0;
    uint32_t _triggerMs = 0;
    esp_task_wdt_config_t* _wdt_config = nullptr;

    uint32_t _str_generations = 0;
    uint32_t _end_generations = 0;

    RenderRunCallback _run_cb = nullptr;
    RenderEndCallback _end_cb = nullptr;
    RenderErrCallback _err_cb = nullptr;
    RenderEngCallback _eng_cb = nullptr;
    RenderUndCallback _und_cb = nullptr;

    IPAddress  _hostIP;
    char*    JsonBuffer;    const size_t sz_JsonBuffer  =  4096;

    // static
    static NEURGenerator* self;

    uTimer16<millis> neur_timer;

    // Функция для URL-кодирования (упрощенная версия)
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
