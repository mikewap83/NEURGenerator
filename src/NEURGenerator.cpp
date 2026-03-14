#include "NEURGenerator.h"

NEURGenerator::NEURGenerator() : _timer(5000) {
    _apiKey = nullptr;
    _balance = nullptr;
    _lastError = nullptr;
    _jsonBuffer = nullptr;
    _lastImageUrl = nullptr;
    _model = nullptr;
    _quality = nullptr;
    _http = nullptr;
    
    _wifiConnected = false;
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
    
    if (_flags.usePsram) {
        _apiKey = (char*)ps_malloc(keySize);
        _balance = (char*)ps_malloc(balanceSize);
        _lastError = (char*)ps_malloc(errorSize);
        _jsonBuffer = (char*)ps_malloc(JSON_BUFFER_SIZE);
        _lastImageUrl = (char*)ps_malloc(urlSize);
        _model = (char*)ps_malloc(modelSize);
        _quality = (char*)ps_malloc(qualitySize);
    } else {
        _apiKey = (char*)malloc(keySize);
        _balance = (char*)malloc(balanceSize);
        _lastError = (char*)malloc(errorSize);
        _jsonBuffer = (char*)malloc(JSON_BUFFER_SIZE);
        _lastImageUrl = (char*)malloc(urlSize);
        _model = (char*)malloc(modelSize);
        _quality = (char*)malloc(qualitySize);
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
}

void NEURGenerator::connectWiFi(const char* ssid, const char* password) {
    Serial.print("Подключение к WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("\n✅ WiFi подключен");
        Serial.print("IP адрес: ");
        Serial.println(WiFi.localIP());
        
        // Пингуем хост для проверки соединения
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

// ========== НОВЫЕ МЕТОДЫ ==========

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
    
    // Кодируем промпт для URL
    char encodedPrompt[1024];
    urlEncode(prompt, encodedPrompt, sizeof(encodedPrompt));
    
    // Формируем URL
    char url[2048];
    snprintf(url, sizeof(url), 
             "/image/%s?model=%s&width=%d&height=%d&quality=%s&nologo=true",
             encodedPrompt, _model, _width, _height, _quality);
    
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
    _http->setTimeout(30000); // Увеличиваем таймаут для генерации
    
    if (!_http->connect()) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к серверу");
        cleanupHttp();
        return false;
    }
    
    // Формируем заголовки
    ghttp::Client::Headers headers;
    headers.add("Authorization", _apiKey);
    headers.add("Accept", "image/jpeg");
    headers.add("User-Agent", "NEURGenerator/1.1.0");
    
    Serial.println("📡 Отправка запроса на генерацию...");
    
    // Отправляем GET запрос
    if (!_http->request(url, "GET", headers)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка отправки запроса");
        cleanupHttp();
        return false;
    }
    
    // Получаем ответ
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
    
    // Сохраняем URL изображения
    if (_lastImageUrl) {
        snprintf(_lastImageUrl, URL_BUFFER_SIZE - 1, 
                 "https://%s%s", HOST, url);
    }
    
    Serial.println("✅ Запрос на генерацию успешно отправлен");
    Serial.print("🖼️ URL изображения: ");
    Serial.println(_lastImageUrl);
    
    cleanupHttp();
    return true;
}

bool NEURGenerator::generateImage(const char* prompt) {
    _flags.hasError = false;
    
    // Используем таймер для повторных попыток
    _timer.reset();
    
    int attempts = 0;
    const int maxAttempts = 3;
    
    while (attempts < maxAttempts) {
        if (requestGeneration(prompt)) {
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

bool NEURGenerator::generateImage(const char* prompt, const char* negativePrompt) {
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

const char* NEURGenerator::getLastImageUrl() {
    return _lastImageUrl ? _lastImageUrl : "";
}

int NEURGenerator::getLastHttpCode() {
    return _lastHttpCode;
}

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
    
    // Создаем HTTPS клиент
    _http = new ghttp::Client(HOST, PORT);
    if (!_http) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка создания HTTP клиента");
        return false;
    }
    
    // Настраиваем SSL (insecure для теста)
    _http->setInsecure();
    _http->setTimeout(10000);
    
    if (!_http->connect()) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка подключения к серверу");
        cleanupHttp();
        return false;
    }
    
    // Формируем заголовки
    ghttp::Client::Headers headers;
    headers.add("Authorization", _apiKey);
    headers.add("Accept", "application/json");
    headers.add("User-Agent", "NEURGenerator/1.0.1");
    
    Serial.println("📡 Запрос баланса...");
    
    // Отправляем GET запрос
    if (!_http->request("/account/balance", "GET", headers)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка отправки запроса");
        cleanupHttp();
        return false;
    }
    
    // Получаем ответ
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
    
    // Читаем тело ответа
    size_t bytesRead = resp.body().readBytes(_jsonBuffer, JSON_BUFFER_SIZE - 1);
    _jsonBuffer[bytesRead] = '\0';
    
    Serial.print("Ответ: ");
    Serial.println(_jsonBuffer);
    
    // Парсим JSON с помощью GSON
    gson::Parser json;
    if (!json.parse(_jsonBuffer)) {
        if (_lastError) snprintf(_lastError, 128, "Ошибка парсинга JSON");
        cleanupHttp();
        return false;
    }
    
    // Извлекаем баланс
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
    
    // Используем таймер для повторных попыток
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

bool NEURGenerator::isWiFiConnected() {
    return _wifiConnected;
}

const char* NEURGenerator::getLastError() {
    return _lastError ? _lastError : "";
}

void NEURGenerator::cleanupHttp() {
    if (_http) {
        _http->stop();
        delete _http;
        _http = nullptr;
    }
}

void NEURGenerator::tick() {
    // Для будущих обновлений с таймерами
    _timer.tick();
}
