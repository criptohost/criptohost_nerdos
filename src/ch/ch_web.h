#ifndef CH_WEB_H
#define CH_WEB_H

void ch_web_setup();   // sobe AsyncWebServer + WS + rotas /api/* (chamar com WiFi conectado)
void ch_web_loop();    // push WS 5s, refresh de fleet — chamar do loop()

#endif // CH_WEB_H
