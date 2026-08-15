#ifndef CH_MDNS_H
#define CH_MDNS_H

#include <Arduino.h>

void   ch_mdns_setup();       // anuncia _criptohost._tcp com TXT worker/fw/hardware
String ch_fleet_json();       // descobre peers via mDNS e devolve [{worker,ip,port},...]

#endif // CH_MDNS_H
