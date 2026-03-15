#include "NEURGenerator.h"

NEURGenerator::NEURGenerator() : _timer(5000) {
    _apiKey = nullptr;
    _balance = nullptr;
    _lastError = nullptr;
    _jsonBuffer = nullptr;
    _lastImageUrl = nullptr;
    _model = nullptr;
    _quality = nullptr;
    
    // Буферы для промптов
    _suffix = nullptr;
    _modifier = nullptr;
    _denial = nullptr;
    _preparedPrompt = nullptr;
    _translatedText = nullptr;
    _myMemoryEmail = nullptr;
    
    _jpegData = nullptr;
    _jpegDataSize = 0;
    _jpegDataCapacity = JPEG_BUFFER_SIZE;
    
    _http = nullptr;
    
    _wifiConnected = false;
    _translateEnabled = false;
    _lastHttpCode = 0;
    _width = 512;
    _height = 512;
    
    _flags.usePsram = false;
    _flags.hasError = false;
    
    // Проверяем наличие PSRAM
    _flags.usePsram = (psramFound() && psramInit());
    
    // Выделяем буферы
    allocateBuffers();
    
    // Устанавливаем модель по умолчанию
    if (_model) {
        strcpy(_model, "flux");
    }
    
    // Устанавливаем качество по умолчанию
    if (_quality) {
        strcpy(_quality, "medium");
    }
}

void NEURGenerator::allocateBuffers() {
    size_t keySize = 128;
    size_t balanceSize = 32;
    size_t errorSize = 128;
    size_t urlSize = URL_BUFFER_SIZE;
    size_t modelSize = 32;
    size_t qualitySize = 16;
    size_t promptSize = PROMPT_BUFFER_SIZE;
    size_t emailSize = 64;
    
    if (_flags.usePsram) {
        _apiKey = (char*)ps_malloc(keySize);
        _balance = (char*)ps_malloc(balanceSize);
        _lastError = (char*)ps_malloc(errorSize);
        _jsonBuffer = (char*)ps_malloc(JSON_BUFFER_SIZE);
        _lastImageUrl = (char*)ps_malloc(urlSize);
        _model = (char*)ps_malloc(modelSize);
        _quality = (char*)ps_malloc(qualitySize);
        
        // Буферы для промптов
        _suffix = (char*)ps_malloc(promptSize);
        _modifier = (char*)ps_malloc(promptSize);
        _denial = (char*)ps_malloc(promptSize);
        _preparedPrompt = (char*)ps_malloc(promptSize * 2);
        _translatedText = (char*)ps_malloc(promptSize);
        _myMemoryEmail = (char*)ps_malloc(emailSize);
        
        _jpegData = (uint8_t*)ps_malloc(_jpegDataCapacity);
    } else {
        _apiKey = (char*)malloc(keySize);
        _balance = (char*)malloc(balanceSize);
        _lastError = (char*)malloc(errorSize);
        _jsonBuffer = (char*)malloc(JSON_BUFFER_SIZE);
        _lastImageUrl = (char*)malloc(urlSize);
        _model = (char*)malloc(modelSize);
        _quality = (char*)malloc(qualitySize);
        
        // Буферы для промптов
        _suffix = (char*)malloc(promptSize);
        _modifier = (char*)malloc(promptSize);
        _denial = (char*)malloc(promptSize);
        _preparedPrompt = (char*)malloc(promptSize * 2);
        _translatedText = (char*)malloc(promptSize);
        _myMemoryEmail = (char*)malloc(emailSize);
        
        _jpegData = (uint8_t*)malloc(_jpegDataCapacity);
    }
    
    // Инициализируем пустыми строками
    if (_apiKey) _apiKey[0] = '\0';
    if (_balance) {
        _balance[0] = '0';
        _balance[1] = '\0';
    }
    if (_lastError) _lastError[0] = '\0';
    if (_jsonBuffer) _jsonBuffer[0] = '\0';
    if (_lastImageUrl) _lastImageUrl[0] = '\0';
    if (_model) _model[0] = '\0';
    if (_quality) _quality[0] = '\0';
    
    // Буферы для промптов
    if (_suffix) _suffix[0] = '\0';
    if (_modifier) _modifier[0] = '\0';
    if (_denial) _denial[0] = '\0';
    if (_preparedPrompt) _preparedPrompt[0] = '\0';
    if (_translatedText) _translatedText[0] = '\0';
    if (_myMemoryEmail) _myMemoryEmail[0] = '\0';
    
    // Очищаем JPEG буфер
    clearJpegBuffer();
}

void NEURGenerator::clearJpegBuffer() {
    if (_jpegData) {
        memset(_jpegData, 0, _jpegDataCapacity);
    }
    _jpegDataSize = 0;
}

void NEURGenerator::resetWDT() {
    esp_task_wdt_reset();
}

// ========== МЕТОДЫ ДЛЯ ПРОМПТОВ ==========

void NEURGenerator::setSuffix(const char* suffix) {
    if (_suffix && suffix) {
        strncpy(_suffix, suffix, PROMPT_BUFFER_SIZE - 1);
        _suffix[PROMPT_BUFFER_SIZE - 1] = '\0';
    }
}

void NEURGenerator::setModifier(const char* modifier) {
    if (_modifier && modifier) {
        strncpy(_modifier, modifier, PROMPT_BUFFER_SIZE - 1);
        _modifier[PROMPT_BUFFER_SIZE - 1] = '\0';
    }
}

void NEURGenerator::setDenial(const char* denial) {
    if (_denial && denial) {
        strncpy(_denial, denial, PROMPT_BUFFER_SIZE - 1);
        _denial[PROMPT_BUFFER_SIZE - 1] = '\0';
    }
}

void NEURGenerator::setTranslate(bool enable) {
    _translateEnabled = enable;
}

void NEURGenerator::setMyMemoryEmail(const char* email) {
    if (_myMemoryEmail && email) {
        strncpy(_myMemoryEmail, email, 63);
        _myMemoryEmail[63] = '\0';
    }
}

bool NEURGenerator::isRussianText(const char* text) {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    
    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = text[i];
        // Русские буквы в UTF-8 (приблизительно)
        if (c >= 0xC0 && c <= 0xFF) {
            return true;
        }
    }
    return false;
}

bool NEURGenerator::isRussian(const char* text) {
    return isRussianText(text);
}

bool NEURGenerator::isEnglishText(const char* text) {
    if (!text || strlen(text) == 0) {
        return false;
    }
    
    size_t len = strlen(text);
    if (len < 2 || len > 1000) {
        return false;
    }
    
    int letterCount = 0;
    int validCharCount = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            letterCount++;
            validCharCount++;
        }
        else if (c >= '0' && c <= '9') {
            validCharCount++;
        }
        else if (c == ' ' || c == ',' || c == '.' || c == '!' || c == '?' ||
                 c == '-' || c == '_' || c == '(' || c == ')' || c == ':' ||
                 c == ';' || c == '\'' || c == '"') {
            validCharCount++;
        }
    }
    
    float letterRatio = (float)letterCount / len;
    float validRatio = (float)validCharCount / len;
    
    return (letterRatio >= 0.3f && validRatio >= 0.8f);
}

bool NEURGenerator::requestTranslation(const char* text) {
    if (!_wifiConnected) {
        if (_lastError) snprintf(_lastError, 128, "Нет подключения к WiFi");
        return false;
    }
    
    cleanupHttp();
    
    // Кодируем текст для URL
    char encodedText[1024];
    urlEncode(text, encodedText, sizeof(encodedText));
    
    // Формируем URL
    char url[2048];
    if (_myMemoryEmail && _myMemoryEmail[0] != '\0') {
        snprintf(url, sizeof(url), "/get?q=%s&langpair=ru|en&de=%s",
                 encodedText, _myMemoryEmail);
    } else {
        snprintf(url, sizeof(url), "/get?q=%s&langpair=ru|en", encodedText);
    }
    
    Serial.print("🔗 URL перевода: ");
    Serial.println(url);
    
    // Создаем HTTPS клиент
    _http = new ghttp::Client(TRANS_HOST, TRANS_PORT);
    if (!_http) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка создания HTTP клиента");
        return false;
    }
    
    _http->setInsecure();
    _http->setTimeout(10000);
    
    if (!_http->connect()) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к серверу перевода");
        cleanupHttp();
        return false;
    }
    
    // Формируем заголовки
    ghttp::Client::Headers headers;
    headers.add("Accept", "*/*");
    headers.add("User-Agent", "NEURGenerator/1.2.0");
    
    if (!_http->request(url, "GET", headers)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка отправки запроса перевода");
        cleanupHttp();
        return false;
    }
    
    ghttp::Client::Response resp = _http->getResponse();
    if (!resp) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка получения ответа перевода");
        cleanupHttp();
        return false;
    }
    
    int httpCode = resp.code();
    if (httpCode != 200) {
        if (_lastError) snprintf(_lastError, 128, "HTTP ошибка перевода: %d", httpCode);
        cleanupHttp();
        return false;
    }
    
    // Читаем ответ
    size_t bytesRead = resp.body().readBytes(_jsonBuffer, JSON_BUFFER_SIZE - 1);
    _jsonBuffer[bytesRead] = '\0';
    
    // Парсим JSON
    gson::Parser json;
    if (!json.parse(_jsonBuffer)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка парсинга JSON перевода");
        cleanupHttp();
        return false;
    }
    
    // Извлекаем перевод
    const char* translated = json["responseData"]["translatedText"].c_str();
    if (translated && translated[0] != '\0' && isEnglishText(translated)) {
        if (_translatedText) {
            strncpy(_translatedText, translated, PROMPT_BUFFER_SIZE - 1);
            _translatedText[PROMPT_BUFFER_SIZE - 1] = '\0';
        }
        cleanupHttp();
        return true;
    }
    
    if (_lastError) snprintf(_lastError, 128, "Не удалось извлечь перевод");
    cleanupHttp();
    return false;
}

bool NEURGenerator::translateText(const char* russian_text) {
    _flags.hasError = false;
    
    if (!russian_text || russian_text[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Текст для перевода пуст");
        return false;
    }
    
    if (!isRussianText(russian_text)) {
        // Если текст не русский, просто копируем его
        if (_translatedText) {
            strncpy(_translatedText, russian_text, PROMPT_BUFFER_SIZE - 1);
            _translatedText[PROMPT_BUFFER_SIZE - 1] = '\0';
        }
        return true;
    }
    
    _timer.reset();
    
    int attempts = 0;
    const int maxAttempts = 3;
    
    while (attempts < maxAttempts) {
        if (requestTranslation(russian_text)) {
            Serial.print("✅ Перевод: ");
            Serial.println(_translatedText);
            return true;
        }
        
        attempts++;
        if (attempts < maxAttempts) {
            delay(2000);
        }
    }
    
    _flags.hasError = true;
    return false;
}

const char* NEURGenerator::getTranslatedText() {
    return _translatedText ? _translatedText : "";
}

bool NEURGenerator::preparePrompt(const char* prompt) {
    if (!prompt || prompt[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Промпт не может быть пустым");
        return false;
    }
    
    if (!_preparedPrompt) return false;
    
    _preparedPrompt[0] = '\0';
    
    // 1. Основной промпт (с переводом если нужно)
    const char* mainPrompt = prompt;
    
    if (_translateEnabled && isRussianText(prompt)) {
        if (translateText(prompt)) {
            mainPrompt = _translatedText;
            Serial.println("✅ Использован переведенный промпт");
        } else {
            Serial.println("⚠️ Использован оригинальный промпт (перевод не удался)");
        }
    }
    
    strcpy(_preparedPrompt, mainPrompt);
    
    // 2. Добавляем суффикс
    if (_suffix && _suffix[0] != '\0') {
        if (strlen(_preparedPrompt) > 0) strcat(_preparedPrompt, ", ");
        strcat(_preparedPrompt, _suffix);
    }
    
    // 3. Добавляем модификатор
    if (_modifier && _modifier[0] != '\0') {
        if (strlen(_preparedPrompt) > 0) strcat(_preparedPrompt, ", ");
        strcat(_preparedPrompt, _modifier);
    }
    
    Serial.print("📝 Подготовленный промпт: ");
    Serial.println(_preparedPrompt);
    
    return true;
}

const char* NEURGenerator::getPreparedPrompt() {
    return _preparedPrompt ? _preparedPrompt : "";
}

const char* NEURGenerator::getDenial() {
    return _denial ? _denial : "";
}

// ========== МЕТОДЫ ДЛЯ ГЕНЕРАЦИИ ==========

void NEURGenerator::setModel(const char* model) {
    if (_model && model) {
        strncpy(_model, model, 31);
        _model[31] = '\0';
        Serial.printf("✅ Модель установлена: %s\n", _model);
    }
}

void NEURGenerator::setSize(uint16_t width, uint16_t height) {
    _width = width;
    _height = height;
    Serial.printf("✅ Размер установлен: %dx%d\n", _width, _height);
}

void NEURGenerator::setQuality(const char* quality) {
    if (_quality && quality) {
        strncpy(_quality, quality, 15);
        _quality[15] = '\0';
        Serial.printf("✅ Качество установлено: %s\n", _quality);
    }
}

void NEURGenerator::urlEncode(const char* src, char* dest, size_t destSize) {
    const char* hex = "0123456789ABCDEF";
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] != '\0' && j < destSize - 1) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            dest[j++] = c;
        } else if (c == ' ') {
            if (j + 3 < destSize - 1) {
                dest[j++] = '%';
                dest[j++] = '2';
                dest[j++] = '0';
            }
        } else {
            if (j + 3 < destSize - 1) {
                dest[j++] = '%';
                dest[j++] = hex[(c >> 4) & 0xF];
                dest[j++] = hex[c & 0xF];
            }
        }
        i++;
    }
    dest[j] = '\0';
}

bool NEURGenerator::requestGeneration(const char* prompt, const char* negativePrompt) {
    if (!_wifiConnected) {
        if (_lastError) snprintf(_lastError, 128, "Нет подключения к WiFi");
        return false;
    }
    
    if (!_apiKey || _apiKey[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Не установлен API ключ");
        return false;
    }
    
    if (!prompt || prompt[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Промпт не может быть пустым");
        return false;
    }
    
    cleanupHttp();
    clearJpegBuffer();
    
    // Кодируем промпт для URL
    char encodedPrompt[1024];
    urlEncode(prompt, encodedPrompt, sizeof(encodedPrompt));
    
    // Формируем URL
    char url[2048];
    snprintf(url, sizeof(url), 
             "/image/%s?model=%s&width=%d&height=%d&quality=%s&nologo=true",
             encodedPrompt, _model, _width, _height, _quality);
    
    // Добавляем private если есть ключ
    if (_apiKey && _apiKey[0] != '\0') {
        size_t len = strlen(url);
        snprintf(url + len, sizeof(url) - len, "&private=true");
    }
    
    // Добавляем negative prompt если есть
    if (negativePrompt && negativePrompt[0] != '\0') {
        char encodedNegative[512];
        urlEncode(negativePrompt, encodedNegative, sizeof(encodedNegative));
        
        size_t len = strlen(url);
        snprintf(url + len, sizeof(url) - len, "&negative_prompt=%s", encodedNegative);
    }
    
    Serial.print("🔗 URL запроса: ");
    Serial.println(url);
    
    // Создаем HTTPS клиент
    _http = new ghttp::Client(HOST, PORT);
    if (!_http) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка создания HTTP клиента");
        return false;
    }
    
    // Настраиваем SSL
    _http->setInsecure();
    _http->setTimeout(30000);
    
    if (!_http->connect()) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к серверу");
        cleanupHttp();
        return false;
    }
    
    // Формируем заголовки
    ghttp::Client::Headers headers;
    headers.add("Authorization", _apiKey);
    headers.add("Accept", "image/jpeg");
    headers.add("User-Agent", "NEURGenerator/1.2.0");
    
    Serial.println("📡 Отправка запроса на генерацию...");
    
    if (!_http->request(url, "GET", headers)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка отправки запроса");
        cleanupHttp();
        return false;
    }
    
    ghttp::Client::Response resp = _http->getResponse();
    if (!resp) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка получения ответа");
        cleanupHttp();
        return false;
    }
    
    _lastHttpCode = resp.code();
    Serial.printf("HTTP код: %d\n", _lastHttpCode);
    
    if (_lastHttpCode != 200) {
        if (_lastError) snprintf(_lastError, 128, "HTTP ошибка: %d", _lastHttpCode);
        
        // Читаем тело ошибки если есть
        size_t bytesRead = resp.body().readBytes(_jsonBuffer, JSON_BUFFER_SIZE - 1);
        if (bytesRead > 0) {
            _jsonBuffer[bytesRead] = '\0';
            Serial.print("Ответ сервера: ");
            Serial.println(_jsonBuffer);
        }
        
        cleanupHttp();
        return false;
    }
    
    // Читаем данные изображения
    size_t totalBytesRead = 0;
    uint8_t buffer[512];
    
    while (resp.body().available() > 0 && totalBytesRead < _jpegDataCapacity) {
        size_t toRead = min(sizeof(buffer), _jpegDataCapacity - totalBytesRead);
        size_t bytesRead = resp.body().readBytes((char*)buffer, toRead);
        
        if (bytesRead == 0) break;
        
        if (_jpegData) {
            memcpy(_jpegData + totalBytesRead, buffer, bytesRead);
        }
        totalBytesRead += bytesRead;
        
        resetWDT(); // Сбрасываем WDT во время загрузки
    }
    
    _jpegDataSize = totalBytesRead;
    
    // Сохраняем URL изображения
    if (_lastImageUrl) {
        snprintf(_lastImageUrl, URL_BUFFER_SIZE - 1, 
                 "https://%s%s", HOST, url);
    }
    
    Serial.printf("✅ Получено %d байт JPEG\n", _jpegDataSize);
    
    cleanupHttp();
    return _jpegDataSize > 0;
}

bool NEURGenerator::generate() {
    if (!_preparedPrompt || _preparedPrompt[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Промпт не подготовлен");
        return false;
    }
    
    return generate(_preparedPrompt, _denial);
}

bool NEURGenerator::generate(const char* prompt) {
    return generate(prompt, _denial);
}

bool NEURGenerator::generate(const char* prompt, const char* negativePrompt) {
    _flags.hasError = false;
    
    _timer.reset();
    
    int attempts = 0;
    const int maxAttempts = 3;
    
    while (attempts < maxAttempts) {
        if (requestGeneration(prompt, negativePrompt)) {
            return true;
        }
        
        attempts++;
        if (attempts < maxAttempts) {
            Serial.printf("Повторная попытка %d/%d через 2 сек...\n", attempts + 1, maxAttempts);
            delay(2000);
        }
    }
    
    _flags.hasError = true;
    return false;
}

// ========== МЕТОДЫ ДЛЯ ПОЛУЧЕНИЯ ДАННЫХ ==========

const char* NEURGenerator::getLastImageUrl() {
    return _lastImageUrl ? _lastImageUrl : "";
}

uint8_t* NEURGenerator::getImageData() {
    return _jpegData;
}

size_t NEURGenerator::getImageDataSize() {
    return _jpegDataSize;
}

bool NEURGenerator::hasImageData() {
    return _jpegData != nullptr && _jpegDataSize > 0;
}

void NEURGenerator::clearImageData() {
    clearJpegBuffer();
}

// ========== МЕТОДЫ ДЛЯ БАЛАНСА ==========

bool NEURGenerator::requestBalance() {
    if (!_wifiConnected) {
        if (_lastError) snprintf(_lastError, 128, "Нет подключения к WiFi");
        return false;
    }
    
    if (!_apiKey || _apiKey[0] == '\0') {
        if (_lastError) snprintf(_lastError, 128, "Не установлен API ключ");
        return false;
    }
    
    cleanupHttp();
    
    _http = new ghttp::Client(HOST, PORT);
    if (!_http) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка создания HTTP клиента");
        return false;
    }
    
    _http->setInsecure();
    _http->setTimeout(10000);
    
    if (!_http->connect()) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к серверу");
        cleanupHttp();
        return false;
    }
    
    ghttp::Client::Headers headers;
    headers.add("Authorization", _apiKey);
    headers.add("Accept", "application/json");
    headers.add("User-Agent", "NEURGenerator/1.2.0");
    
    Serial.println("📡 Запрос баланса...");
    
    if (!_http->request("/account/balance", "GET", headers)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка отправки запроса");
        cleanupHttp();
        return false;
    }
    
    ghttp::Client::Response resp = _http->getResponse();
    if (!resp) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка получения ответа");
        cleanupHttp();
        return false;
    }
    
    int httpCode = resp.code();
    Serial.printf("HTTP код: %d\n", httpCode);
    
    if (httpCode != 200) {
        if (_lastError) snprintf(_lastError, 128, "HTTP ошибка: %d", httpCode);
        cleanupHttp();
        return false;
    }
    
    size_t bytesRead = resp.body().readBytes(_jsonBuffer, JSON_BUFFER_SIZE - 1);
    _jsonBuffer[bytesRead] = '\0';
    
    Serial.print("Ответ: ");
    Serial.println(_jsonBuffer);
    
    gson::Parser json;
    if (!json.parse(_jsonBuffer)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка парсинга JSON");
        cleanupHttp();
        return false;
    }
    
    const char* balance = json["balance"].c_str();
    if (balance && balance[0] != '\0') {
        if (_balance) {
            strncpy(_balance, balance, 31);
            _balance[31] = '\0';
        }
        cleanupHttp();
        return true;
    } else {
        if (_lastError) snprintf(_lastError, 128, "Поле balance не найдено");
        cleanupHttp();
        return false;
    }
}

bool NEURGenerator::checkBalance() {
    _flags.hasError = false;
    
    _timer.reset();
    
    int attempts = 0;
    const int maxAttempts = 3;
    
    while (attempts < maxAttempts) {
        if (requestBalance()) {
            Serial.print("💰 Баланс: ");
            Serial.print(_balance);
            Serial.println(" pollen");
            return true;
        }
        
        attempts++;
        if (attempts < maxAttempts) {
            Serial.printf("Повторная попытка %d/%d через 2 сек...\n", attempts + 1, maxAttempts);
            delay(2000);
        }
    }
    
    _flags.hasError = true;
    return false;
}

const char* NEURGenerator::getBalance() {
    return _balance ? _balance : "0";
}

// ========== СЛУЖЕБНЫЕ МЕТОДЫ ==========

void NEURGenerator::connectWiFi(const char* ssid, const char* password) {
    Serial.print("Подключение к WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
        resetWDT();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\n✅ WiFi подключен");
        Serial.print("IP адрес: ");
        Serial.println(WiFi.localIP());
        
        IPAddress hostIp;
        if (WiFi.hostByName(HOST, hostIp)) {
            if (Ping.ping(hostIp, 1)) {
                Serial.println("✅ Хост доступен");
            } else {
                Serial.println("⚠️ Хост не отвечает на ping");
            }
        }
    } else {
        _wifiConnected = false;
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к WiFi");
        Serial.println("\n❌ Ошибка WiFi");
    }
}

void NEURGenerator::setApiKey(const char* key) {
    if (_apiKey) {
        snprintf(_apiKey, 128, "Bearer %s", key);
        Serial.println("✅ API ключ установлен");
    }
}

bool NEURGenerator::isWiFiConnected() {
    return _wifiConnected;
}

const char* NEURGenerator::getLastError() {
    return _lastError ? _lastError : "";
}

int NEURGenerator::getLastHttpCode() {
    return _lastHttpCode;
}

void NEURGenerator::cleanupHttp() {
    if (_http) {
        _http->stop();
        delete _http;
        _http = nullptr;
    }
}

void NEURGenerator::tick() {
    _timer.tick();
}
