#ifndef CH_STATE_H
#define CH_STATE_H

#include <Arduino.h>

// Single source of truth do contrato /api/status (escopo §4.2).
// Lê os globais do core (mining.cpp) sem tocá-los — M0-05.

String ch_worker_name();          // sufixo de wallet.worker, ou CH-<chip> no default de fábrica
String ch_chip_id();              // 4 hex do MAC — único por placa
String ch_ap_ssid();              // AP de setup: CriptoHostNerdOS-<chip>
String ch_mdns_hostname();        // DHCP + http://nome.local (sempre único por MAC)
bool   ch_ensure_unique_identity(); // troca YOUR_WALLET.CH-01 → YOUR_WALLET.CH-<chip>
String ch_status_json();          // JSON completo do contrato fleet
void   ch_log_event(const char* type, const String& msg); // ring buffer p/ dashboard
String ch_events_json();          // últimos 24 eventos (share stream + connection log)
String ch_errors_json();          // rejects/falhas, ring próprio (não evicto pelo live log)
void   ch_state_tick();           // sintetiza eventos a partir dos contadores

#endif // CH_STATE_H
