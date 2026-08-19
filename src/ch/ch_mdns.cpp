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

String ch_fleet_json()
{
  // ponytail: query síncrona (~3s) chamada pela página Fleet; suficiente p/ LAN
  int n = MDNS.queryService(CH_MDNS_SERVICE, "tcp");
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"worker\":\"" + MDNS.txt(i, "worker") + "\"";
    j += ",\"fw\":\"" + MDNS.txt(i, "fw") + "\"";
    j += ",\"hardware\":\"" + MDNS.txt(i, "hardware") + "\"";
    j += ",\"ip\":\"" + MDNS.IP(i).toString() + "\"";
    j += ",\"port\":" + String(MDNS.port(i)) + "}";
  }
  j += "]";
  return j;
}
#endif // CH_BUILD
