#ifdef CH_BUILD
#include "ch_state.h"
#include "ch_config.h"

#include <WiFi.h>
#include "../mining.h"
#include "../drivers/storage/storage.h"

// Globais do core (mining.cpp) — leitura apenas
extern uint32_t templates;
extern uint32_t Mhashes;
extern uint32_t hashes;
extern uint32_t elapsedKHs;
extern uint64_t upTime;
extern volatile uint32_t shares;
extern volatile uint32_t valids;
extern volatile uint32_t ch_shares_sent;
extern volatile uint32_t ch_shares_accepted;
extern volatile uint32_t ch_shares_rejected;
extern double best_diff;
extern TSettings Settings;

String ch_worker_name()
{
  // wallet.worker configurado tem prioridade; senão CH-<modelo>-<chip>
  String w(Settings.BtcWallet);
  int dot = w.indexOf('.');
  if (dot > 0 && dot < (int)w.length() - 1)
    return w.substring(dot + 1);
  char buf[24];
  snprintf(buf, sizeof(buf), "%s%s-%02X", CH_WORKER_PREFIX, CH_MODEL,
           (uint8_t)(ESP.getEfuseMac() & 0xFF));
  return String(buf);
}

String ch_status_json()
{
  // ponytail: elapsedKHs é atualizado ~1x/s pelo runMonitor → já é kH/s
  String j;
  j.reserve(512);
  j += "{\"worker\":\"" + ch_worker_name() + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"hardware\":\"" CH_HARDWARE "\"";
  j += ",\"fw\":\"" CH_VERSION "\"";
  j += ",\"status\":\"";
  j += (elapsedKHs > 0 ? "mining" : (WiFi.status() == WL_CONNECTED ? "idle" : "offline"));
  j += "\"";
  j += ",\"hashrate_khs\":" + String((double)elapsedKHs, 1);
  j += ",\"temp_c\":" + String(temperatureRead(), 1);
  j += ",\"rssi_dbm\":" + String(WiFi.RSSI());
  j += ",\"uptime_s\":" + String((uint32_t)upTime);
  j += ",\"pool\":\"" + Settings.PoolAddress + ":" + String(Settings.PoolPort) + "\"";
  uint32_t sent = ch_shares_sent, acc = ch_shares_accepted, rej = ch_shares_rejected;
  uint32_t pending = (sent > acc + rej) ? sent - acc - rej : 0;
  j += ",\"shares\":{\"found\":" + String(shares) +
       ",\"sent\":" + String(sent) +
       ",\"accepted\":" + String(acc) +
       ",\"rejected\":" + String(rej) +
       ",\"pending\":" + String(pending) + "}";
  j += ",\"best_difficulty\":" + String(best_diff, 4);
  j += ",\"templates\":" + String(templates);
  j += ",\"valid_blocks\":" + String(valids);
  j += "}";
  return j;
}

// ---- Event log (share stream + connection log) ----
#define CH_EVENTS_MAX 24
struct ChEvent { uint32_t t; String type; String msg; };
static ChEvent s_events[CH_EVENTS_MAX];
static int s_evHead = 0, s_evCount = 0;

void ch_log_event(const char* type, const String& msg)
{
  s_events[s_evHead] = { (uint32_t)upTime, String(type), msg };
  s_evHead = (s_evHead + 1) % CH_EVENTS_MAX;
  if (s_evCount < CH_EVENTS_MAX) s_evCount++;
}

String ch_events_json()
{
  String j = "[";
  for (int i = 0; i < s_evCount; i++) {
    int idx = (s_evHead - 1 - i + CH_EVENTS_MAX * 2) % CH_EVENTS_MAX;
    if (i) j += ",";
    j += "{\"t\":" + String(s_events[idx].t) +
         ",\"type\":\"" + s_events[idx].type + "\"" +
         ",\"msg\":\"" + s_events[idx].msg + "\"}";
  }
  j += "]";
  return j;
}

void ch_state_tick()
{
  // Sintetiza eventos a partir dos deltas dos contadores — zero hooks no core
  static uint32_t lSent = 0, lAcc = 0, lRej = 0, lTpl = 0, lValid = 0;
  static bool lMining = false;

  uint32_t v;
  if ((v = ch_shares_sent)     != lSent)  { ch_log_event("share",  "Share sent #" + String(v));           lSent = v; }
  if ((v = ch_shares_accepted) != lAcc)   { ch_log_event("accept", "Share accepted by pool");            lAcc = v; }
  if ((v = ch_shares_rejected) != lRej)   { ch_log_event("reject", "Share rejected by pool");            lRej = v; }
  if ((v = templates)          != lTpl)   { if (v % 25 == 0) ch_log_event("job", "Template #" + String(v)); lTpl = v; }
  if ((v = valids)             != lValid) { ch_log_event("block",  "VALID BLOCK FOUND!");               lValid = v; }

  bool mining = elapsedKHs > 0;
  if (mining != lMining) {
    ch_log_event("conn", mining ? "Mining on " + Settings.PoolAddress : "Hashrate idle — checking pool");
    lMining = mining;
  }
}
#endif // CH_BUILD
