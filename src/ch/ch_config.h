#ifndef CH_CONFIG_H
#define CH_CONFIG_H

// CriptoHost NerdOS — constantes de identidade e rede
// Marca protegida (ver BRANDING.md). Compile com -D CH_UNBRANDED para fork neutro.

#ifndef CH_VERSION
#define CH_VERSION "v0.2.0-alpha"
#endif

#ifdef CH_UNBRANDED
#define CH_PRODUCT_NAME   "NerdOS Miner (unbranded)"
#define CH_AP_SSID        "NerdOSAP"
#define CH_WORKER_PREFIX  "NODE-"
#define CH_MDNS_SERVICE   "nerdosminer"   // _nerdosminer._tcp
#else
#define CH_PRODUCT_NAME   "CriptoHost NerdOS"
#define CH_AP_SSID        "CriptoHostNerdOS"
#define CH_WORKER_PREFIX  "CH-"
#define CH_MDNS_SERVICE   "criptohost"    // _criptohost._tcp
#endif

#if defined(BOARD_DEVKIT_V1)
#define CH_HARDWARE "ESP32 DevKit V1"
#define CH_MODEL    "DevKit"
#elif defined(BOARD_S3_HEADLESS)
#define CH_HARDWARE "ESP32-S3"
#define CH_MODEL    "S3"
#elif defined(BOARD_TDISPLAY_S3)
#define CH_HARDWARE "LilyGO T-Display S3"
#define CH_MODEL    "TDS3"
#elif defined(BOARD_C3)
#define CH_HARDWARE "ESP32-C3"
#define CH_MODEL    "C3"
#else
#define CH_HARDWARE "ESP32"
#define CH_MODEL    "ESP32"
#endif

#define CH_WS_PUSH_MS     5000   // push de telemetria no /ws (escopo O2: <=5s)
#define CH_HTTP_PORT      80

#endif // CH_CONFIG_H
