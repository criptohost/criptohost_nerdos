#ifdef CH_BUILD
#include "ch_mdns.h"
#include "ch_config.h"
#include "ch_state.h"

#include <WiFi.h>
#include <ESPmDNS.h>

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
  j += "]";
  return j;
}
#endif // CH_BUILD
