#ifdef CH_BUILD
// CriptoHost NerdOS — servidor web local (dashboard + API fleet + OTA)
// Escopo §4.1: /  /api/status  /api/config  /api/identify  /api/restart
//              /api/factory-reset  /api/ota  /api/fleet  /api/bench  /ws
#include "ch_web.h"
#include "ch_config.h"
#include "ch_state.h"
#include "ch_mdns.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "../wManager.h"
#include "../drivers/storage/storage.h"
#include "../drivers/storage/nvMemory.h"
#include "../ShaTests/nerdSHA256plus.h"

extern TSettings Settings;
extern nvMemory nvMem;

static AsyncWebServer server(CH_HTTP_PORT);
static AsyncWebSocket ws("/ws");

static volatile uint32_t s_identifyUntil = 0;   // millis limite do blink
static volatile bool s_restartPending = false;
static volatile bool s_fleetRefresh = false;
static String s_fleetCache = "[]";
static bool s_otaOk = false;

#ifndef CH_LED_PIN
#define CH_LED_PIN 2   // LED onboard da maioria dos DevKit; sem efeito se ausente
#endif

static void addCors(AsyncWebServerResponse* r)
{
  r->addHeader("Access-Control-Allow-Origin", "*"); // fleet: browser consulta peers direto
}

static void sendJson(AsyncWebServerRequest* req, const String& body, int code = 200)
{
  AsyncWebServerResponse* r = req->beginResponse(code, "application/json", body);
  addCors(r);
  req->send(r);
}

// ---- /api/config ----
static String configJson()
{
  String j = "{\"pool\":\"" + Settings.PoolAddress + "\"";
  j += ",\"port\":" + String(Settings.PoolPort);
  j += ",\"wallet\":\"" + String(Settings.BtcWallet) + "\"";
  j += ",\"password\":\"" + String(Settings.PoolPassword) + "\"";
  j += ",\"timezone\":" + String(Settings.Timezone);
  j += ",\"fw\":\"" CH_VERSION "\",\"hardware\":\"" CH_HARDWARE "\"}";
  return j;
}

static void handleConfigPost(AsyncWebServerRequest* req, uint8_t* data, size_t len)
{
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, data, len)) { sendJson(req, "{\"error\":\"json invalido\"}", 400); return; }

  // Validação mínima de campos (trust boundary)
  String pool = doc["pool"] | Settings.PoolAddress;
  int port = doc["port"] | Settings.PoolPort;
  String wallet = doc["wallet"] | String(Settings.BtcWallet);
  String pass = doc["password"] | String(Settings.PoolPassword);
  pool.trim(); wallet.trim();
  if (pool.length() < 4 || pool.length() > 128 || port < 1 || port > 65535 ||
      wallet.length() < 8 || wallet.length() >= 80 || pass.length() >= 80) {
    sendJson(req, "{\"error\":\"campos invalidos\"}", 400); return;
  }

  Settings.PoolAddress = pool;
  Settings.PoolPort = port;
  strncpy(Settings.BtcWallet, wallet.c_str(), sizeof(Settings.BtcWallet) - 1);
  strncpy(Settings.PoolPassword, pass.c_str(), sizeof(Settings.PoolPassword) - 1);
  if (doc.containsKey("timezone")) Settings.Timezone = doc["timezone"].as<int>();
  nvMem.saveConfig(&Settings);
  ch_log_event("conn", "Config salva — reiniciando");
  sendJson(req, "{\"ok\":true,\"restarting\":true}");
  s_restartPending = true; // Save & Restart (M2-04)
}

// ---- /api/ota (M2-06/07) ----
static void handleOtaUpload(AsyncWebServerRequest* req, String filename, size_t index,
                            uint8_t* data, size_t len, bool final)
{
  if (index == 0) {
    s_otaOk = false;
    if (len < 1 || data[0] != 0xE9) { // magic byte de imagem ESP32 (M2-07)
      req->send(400, "application/json", "{\"error\":\"binario invalido (magic byte)\"}");
      return;
    }
    Serial.printf("[CH] OTA start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      req->send(500, "application/json", "{\"error\":\"sem espaco para OTA\"}");
      return;
    }
    s_otaOk = true;
  }
  if (s_otaOk && len && Update.write(data, len) != len) {
    Update.abort();
    s_otaOk = false;
  }
  if (final && s_otaOk) {
    if (Update.end(true)) {
      Serial.println("[CH] OTA concluido");
      ch_log_event("conn", "OTA aplicado — reiniciando");
    } else {
      s_otaOk = false;
      Serial.printf("[CH] OTA erro: %s\n", Update.errorString());
    }
  }
}

// ---- /api/bench (M1-05) ----
static String benchJson()
{
  // Bench SW: nerdSHA256plus (midstate + double hash) por 200 ms
  extern uint32_t elapsedKHs;
  uint8_t header[80] = {0}, hash[32];
  nerdSHA256_context mid;
  nerd_mids(mid.digest, header);
  uint32_t n = 0, t0 = millis();
  while (millis() - t0 < 200) {
    for (int i = 0; i < 100; i++) { header[76] = n; nerd_sha256d(&mid, header + 64, hash); n++; }
  }
  double swKhs = (double)n / (millis() - t0);
  String j = "{\"sw_khs\":" + String(swKhs, 1);
  j += ",\"live_khs\":" + String((double)elapsedKHs, 1);
#ifdef USE_HW_SHA
  j += ",\"method\":\"hw\"}";
#else
  j += ",\"method\":\"sw\"}";
#endif
  return j;
}

void ch_web_setup()
{
  LittleFS.begin(true);

  ws.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType t, void*, uint8_t*, size_t) {
    if (t == WS_EVT_CONNECT) c->text(ch_status_json());
  });
  server.addHandler(&ws);

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, ch_status_json()); });
  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, ch_events_json()); });
  server.on("/api/bench",  HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, benchJson()); });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, configJson()); });
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len == total) handleConfigPost(r, data, len); // ponytail: config cabe em 1 chunk (<1.4KB)
    });

  server.on("/api/identify", HTTP_POST, [](AsyncWebServerRequest* r) {
    s_identifyUntil = millis() + 10000;
    ch_log_event("conn", "Identify acionado");
    sendJson(r, "{\"ok\":true}");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* r) {
    sendJson(r, "{\"ok\":true,\"restarting\":true}");
    s_restartPending = true;
  });

  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest* r) {
    // reset_configuration limpa config + wifi e reinicia (volta ao captive portal)
    r->onDisconnect([]() { reset_configuration(); });
    sendJson(r, "{\"ok\":true,\"resetting\":true}");
  });

  server.on("/api/fleet", HTTP_GET, [](AsyncWebServerRequest* r) {
    s_fleetRefresh = true; // query mDNS roda no loop() p/ não travar a task async
    sendJson(r, "{\"self\":" + ch_status_json() + ",\"peers\":" + s_fleetCache + "}");
  });

  server.on("/api/ota", HTTP_POST, [](AsyncWebServerRequest* r) {
    bool ok = s_otaOk && !Update.hasError();
    r->send(ok ? 200 : 500, "application/json",
            ok ? "{\"ok\":true,\"restarting\":true}" : "{\"error\":\"falha no update\"}");
    if (ok) s_restartPending = true;
  }, handleOtaUpload);

  // CORS preflight p/ fleet cross-origin
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (r->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse* resp = r->beginResponse(204);
      addCors(resp);
      resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
      r->send(resp);
    } else r->send(404, "text/plain", "Not found");
  });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=300");

  server.begin();
  Serial.printf("[CH] Web server em http://%s\n", WiFi.localIP().toString().c_str());
}

void ch_web_loop()
{
  static uint32_t lastPush = 0, lastBlink = 0;
  uint32_t now = millis();

  ch_state_tick();

  if (now - lastPush >= CH_WS_PUSH_MS) {
    lastPush = now;
    ws.cleanupClients();
    if (ws.count() > 0)
      ws.textAll("{\"status\":" + ch_status_json() + ",\"events\":" + ch_events_json() + "}");
  }

  if (s_identifyUntil > now) { // Identify: pisca LED a 5 Hz (M2-03/08)
    if (now - lastBlink >= 100) {
      lastBlink = now;
      pinMode(CH_LED_PIN, OUTPUT);
      digitalWrite(CH_LED_PIN, !digitalRead(CH_LED_PIN));
    }
  }

  if (s_fleetRefresh) {
    s_fleetRefresh = false;
    s_fleetCache = ch_fleet_json(); // bloqueia ~3s aqui no loop(), mineração não para (tasks próprias)
  }

  if (s_restartPending) {
    s_restartPending = false;
    delay(300); // deixa a resposta HTTP sair
    ESP.restart();
  }
}
#endif // CH_BUILD
