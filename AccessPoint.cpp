#include "AccessPoint.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

namespace {
const uint8_t START_NAME_LENGTH = 8;
const uint16_t DNS_PORT = 53;
const unsigned long STA_CONNECTION_TIMEOUT_MS = 30000UL;
const unsigned long PORTAL_SHUTDOWN_DELAY_MS = 500UL;
const char ALPHANUMERIC[] =
    "abcdefghijklmnopqrstuvwxyz0123456789";

String generateStartName() {
  String startName;
  startName.reserve(START_NAME_LENGTH);

  for (uint8_t i = 0; i < START_NAME_LENGTH; i++) {
    startName += ALPHANUMERIC[esp_random() % (sizeof(ALPHANUMERIC) - 1)];
  }

  return startName;
}

bool isValidStartName(const String& startName) {
  if (startName.length() != START_NAME_LENGTH) {
    return false;
  }

  for (uint8_t i = 0; i < startName.length(); i++) {
    bool isAllowed = false;

    for (size_t j = 0; j < sizeof(ALPHANUMERIC) - 1; j++) {
      if (startName[i] == ALPHANUMERIC[j]) {
        isAllowed = true;
        break;
      }
    }

    if (!isAllowed) {
      return false;
    }
  }

  return true;
}

bool loadConfiguration(String& startName,
                       String& staSsid,
                       String& staPassword) {
  Preferences preferences;

  if (!preferences.begin("nivel_agua", false)) {
    return false;
  }

  startName = preferences.getString("start_name", "");

  if (!isValidStartName(startName)) {
    startName = generateStartName();

    if (preferences.putString("start_name", startName) != startName.length()) {
      preferences.end();
      return false;
    }
  }

  staSsid = preferences.getString("sta_ssid", "");
  staPassword = preferences.getString("sta_password", "");
  preferences.end();
  return true;
}

bool isJsonStringEscape(char value) {
  return value == '"' || value == '\\' || value == '/' || value == 'b' ||
         value == 'f' || value == 'n' || value == 'r' || value == 't';
}

bool extractJsonString(const String& json,
                       const char* key,
                       String& value) {
  String marker = "\"";
  marker += key;
  marker += "\"";

  const int keyPosition = json.indexOf(marker);
  if (keyPosition < 0) {
    return false;
  }

  const int colonPosition = json.indexOf(':', keyPosition + marker.length());
  if (colonPosition < 0) {
    return false;
  }

  const int openingQuote = json.indexOf('"', colonPosition + 1);
  if (openingQuote < 0) {
    return false;
  }

  value = "";

  for (int i = openingQuote + 1; i < json.length(); i++) {
    const char current = json[i];

    if (current == '"') {
      return true;
    }

    if (current != '\\') {
      value += current;
      continue;
    }

    if (i + 1 >= json.length() || !isJsonStringEscape(json[i + 1])) {
      return false;
    }

    const char escaped = json[++i];
    switch (escaped) {
      case '"':
      case '\\':
      case '/':
        value += escaped;
        break;
      case 'b':
        value += '\b';
        break;
      case 'f':
        value += '\f';
        break;
      case 'n':
        value += '\n';
        break;
      case 'r':
        value += '\r';
        break;
      case 't':
        value += '\t';
        break;
    }
  }

  return false;
}

void appendJsonString(String& json, const String& value) {
  json += '"';

  for (size_t i = 0; i < value.length(); i++) {
    const char current = value[i];

    switch (current) {
      case '"':
        json += "\\\"";
        break;
      case '\\':
        json += "\\\\";
        break;
      case '\b':
        json += "\\b";
        break;
      case '\f':
        json += "\\f";
        break;
      case '\n':
        json += "\\n";
        break;
      case '\r':
        json += "\\r";
        break;
      case '\t':
        json += "\\t";
        break;
      default:
        json += current;
        break;
    }
  }

  json += '"';
}
}  // namespace

struct AccessPoint::Impl {
  DNSServer dnsServer;
  WebServer server;
  String startName;
  String apSsid;
  String apPassword;
  String staSsid;
  String staPassword;
  bool apStarted;
  bool serverStarted;
  bool filesystemStarted;
  bool shutdownPending;
  bool initialized;
  unsigned long shutdownAt;

  Impl()
      : server(80),
        apStarted(false),
        serverStarted(false),
        filesystemStarted(false),
        shutdownPending(false),
        initialized(false),
        shutdownAt(0) {}
};

AccessPoint::AccessPoint() : _impl(new Impl()) {}

AccessPoint::~AccessPoint() {
  stopPortal();
  delete _impl;
}

bool AccessPoint::begin() {
  if (_impl == nullptr) {
    Serial.println("Falha ao alocar a rede AP");
    return false;
  }

  if (_impl->initialized) {
    return true;
  }

  if (!loadConfiguration(_impl->startName,
                         _impl->staSsid,
                         _impl->staPassword)) {
    Serial.println("Falha ao acessar o armazenamento persistente");
    return false;
  }

  _impl->apSsid = "distancia-" + _impl->startName;
  _impl->apPassword = _impl->startName;

  if (_impl->staSsid.length() > 0) {
    Serial.print("Tentando conectar em: ");
    Serial.println(_impl->staSsid);

    if (connectSta()) {
      Serial.println("STA conectado");
      Serial.print("IP STA: ");
      Serial.println(WiFi.localIP());
      _impl->initialized = true;
      return true;
    }

    Serial.println("Falha na conexão STA; iniciando access point");
  }

  if (!startPortal()) {
    return false;
  }

  _impl->initialized = true;
  return true;
}

bool AccessPoint::connectSta() {
  WiFi.mode(_impl->apStarted ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(_impl->staSsid.c_str(), _impl->staPassword.c_str());

  const unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < STA_CONNECTION_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.disconnect(false, false);
  return false;
}

bool AccessPoint::saveStaCredentials() {
  Preferences preferences;

  if (!preferences.begin("nivel_agua", false)) {
    return false;
  }

  const size_t savedSsid =
      preferences.putString("sta_ssid", _impl->staSsid);
  const size_t savedPassword =
      preferences.putString("sta_password", _impl->staPassword);

  preferences.end();

  return savedSsid == _impl->staSsid.length() &&
         savedPassword == _impl->staPassword.length();
}

bool AccessPoint::startPortal() {
  if (!LittleFS.begin(false)) {
    Serial.println("Falha ao montar o LittleFS");
    return false;
  }

  _impl->filesystemStarted = true;
  WiFi.mode(WIFI_AP_STA);

  if (!WiFi.softAP(_impl->apSsid.c_str(), _impl->apPassword.c_str())) {
    Serial.println("Falha ao iniciar o access point");
    LittleFS.end();
    _impl->filesystemStarted = false;
    return false;
  }

  const IPAddress apIP = WiFi.softAPIP();

  _impl->dnsServer.start(DNS_PORT, "*", apIP);

  _impl->server.on("/", HTTP_GET, [this]() {
    serveFile("/index.html", "text/html");
  });

  _impl->server.on("/style.css", HTTP_GET, [this]() {
    serveFile("/style.css", "text/css");
  });

  _impl->server.on("/app.js", HTTP_GET, [this]() {
    serveFile("/app.js", "application/javascript");
  });

  _impl->server.on("/api/networks", HTTP_GET, [this]() {
    handleNetworks();
  });

  _impl->server.on("/api/connect", HTTP_POST, [this]() {
    handleConnect();
  });

  _impl->server.onNotFound([this]() {
    if (_impl->server.uri().startsWith("/api/")) {
      _impl->server.send(
          404,
          "application/json",
          "{\"error\":\"not_found\"}");
      return;
    }

    _impl->server.sendHeader("Location", "/", true);
    _impl->server.send(302, "text/plain", "");
  });

  _impl->server.begin();
  _impl->apStarted = true;
  _impl->serverStarted = true;

  Serial.println("Access point iniciado");
  Serial.print("start_name: ");
  Serial.println(_impl->startName);
  Serial.print("SSID: ");
  Serial.println(_impl->apSsid);
  Serial.print("Senha: ");
  Serial.println(_impl->apPassword);
  Serial.print("IP: ");
  Serial.println(apIP);

  return true;
}

void AccessPoint::stopPortal() {
  if (_impl == nullptr) {
    return;
  }

  if (_impl->serverStarted) {
    _impl->server.stop();
    _impl->serverStarted = false;
  }

  if (_impl->apStarted) {
    _impl->dnsServer.stop();
    WiFi.softAPdisconnect(false);
    _impl->apStarted = false;
  }

  if (_impl->filesystemStarted) {
    LittleFS.end();
    _impl->filesystemStarted = false;
  }

  _impl->shutdownPending = false;
}

void AccessPoint::serveFile(const char* path, const char* contentType) {
  File file = LittleFS.open(path, "r");

  if (!file) {
    _impl->server.send(404, "text/plain", "Arquivo nao encontrado");
    return;
  }

  _impl->server.streamFile(file, contentType);
  file.close();
}

void AccessPoint::handleNetworks() {
  const int networkCount = WiFi.scanNetworks();

  if (networkCount < 0) {
    WiFi.scanDelete();
    _impl->server.send(
        500,
        "application/json",
        "{\"networks\":[],\"error\":\"scan_failed\"}");
    return;
  }

  String response = "{\"networks\":[";
  bool firstNetwork = true;

  for (int i = 0; i < networkCount; i++) {
    const String ssid = WiFi.SSID(i);

    if (ssid.length() == 0 || ssid == _impl->apSsid) {
      continue;
    }

    bool duplicate = false;
    for (int j = 0; j < i; j++) {
      if (WiFi.SSID(j) == ssid) {
        duplicate = true;
        break;
      }
    }

    if (duplicate) {
      continue;
    }

    if (!firstNetwork) {
      response += ',';
    }

    appendJsonString(response, ssid);
    firstNetwork = false;
  }

  response += "]}";
  WiFi.scanDelete();
  _impl->server.send(200, "application/json", response);
}

void AccessPoint::handleConnect() {
  if (_impl->shutdownPending) {
    _impl->server.send(
        409,
        "application/json",
        "{\"connected\":false,\"error\":\"shutdown_pending\"}");
    return;
  }

  const String body = _impl->server.arg("plain");

  if (!extractJsonString(body, "ssid", _impl->staSsid) ||
      !extractJsonString(body, "password", _impl->staPassword) ||
      _impl->staSsid.length() == 0) {
    _impl->server.send(
        400,
        "application/json",
        "{\"connected\":false,\"error\":\"invalid_request\"}");
    return;
  }

  Serial.print("Tentando conectar em: ");
  Serial.println(_impl->staSsid);

  if (!connectSta()) {
    _impl->server.send(
        503,
        "application/json",
        "{\"connected\":false,\"error\":\"connection_failed\"}");
    return;
  }

  if (!saveStaCredentials()) {
    WiFi.disconnect(false, false);
    _impl->server.send(
        500,
        "application/json",
        "{\"connected\":false,\"error\":\"save_failed\"}");
    return;
  }

  _impl->server.send(
      200,
      "application/json",
      "{\"connected\":true}");
  _impl->shutdownPending = true;
  _impl->shutdownAt = millis() + PORTAL_SHUTDOWN_DELAY_MS;
}

void AccessPoint::handleClient() {
  if (_impl == nullptr) {
    return;
  }

  if (_impl->serverStarted) {
    _impl->dnsServer.processNextRequest();
    _impl->server.handleClient();
  }

  if (_impl->shutdownPending &&
      static_cast<long>(millis() - _impl->shutdownAt) >= 0) {
    stopPortal();
    WiFi.mode(WIFI_STA);
    Serial.println("Access point encerrado; STA mantida");
  }
}
