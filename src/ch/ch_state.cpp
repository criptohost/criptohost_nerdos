#ifdef CH_BUILD
#include "ch_state.h"
#include "ch_config.h"

#include <WiFi.h>
#include <string.h>
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

String ch_chip_id()
{
  uint64_t mac = ESP.getEfuseMac();
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", (uint8_t)(mac >> 8), (uint8_t)mac);
  return String(buf);
}

String ch_ap_ssid()
{
  String s = String(CH_AP_SSID) + "-" + ch_chip_id();
  if (s.length() > 32) s.remove(32);
  return s;
}

String ch_worker_name()
{
  // wallet.worker configurado tem prioridade; senão CH-<chip>
  String w(Settings.BtcWallet);
  int dot = w.lastIndexOf('.');
  if (dot > 0 && dot < (int)w.length() - 1)
    return w.substring(dot + 1);
  return String(CH_WORKER_PREFIX) + ch_chip_id();
}

bool ch_ensure_unique_identity()
{
  String w(Settings.BtcWallet);
  int dot = w.lastIndexOf('.');
  String user = (dot > 0) ? w.substring(0, dot) : w;
  String wrk = (dot > 0 && dot < (int)w.length() - 1) ? w.substring(dot + 1) : "";
  bool placeholder = user.startsWith("YOUR_WALLET");
  bool generic = !wrk.length() || wrk.equalsIgnoreCase("CH-01");
  if (!placeholder || !generic) return false;
  String nw = String("YOUR_WALLET.") + String(CH_WORKER_PREFIX) + ch_chip_id();
  strncpy(Settings.BtcWallet, nw.c_str(), sizeof(Settings.BtcWallet) - 1);
  Settings.BtcWallet[sizeof(Settings.BtcWallet) - 1] = '\0';
  return true;
}

static String ch_sanitize_host(const String& src, size_t maxLen)
{
  String t(src);
  t.toLowerCase();
  String h;
  h.reserve(maxLen);
  for (size_t i = 0; i < t.length() && h.length() < maxLen; i++) {
    char c = t[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) h += c;
    else if (h.length() && h[h.length() - 1] != '-') h += '-';
  }
  while (h.endsWith("-")) h.remove(h.length() - 1);
  return h;
}

String ch_mdns_hostname()
{
  String id = ch_chip_id();
  id.toLowerCase();
  String base = ch_sanitize_host(ch_worker_name(), 31);
  if (!base.length()) base = "criptohost";
  if (base.endsWith(id)) {
    if (base.length() > 31) base.remove(31);
    return base;
  }
  size_t keep = 31 - 1 - id.length();
  if (base.length() > keep) base.remove(keep);
  while (base.endsWith("-")) base.remove(base.length() - 1);
  if (!base.length()) base = "ch";
  return base + "-" + id;
}

String ch_status_json()
{
  // ponytail: elapsedKHs é atualizado ~1x/s pelo runMonitor → já é kH/s
  String j;
  j.reserve(576);
  j += "{\"worker\":\"" + ch_worker_name() + "\"";
  j += ",\"hostname\":\"" + ch_mdns_hostname() + "\"";
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  j += ",\"hardware\":\"" CH_HARDWARE "\"";
  j += ",\"fw\":\"" CH_VERSION "\"";
  j += ",\"status\":\"";
  if (elapsedKHs > 0) j += "mining";
  else if (WiFi.status() != WL_CONNECTED) j += "offline";
  else if (templates == 0) j += "connecting";
  else j += "idle";
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
#define CH_ERRORS_MAX 48
struct ChEvent { uint32_t t; String type; String msg; };
static ChEvent s_events[CH_EVENTS_MAX];
static int s_evHead = 0, s_evCount = 0;
static ChEvent s_errors[CH_ERRORS_MAX];
static int s_errHead = 0, s_errCount = 0;

static bool ch_is_error_event(const char* type, const String& msg)
{
  if (!type) return false;
  if (strcmp(type, "reject") == 0) return true;
  if (strcmp(type, "conn") != 0) return false;
  String m(msg);
  m.toLowerCase();
  return m.indexOf("fail") >= 0 || m.indexOf("error") >= 0
      || m.indexOf("lost") >= 0 || m.indexOf("timeout") >= 0;
}

static void ch_ring_push(ChEvent* buf, int* head, int* count, int max,
                         const char* type, const String& msg)
{
  buf[*head] = { (uint32_t)upTime, String(type), msg };
  *head = (*head + 1) % max;
  if (*count < max) (*count)++;
}

static String ch_ring_json(const ChEvent* buf, int head, int count, int max)
{
  auto esc = [](const String& in) {
    String o;
    o.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
      char c = in[i];
      if (c == '"' || c == '\\') { o += '\\'; o += c; }
      else if ((uint8_t)c >= 0x20) o += c;
    }
    return o;
  };
  String j = "[";
  for (int i = 0; i < count; i++) {
    int idx = (head - 1 - i + max * 2) % max;
    if (i) j += ",";
    j += "{\"t\":" + String(buf[idx].t) +
         ",\"type\":\"" + buf[idx].type + "\"" +
         ",\"msg\":\"" + esc(buf[idx].msg) + "\"}";
  }
  j += "]";
  return j;
}

void ch_log_event(const char* type, const String& msg)
{
  ch_ring_push(s_events, &s_evHead, &s_evCount, CH_EVENTS_MAX, type, msg);
  if (ch_is_error_event(type, msg))
    ch_ring_push(s_errors, &s_errHead, &s_errCount, CH_ERRORS_MAX, type, msg);
}

String ch_events_json()
{
  return ch_ring_json(s_events, s_evHead, s_evCount, CH_EVENTS_MAX);
}

String ch_errors_json()
{
  return ch_ring_json(s_errors, s_errHead, s_errCount, CH_ERRORS_MAX);
}

void ch_state_tick()
{
  // Sintetiza eventos a partir dos deltas dos contadores — zero hooks no core
  static uint32_t lSent = 0, lAcc = 0, lRej = 0, lTpl = 0, lValid = 0;
  static bool lMining = false;

  uint32_t v;
  if ((v = ch_shares_sent)     != lSent)  { ch_log_event("share",  "Share sent #" + String(v));           lSent = v; }
  if ((v = ch_shares_accepted) != lAcc)   { ch_log_event("accept", "Share accepted by pool");            lAcc = v; }
  if ((v = ch_shares_rejected) != lRej)   { lRej = v; } // detail is logged from stratum when the pool replies
  if ((v = templates)          != lTpl)   { if (v % 25 == 0) ch_log_event("job", "Template #" + String(v)); lTpl = v; }
  if ((v = valids)             != lValid) { ch_log_event("block",  "VALID BLOCK FOUND!");               lValid = v; }

  bool mining = elapsedKHs > 0;
  if (mining != lMining) {
    ch_log_event("conn", mining ? "Mining on " + Settings.PoolAddress : "Hashrate idle — checking pool");
    lMining = mining;
  }
}
#endif // CH_BUILD
