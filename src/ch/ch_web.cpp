#ifdef CH_BUILD
// CriptoHost NerdOS — servidor web local (dashboard + API fleet + OTA)
// Escopo §4.1: /  /api/status  /api/config  /api/restart
//              /api/factory-reset  /api/ota  /api/fleet  /api/bench  /ws
#include "ch_web.h"
#include "ch_config.h"
#include "ch_state.h"
#include "ch_mdns.h"

#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include "../wManager.h"
#include "../drivers/storage/storage.h"
#include "../drivers/storage/nvMemory.h"
#include "../ShaTests/nerdSHA256plus.h"
#include "../mining.h"

extern TSettings Settings;
extern nvMemory nvMem;

static AsyncWebServer server(CH_HTTP_PORT);
static AsyncWebSocket ws("/ws");

static volatile bool s_restartPending = false;
static volatile bool s_fleetRefresh = false;
static String s_fleetCache = "[]";
static bool s_wifiScan = false;
static bool s_wifiSavePending = false;
static String s_wifiScanCache = "[]";
static String s_wifiNewSsid;
static String s_wifiNewPass;
static bool s_otaBusy = false;
static bool s_otaPrepare = false;
static volatile bool s_otaReady = false;
static volatile bool s_otaFail = false;
static volatile bool s_otaFinishing = false;
static volatile bool s_otaDone = false;
static volatile bool s_otaOk = false;
static uint32_t s_otaLastChunk = 0;
static StreamBufferHandle_t s_otaStream = nullptr;
static TaskHandle_t s_otaWriter = nullptr;
static char s_otaErr[96] = {0};

static void otaSetErr(const char* why)
{
  strncpy(s_otaErr, why ? why : "update failed", sizeof(s_otaErr) - 1);
  s_otaErr[sizeof(s_otaErr) - 1] = '\0';
}

static void otaCleanupWriter()
{
  if (s_otaWriter) {
    vTaskDelete(s_otaWriter);
    s_otaWriter = nullptr;
  }
  if (s_otaStream) {
    vStreamBufferDelete(s_otaStream);
    s_otaStream = nullptr;
  }
}

static void otaFailAndResume()
{
  otaCleanupWriter();
  Update.abort();
  s_otaOk = false;
  s_otaBusy = false;
  s_otaReady = false;
  s_otaPrepare = false;
  s_otaFail = true;
  s_otaDone = true;
  mining_resume_after_ota();
  WiFi.setSleep(true);
}

static void otaWriterFail(const char* why)
{
  otaSetErr(why);
  Serial.printf("[CH] OTA failed: %s\n", s_otaErr);
  Update.abort();
  if (s_otaStream) {
    vStreamBufferDelete(s_otaStream);
    s_otaStream = nullptr;
  }
  s_otaOk = false;
  s_otaReady = false;
  s_otaBusy = false;
  s_otaPrepare = false;
  s_otaFail = true;
  s_otaDone = true;
  s_otaWriter = nullptr;
  mining_resume_after_ota();
  WiFi.setSleep(true);
  vTaskDelete(NULL);
}

static void otaWriterTask(void*)
{
  if (!esp_ota_get_next_update_partition(NULL))
    otaWriterFail("no OTA slot (USB factory flash needed)");
  if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    otaWriterFail(Update.errorString());
  s_otaStream = xStreamBufferCreate(4 * 1024, 1);
  if (!s_otaStream)
    otaWriterFail("out of memory");
  s_otaOk = true;
  s_otaReady = true;
  s_otaLastChunk = millis();
  Serial.println("[CH] OTA ready");

  uint8_t buf[1024];
  for (;;) {
    size_t n = xStreamBufferReceive(s_otaStream, buf, sizeof(buf), pdMS_TO_TICKS(200));
    if (n) {
      if (Update.write(buf, n) != n)
        otaWriterFail(Update.errorString());
    }
    if (s_otaFinishing && s_otaStream && xStreamBufferBytesAvailable(s_otaStream) == 0) {
      s_otaOk = !s_otaFail && Update.end(true);
      if (!s_otaOk) {
        otaSetErr(Update.errorString());
        Update.abort();
      }
      s_otaDone = true;
      s_otaWriter = nullptr;
      vTaskDelete(NULL);
    }
  }
}

static void addCors(AsyncWebServerResponse* r)
{
  r->addHeader("Access-Control-Allow-Origin", "*"); // fleet: browser consulta peers direto
}

static String jsonEscape(const String& in)
{
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if ((uint8_t)c >= 0x20) out += c;
  }
  return out;
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
  j += ",\"hostname\":\"" + ch_mdns_hostname() + "\"";
  j += ",\"ap_ssid\":\"" + ch_ap_ssid() + "\"";
  j += ",\"fw\":\"" CH_VERSION "\",\"hardware\":\"" CH_HARDWARE "\"}";
  return j;
}

static void handleConfigPost(AsyncWebServerRequest* req, uint8_t* data, size_t len)
{
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, data, len)) { sendJson(req, "{\"error\":\"invalid json\"}", 400); return; }

  // Validação mínima de campos (trust boundary)
  String pool = doc["pool"] | Settings.PoolAddress;
  int port = doc["port"] | Settings.PoolPort;
  String wallet = doc["wallet"] | String(Settings.BtcWallet);
  String pass = doc["password"] | String(Settings.PoolPassword);
  pool.trim(); wallet.trim();
  if (pool.length() < 4 || pool.length() > 128 || port < 1 || port > 65535 ||
      wallet.length() < 8 || wallet.length() >= 80 || pass.length() >= 80) {
    sendJson(req, "{\"error\":\"invalid fields\"}", 400); return;
  }
  if (doc.containsKey("timezone")) {
    int tz = doc["timezone"].as<int>();
    if (tz < -12 || tz > 12) { sendJson(req, "{\"error\":\"invalid timezone\"}", 400); return; }
    Settings.Timezone = tz;
  }

  Settings.PoolAddress = pool;
  Settings.PoolPort = port;
  strncpy(Settings.BtcWallet, wallet.c_str(), sizeof(Settings.BtcWallet) - 1);
  strncpy(Settings.PoolPassword, pass.c_str(), sizeof(Settings.PoolPassword) - 1);
  nvMem.saveConfig(&Settings);
  ch_log_event("conn", "Config saved — restarting");
  sendJson(req, "{\"ok\":true,\"restarting\":true}");
  s_restartPending = true; // Save & Restart (M2-04)
}

static void handleWifiPost(AsyncWebServerRequest* req, uint8_t* data, size_t len)
{
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, data, len)) { sendJson(req, "{\"error\":\"invalid json\"}", 400); return; }
  String ssid = doc["ssid"] | "";
  String pass = doc["password"] | "";
  ssid.trim();
  if (ssid.length() < 1 || ssid.length() > 32 || pass.length() > 63 ||
      (pass.length() > 0 && pass.length() < 8)) {
    sendJson(req, "{\"error\":\"invalid wifi fields\"}", 400);
    return;
  }
  s_wifiNewSsid = ssid;
  s_wifiNewPass = pass;
  s_wifiSavePending = true;
  ch_log_event("conn", "Wi-Fi saved — restarting");
  sendJson(req, "{\"ok\":true,\"restarting\":true}");
  s_restartPending = true;
}

// ---- /api/ota (M2-06/07) ----
// Flash writes run on a dedicated task so the async TCP callback only copies
// bytes. Mining must already be paused (/api/ota/prepare) — SHA on core 0
// otherwise starves Wi-Fi and iOS drops the POST around 15–25%.
static void handleOtaUpload(AsyncWebServerRequest* req, String filename, size_t index,
                            uint8_t* data, size_t len, bool final)
{
  (void)req;
  if (index == 0) {
    Serial.printf("[CH] OTA start: %s\n", filename.c_str());
    if (!s_otaReady || !s_otaStream) {
      s_otaFail = true;
      return;
    }
    if (len < 1 || data[0] != 0xE9) {
      s_otaFail = true;
      s_otaOk = false;
      return;
    }
  }
  s_otaLastChunk = millis();
  if (s_otaFail || !s_otaStream) return;
  if (len && xStreamBufferSend(s_otaStream, data, len, pdMS_TO_TICKS(20000)) != len)
    s_otaFail = true;
  if (final) {
    s_otaFinishing = true;
    uint32_t t0 = millis();
    while (!s_otaDone && millis() - t0 < 60000)
      delay(10);
  }
}

// ---- /api/bench (M1-05) ----
static String benchJson()
{
  // kH/s por backend medidos no boot (ch_sha_selftest) + hashrate vivo do minerador
  extern uint32_t elapsedKHs;
  const ch_sha_bench_t& b = ch_sha_bench;
  String j = "{\"sw_khs\":" + String(b.sw_khs, 1);
  j += ",\"mbedtls_khs\":" + String(b.mbedtls_khs, 1);
  j += ",\"hw_khs\":" + String(b.hw_khs, 1);
  j += ",\"hw_backend\":\"" + String(b.hw_backend ? b.hw_backend : "none") + "\"";
  j += ",\"hw_vectors_ok\":" + String(b.hw_ok) + ",\"hw_vectors\":" + String(b.hw_n);
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
  server.on("/api/errors", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, ch_errors_json()); });
  server.on("/api/bench",  HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, benchJson()); });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, configJson()); });
  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len == total) handleConfigPost(r, data, len); // ponytail: config cabe em 1 chunk (<1.4KB)
    });

  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest* r) {
    sendJson(r, String("{\"ssid\":\"") + jsonEscape(WiFi.SSID()) +
                    "\",\"rssi\":" + String(WiFi.RSSI()) +
                    ",\"ip\":\"" + WiFi.localIP().toString() + "\"}");
  });
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    s_wifiScan = true;
    sendJson(r, s_wifiScanCache);
  });
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index + len == total) handleWifiPost(r, data, len);
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

  // Lista de peers replicada (gossip): editável em qualquer nó, sincroniza sozinha
  server.on("/api/peers", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, ch_peers_json()); });
  server.on("/api/peers", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
    [](AsyncWebServerRequest* r, uint8_t* data, size_t len, size_t index, size_t total) {
      static String body;   // ponytail: 1 POST por vez (lista pode passar de 1 chunk)
      if (index == 0) body = "";
      body.concat((const char*)data, len);
      if (index + len != total) return;
      DynamicJsonDocument doc(4096);
      if (deserializeJson(doc, body) || total > 3072) {
        body = ""; sendJson(r, "{\"error\":\"invalid json\"}", 400); return;
      }
      String content = doc["content"] | "";
      uint32_t rev = doc["rev"] | 0;
      body = "";
      String err;
      if (!ch_peers_set(content, rev, err)) {
        if (err == "stale") { sendJson(r, "{\"ok\":false,\"stale\":true}"); return; }
        sendJson(r, "{\"error\":\"" + jsonEscape(err) + "\"}", 400); return;
      }
      sendJson(r, "{\"ok\":true}");
    });

  server.on("/api/ota/prepare", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (s_otaBusy) {
      sendJson(r, "{\"ok\":true}");
      return;
    }
    if (!esp_ota_get_next_update_partition(NULL)) {
      sendJson(r, "{\"error\":\"this flash layout has no OTA slot — erase and USB-flash the factory image once\"}", 400);
      return;
    }
    s_otaErr[0] = '\0';
    ws.closeAll();
    mining_pause_for_ota();
    WiFi.setSleep(false);
    s_otaBusy = true;
    s_otaReady = false;
    s_otaFail = false;
    s_otaDone = false;
    s_otaFinishing = false;
    s_otaOk = false;
    s_otaLastChunk = millis();
    s_otaPrepare = true;
    sendJson(r, "{\"ok\":true}");
  });
  server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    String j = String("{\"busy\":") + (s_otaBusy ? "true" : "false") +
               ",\"ready\":" + (s_otaReady ? "true" : "false");
    if (s_otaErr[0]) {
      j += ",\"error\":\"";
      j += jsonEscape(s_otaErr);
      j += "\"";
    }
    j += "}";
    sendJson(r, j);
  });
  server.on("/api/ota", HTTP_POST, [](AsyncWebServerRequest* r) {
    bool ok = s_otaOk && s_otaDone && !s_otaFail && !Update.hasError();
    if (ok) {
      Serial.println("[CH] OTA concluido");
      ch_log_event("conn", "OTA applied — restarting");
      r->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
      s_restartPending = true;
    } else {
      const char* err = (!s_otaReady) ? "miner was still running — retry"
                       : (s_otaFail && !s_otaOk) ? "invalid binary or flash write failed"
                       : "update failed";
      r->send(400, "application/json", String("{\"error\":\"") + err + "\"}");
      otaFailAndResume();
    }
  }, handleOtaUpload);

  // CORS preflight p/ fleet cross-origin
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (r->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse* resp = r->beginResponse(204);
      addCors(resp);
      resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-CH-Token");
      r->send(resp);
    } else r->send(404, "text/plain", "Not found");
  });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=300");

  server.begin();
  Serial.printf("[CH] Web server em http://%s\n", WiFi.localIP().toString().c_str());
}

// Watchdog do web server: se a porta 80 não responder (bind falhou no boot,
// heap apertado etc.), tenta re-begin; persistindo, reinicia a placa. Sem isso
// um begin() falho no boot deixa o nó headless para sempre (mDNS ok, web morta).
static void ch_web_watchdog()
{
  static uint32_t lastCheck = 0;
  static uint8_t fails = 0;
  uint32_t now = millis();
  if (now - lastCheck < 60000) return;
  lastCheck = now;
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient probe;
  bool ok = probe.connect(WiFi.localIP(), CH_HTTP_PORT, 1500);
  probe.stop();
  if (ok) { fails = 0; return; }

  fails++;
  Serial.printf("[CH] web watchdog: porta %d sem resposta (%u)\n", CH_HTTP_PORT, fails);
  if (fails == 2) {
    Serial.println("[CH] web watchdog: re-begin do servidor");
    server.begin();
  } else if (fails >= 4) {
    Serial.println("[CH] web watchdog: reiniciando a placa");
    ch_log_event("conn", "Web server dead — watchdog reboot");
    delay(200);
    ESP.restart();
  }
}

void ch_web_loop()
{
  static uint32_t lastPush = 0;
  uint32_t now = millis();
  ch_web_watchdog();

  if (s_otaPrepare) {
    s_otaPrepare = false;
    otaCleanupWriter();
    Update.abort();
    if (xTaskCreatePinnedToCore(otaWriterTask, "otaW", 6144, nullptr, 6, &s_otaWriter, 1) != pdPASS) {
      otaSetErr("could not start OTA task");
      Serial.println("[CH] OTA prepare failed");
      otaFailAndResume();
    }
  }

  if (s_wifiSavePending) {
    s_wifiSavePending = false;
    WiFi.persistent(true);
    if (s_wifiNewPass.length())
      WiFi.begin(s_wifiNewSsid.c_str(), s_wifiNewPass.c_str());
    else
      WiFi.begin(s_wifiNewSsid.c_str());
    delay(250);
  }

  if (s_restartPending) {
    s_restartPending = false;
    delay(300);
    ESP.restart();
  }

  if (s_otaBusy) {
    // millis() fresco + cast com sinal: o handler async seta s_otaLastChunk
    // DEPOIS do 'now' capturado no topo do loop — a subtração ficava negativa,
    // estourava o unsigned e matava o OTA como "stalled" no instante em que armava
    if ((int32_t)(millis() - s_otaLastChunk) > 120000) {
      Serial.println("[CH] OTA stalled — resuming mining");
      otaFailAndResume();
    }
    return;
  }

  ch_state_tick();

  if (now - lastPush >= CH_WS_PUSH_MS) {
    lastPush = now;
    ws.cleanupClients();
    if (ws.count() > 0)
      ws.textAll("{\"status\":" + ch_status_json() +
                 ",\"events\":" + ch_events_json() +
                 ",\"errors\":" + ch_errors_json() + "}");
  }

  if (s_fleetRefresh) {
    s_fleetRefresh = false;
    s_fleetCache = ch_fleet_json(); // bloqueia ~3s aqui no loop(), mineração não para (tasks próprias)
  }

  static uint32_t lastPeersSync = 0;
  if (WiFi.status() == WL_CONNECTED && now - lastPeersSync >= 60000) {
    lastPeersSync = now;
    ch_peers_sync_tick();
  }

  if (s_wifiScan) {
    s_wifiScan = false;
    int n = WiFi.scanNetworks(false, false);
    String j = "[";
    int cap = n > 20 ? 20 : n;
    for (int i = 0; i < cap; i++) {
      if (i) j += ',';
      j += "{\"ssid\":\"";
      j += jsonEscape(WiFi.SSID(i));
      j += "\",\"rssi\":";
      j += String(WiFi.RSSI(i));
      j += ",\"open\":";
      j += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "true" : "false";
      j += '}';
    }
    j += ']';
    s_wifiScanCache = j;
    WiFi.scanDelete();
  }
}
#endif // CH_BUILD
