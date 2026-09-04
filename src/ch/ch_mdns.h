#ifndef CH_MDNS_H
#define CH_MDNS_H

#include <Arduino.h>

void   ch_mdns_setup();       // anuncia _criptohost._tcp com TXT worker/fw/hardware
String ch_fleet_json();       // descobre peers via mDNS e devolve [{worker,ip,port},...]

// Lista de peers replicada (gossip): /peers.conf + /peers.rev no LittleFS
String ch_peers_json();                                            // {content,rev,editable}
bool   ch_peers_set(const String& content, uint32_t rev, String& err); // rev=0: edição humana
void   ch_peers_sync_tick();                                       // pull de 1 vizinho (round-robin)

#endif // CH_MDNS_H
