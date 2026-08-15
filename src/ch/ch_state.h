#ifndef CH_STATE_H
#define CH_STATE_H

#include <Arduino.h>

// Single source of truth do contrato /api/status (escopo §4.2).
// Lê os globais do core (mining.cpp) sem tocá-los — M0-05.

String ch_worker_name();          // "CH-<modelo>-<nn>" ou wallet.worker configurado
String ch_status_json();          // JSON completo do contrato fleet
void   ch_log_event(const char* type, const String& msg); // ring buffer p/ dashboard
String ch_events_json();          // últimos eventos (share stream + connection log)
void   ch_state_tick();           // sintetiza eventos a partir dos contadores

#endif // CH_STATE_H
