#include "NEURGenerator.h"

NEURGenerator::NEURGenerator() : _timer(5000) { // Таймер на 5 секунд
    _apiKey = nullptr;
    _balance = nullptr;
    _lastError = nullptr;
    _jsonBuffer = nullptr;
    _http = nullptr;
    _wifiConnected = false;
    _flags.usePsram = false;
    _flags.hasError = false;
    
    // Проверяем наличие PSRAM
    _flags.usePsram = (psramFound() && psramInit());
    
    // Выделяем буферы
    allocateBuffers();
}

void NEURGenerator::allocateBuffers() {
    size_t keySize = 128;
    size_t balanceSize = 32;
    size_t errorSize = 128;
    
    if (_flags.usePsram) {
        _apiKey = (char*)ps_malloc(keySize);
        _balance = (char*)ps_malloc(balanceSize);
        _lastError = (char*)ps_malloc(errorSize);
        _jsonBuffer = (char*)ps_malloc(JSON_BUFFER_SIZE);
    } else {
        _apiKey = (char*)malloc(keySize);
        _balance = (char*)malloc(balanceSize);
        _lastError = (char*)malloc(errorSize);
        _jsonBuffer = (char*)malloc(JSON_BUFFER_SIZE);
    }
    
    // Инициализируем пустыми строками
    if (_apiKey) _apiKey[0] = '\0';
    if (_balance) {
        _balance[0] = '0';
        _balance[1] = '\0';
    }
    if (_lastError) _lastError[0] = '\0';
    if (_jsonBuffer) _jsonBuffer[0] = '\0';
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

void NEURGenerator::cleanupHttp() {
    if (_http) {
        _http->stop();
        delete _http;
        _http = nullptr;
    }
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

void NEURGenerator::tick() {
    // Для будущих обновлений с таймерами
    _timer.tick();
}
