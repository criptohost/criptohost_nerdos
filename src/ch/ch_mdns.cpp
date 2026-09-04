#ifdef CH_BUILD
#include "ch_mdns.h"
#include "ch_config.h"
#include "ch_state.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

void ch_mdns_setup()
{
  String host = ch_mdns_hostname();
  if (!MDNS.begin(host.c_str())) {
    Serial.println("[CH] mDNS falhou");
    return;
  }
  MDNS.addService(CH_MDNS_SERVICE, "tcp", CH_HTTP_PORT);
  MDNS.addServiceTxt(CH_MDNS_SERVICE, "tcp", "worker", ch_worker_name().c_str());
  MDNS.addServiceTxt(CH_MDNS_SERVICE, "tcp", "fw", CH_VERSION);
  MDNS.addServiceTxt(CH_MDNS_SERVICE, "tcp", "hardware", CH_HARDWARE);
  Serial.printf("[CH] mDNS: http://%s.local (_%s._tcp)\n", host.c_str(), CH_MDNS_SERVICE);
}

// Cache de peers: a query one-shot perde respostas (multicast em Wi-Fi não
// retransmite), então cada chamada mescla o resultado novo com o último visto.
// Peer some do cache após 10 min sem responder.
#define CH_PEER_CACHE 12
#define CH_PEER_TTL_MS 600000UL
struct ChPeerCache { String worker, fw, hardware, ip; uint16_t port; uint32_t last_ms; };
static ChPeerCache s_peers[CH_PEER_CACHE];

// ---- lista de peers replicada (gossip) ----
// /peers.conf no LittleFS é o mesmo documento do ch/peers.conf dos agents:
// 'ip[:porta] [token]' por linha, revisão em /peers.rev — revisão maior vence
// e propaga pela frota. A placa só faz pull (agents fazem pull+push).
static String s_peersContent;
static uint32_t s_peersRev = 0;
static bool s_peersLoaded = false;

struct ChStaticPeer { String ip, token; uint16_t port; };

static void peersLoad()
{
  if (s_peersLoaded) return;
  s_peersLoaded = true;
  File f = LittleFS.open("/peers.conf", "r");
  if (f) { s_peersContent = f.readString(); f.close(); }
  f = LittleFS.open("/peers.rev", "r");
  if (f) { s_peersRev = (uint32_t)f.readString().toInt(); f.close(); }
}

static void peersStore()
{
  File f = LittleFS.open("/peers.conf", "w");
  if (f) { f.print(s_peersContent); f.close(); }
  f = LittleFS.open("/peers.rev", "w");
  if (f) { f.print(String(s_peersRev)); f.close(); }
}

// Percorre as linhas válidas; devolve quantas achou (até max)
static int peersParse(ChStaticPeer* out, int max)
{
  peersLoad();
  int n = 0;
  int start = 0;
  while (n < max && start <= (int)s_peersContent.length()) {
    int nl = s_peersContent.indexOf('\n', start);
    String ln = nl < 0 ? s_peersContent.substring(start) : s_peersContent.substring(start, nl);
    start = nl < 0 ? s_peersContent.length() + 1 : nl + 1;
    int h = ln.indexOf('#');
    if (h >= 0) ln = ln.substring(0, h);
    ln.trim();
    if (!ln.length()) continue;
    int sp = ln.indexOf(' ');
    String hostport = sp < 0 ? ln : ln.substring(0, sp);
    String tok = sp < 0 ? "" : ln.substring(sp + 1);
    tok.trim();
    int c = hostport.indexOf(':');
    out[n].ip = c < 0 ? hostport : hostport.substring(0, c);
    out[n].port = c < 0 ? 80 : (uint16_t)hostport.substring(c + 1).toInt();
    out[n].token = tok;
    n++;
  }
  return n;
}

static bool peerLineOk(const String& ln)
{
  int sp = ln.indexOf(' ');
  String hostport = sp < 0 ? ln : ln.substring(0, sp);
  String tok = sp < 0 ? "" : ln.substring(sp + 1);
  int c = hostport.indexOf(':');
  String host = c < 0 ? hostport : hostport.substring(0, c);
  String port = c < 0 ? "" : hostport.substring(c + 1);
  if (!host.length()) return false;
  for (size_t i = 0; i < host.length(); i++) {
    char ch = host[i];
    if (!isalnum(ch) && ch != '.' && ch != '_' && ch != '-') return false;
  }
  if (c >= 0) {
    if (!port.length() || port.length() > 5) return false;
    for (size_t i = 0; i < port.length(); i++)
      if (!isdigit(port[i])) return false;
  }
  for (size_t i = 0; i < tok.length(); i++) {
    char ch = tok[i];
    if (!isalnum(ch) && ch != '_' && ch != '-') return false;
  }
  return true;
}

static String jsonEsc(const String& in)
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

String ch_peers_json()
{
  peersLoad();
  return "{\"content\":\"" + jsonEsc(s_peersContent) +
         "\",\"rev\":" + String(s_peersRev) + ",\"editable\":true}";
}

bool ch_peers_set(const String& content, uint32_t rev, String& err)
{
  peersLoad();
  if (content.length() > 2048) { err = "peers list too large"; return false; }
  int start = 0;
  while (start <= (int)content.length()) {
    int nl = content.indexOf('\n', start);
    String ln = nl < 0 ? content.substring(start) : content.substring(start, nl);
    start = nl < 0 ? content.length() + 1 : nl + 1;
    int h = ln.indexOf('#');
    if (h >= 0) ln = ln.substring(0, h);
    ln.trim();
    if (ln.length() && !peerLineOk(ln)) { err = "invalid line: " + ln; return false; }
  }
  if (rev) {                        // push de sync: rev maior vence
    if (rev <= s_peersRev) { err = "stale"; return false; }
  } else {                          // edição humana: revisão nova, monotônica
    uint32_t now = (uint32_t)time(nullptr);
    rev = now > s_peersRev ? now : s_peersRev + 1;
  }
  s_peersContent = content;
  if (s_peersContent.length() && !s_peersContent.endsWith("\n")) s_peersContent += "\n";
  s_peersRev = rev;
  peersStore();
  ch_log_event("conn", "Fleet peers updated (rev " + String(rev) + ")");
  return true;
}

// Pull de gossip: 1 vizinho por chamada (round-robin), rev maior vence.
// Chamado a cada ~60s pelo ch_web_loop; HTTP bloqueia ~2s no loop(), como a
// query do Fleet — a mineração roda em tasks próprias e não para.
void ch_peers_sync_tick()
{
  static int rr = 0;
  ChStaticPeer list[16];
  String myIp = WiFi.localIP().toString();
  ChStaticPeer targets[28];
  int nt = 0;
  int ns = peersParse(list, 16);
  for (int i = 0; i < ns && nt < 28; i++)
    if (!(list[i].ip == myIp && list[i].port == CH_HTTP_PORT)) targets[nt++] = list[i];
  uint32_t now = millis();
  for (int k = 0; k < CH_PEER_CACHE && nt < 28; k++) {
    if (s_peers[k].worker.isEmpty() || now - s_peers[k].last_ms > CH_PEER_TTL_MS) continue;
    if (s_peers[k].ip == myIp) continue;
    targets[nt].ip = s_peers[k].ip;
    targets[nt].port = s_peers[k].port;
    targets[nt].token = "";
    nt++;
  }
  if (!nt) return;   // sem vizinho conhecido: o push dos agents ainda nos alcança

  ChStaticPeer& t = targets[rr++ % nt];
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(2500);
  if (!http.begin("http://" + t.ip + ":" + String(t.port) + "/api/peers")) return;
  if (t.token.length()) http.addHeader("X-CH-Token", t.token);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, http.getString())) {
      uint32_t trev = doc["rev"] | 0;
      if (trev > s_peersRev && doc.containsKey("content")) {
        String err;
        if (ch_peers_set(doc["content"].as<String>(), trev, err))
          Serial.printf("[CH] fleet list adotada de %s (rev %u)\n", t.ip.c_str(), trev);
      }
    }
  }
  http.end();
}

String ch_fleet_json()
{
  // ponytail: query síncrona (~3s) chamada pela página Fleet; suficiente p/ LAN
  int n = MDNS.queryService(CH_MDNS_SERVICE, "tcp");
  uint32_t now = millis();
  for (int i = 0; i < n; i++) {
    String w = MDNS.txt(i, "worker");
    if (w.isEmpty()) continue;
    int slot = -1, oldest = 0;
    for (int k = 0; k < CH_PEER_CACHE; k++) {
      if (s_peers[k].worker == w) { slot = k; break; }
      if (slot < 0 && s_peers[k].worker.isEmpty()) slot = k;
      if (now - s_peers[k].last_ms > now - s_peers[oldest].last_ms) oldest = k;
    }
    if (slot < 0) slot = oldest;
    s_peers[slot] = { w, MDNS.txt(i, "fw"), MDNS.txt(i, "hardware"),
                      MDNS.IP(i).toString(), MDNS.port(i), now };
  }
  String j = "[";
  bool first = true;
  for (int k = 0; k < CH_PEER_CACHE; k++) {
    if (s_peers[k].worker.isEmpty() || now - s_peers[k].last_ms > CH_PEER_TTL_MS) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"worker\":\"" + s_peers[k].worker + "\"";
    j += ",\"fw\":\"" + s_peers[k].fw + "\"";
    j += ",\"hardware\":\"" + s_peers[k].hardware + "\"";
    j += ",\"ip\":\"" + s_peers[k].ip + "\"";
    j += ",\"port\":" + String(s_peers[k].port) + "}";
  }
  // lista replicada: entradas estáticas que o mDNS não cobre (VPS, Android),
  // pulando a própria placa — cada nó vê a frota inteira menos ele mesmo
  ChStaticPeer sp[16];
  int ns = peersParse(sp, 16);
  String myIp = WiFi.localIP().toString();
  for (int i = 0; i < ns; i++) {
    if (sp[i].ip == myIp && sp[i].port == CH_HTTP_PORT) continue;
    bool dup = false;
    for (int k = 0; k < CH_PEER_CACHE && !dup; k++)
      dup = !s_peers[k].worker.isEmpty() && now - s_peers[k].last_ms <= CH_PEER_TTL_MS &&
            s_peers[k].ip == sp[i].ip && s_peers[k].port == sp[i].port;
    if (dup) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"worker\":\"peer-" + sp[i].ip + "\",\"fw\":\"?\",\"hardware\":\"?\"";
    j += ",\"ip\":\"" + sp[i].ip + "\",\"port\":" + String(sp[i].port);
    if (sp[i].token.length()) j += ",\"token\":\"" + sp[i].token + "\"";
    j += "}";
  }
  j += "]";
  return j;
}
#endif // CH_BUILD
