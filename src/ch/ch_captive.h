#ifndef CH_CAPTIVE_H
#define CH_CAPTIVE_H

// Compact Cripto Host skin for tzapu/WiFiManager captive pages.
// No webfonts: the AP has no internet. Pointers must stay valid (static storage).

#include <WiFi.h>
#include <WiFiManager.h>
#include <string.h>
#include "ch_config.h"
#include "ch_state.h"

static const char CH_CAPTIVE_HEAD[] = R"CHHEAD(
<meta name="theme-color" content="#0B041A">
<style>
:root{--bg:#0B041A;--sf:rgba(21,10,46,.94);--in:#0D0522;--bd:rgba(167,139,250,.28);--p:#8B5CF6;--pink:#ED106D;--a:#C4B5FD;--t:#F4F1FA;--g:linear-gradient(135deg,#7C3AED,#ED106D)}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:
  radial-gradient(ellipse 70% 42% at 80% -8%,rgba(237,16,109,.22),transparent 58%),
  radial-gradient(ellipse 60% 40% at 10% 0,rgba(124,58,237,.28),transparent 55%),
  var(--bg)!important;color:var(--t)!important;
  font-family:-apple-system,BlinkMacSystemFont,system-ui,sans-serif;-webkit-font-smoothing:antialiased}
.wrap{display:flex!important;flex-direction:column;text-align:left!important;max-width:420px!important;min-width:0!important;
  width:calc(100% - 24px)!important;max-height:calc(100dvh - 12px)!important;overflow-x:hidden;overflow-y:auto;
  margin:8px auto!important;padding:16px 16px 14px!important;
  background:var(--sf)!important;border:1px solid var(--bd)!important;border-radius:20px!important;
  box-shadow:0 18px 50px rgba(5,1,16,.45)}
.wrap.ch-has-scan{overflow:hidden}
.ch-logo{margin:0 0 10px;line-height:0;flex:0 0 auto}
.ch-logo svg{width:48px;height:47px;display:block}
h1{font-size:1.28rem!important;font-weight:800!important;letter-spacing:-.03em!important;margin:0 0 4px!important;color:#fff!important;flex:0 0 auto}
h3{color:var(--pink)!important;font-size:.68rem!important;letter-spacing:.16em!important;text-transform:uppercase!important;font-weight:700!important;margin:0 0 10px!important;flex:0 0 auto}
.ch-sub{color:var(--a)!important;font-size:.88rem;line-height:1.45;margin:0 0 14px;flex:0 0 auto}
button,.button{background:var(--g)!important;border:0!important;border-radius:12px!important;color:#fff!important;
  font-weight:700!important;font-size:.92rem!important;line-height:2.5rem!important;width:100%!important;margin:.3rem 0!important;flex:0 0 auto}
button:active{transform:scale(.97)}
form{flex:0 0 auto;margin:0}
hr{border:0;border-top:1px solid var(--bd)!important;margin:10px 0!important}
input:not([type=checkbox]),select{width:100%!important;padding:11px 14px!important;border-radius:12px!important;
  background:var(--in)!important;border:1px solid var(--bd)!important;color:#fff!important;font-size:1rem!important;margin:4px 0 10px!important}
input:focus,select:focus{outline:none!important;border-color:var(--pink)!important;box-shadow:0 0 0 3px rgba(237,16,109,.25)!important}
input[type=checkbox]{width:auto!important;accent-color:#ED106D;margin-right:8px}
label,.msg{color:var(--a)!important}
label{font-size:.7rem!important;letter-spacing:.1em!important;text-transform:uppercase!important;font-weight:700!important}
.msg{background:var(--in)!important;border:1px solid var(--bd)!important;border-left:4px solid var(--pink)!important;
  border-radius:12px!important;color:#fff!important;padding:10px 12px!important;margin:10px 0 0!important;flex:0 0 auto}
a{color:#fff!important;text-decoration:none}
/* Scan rows: keep SSID + lock/signal on ONE line (WiFiManager puts .q as a sibling, not inside the <a>) */
.wrap>div:has(>a):not(.msg):not(.ch-scan),
.ch-scan>div{display:flex!important;align-items:center;justify-content:space-between;gap:8px;
  background:var(--in);border-bottom:1px solid var(--bd);padding:11px 12px;margin:0}
.wrap>div:has(>a):not(.msg):not(.ch-scan) a,
.ch-scan a{flex:1 1 auto;min-width:0;background:none!important;border:0!important;padding:0!important;margin:0!important;
  color:#fff!important;font-weight:700!important;text-align:left;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ch-scan{flex:1 1 auto;min-height:96px;overflow-y:auto;-webkit-overflow-scrolling:touch;margin:4px 0 10px;
  border:1px solid var(--bd);border-radius:12px;background:rgba(13,5,34,.55)}
.ch-scan>div:last-child{border-bottom:0}
.q{float:none!important;flex:0 0 auto!important;display:flex!important;align-items:center;height:16px;min-width:40px;margin:0!important;padding:0 0 0 6px!important}
.q:after,.q:before,.q[role=img]{-webkit-filter:invert(1)!important;filter:invert(1)!important;opacity:1!important}
dt{color:var(--pink)!important;font-size:.68rem;letter-spacing:.12em;text-transform:uppercase;margin:12px 0 2px}
dd{margin:0;color:#fff;font-size:.95rem}
</style>
)CHHEAD";

#ifndef CH_UNBRANDED
static const char CH_CAPTIVE_LOGO_BOOT[] = R"CHLOGO(
<template id="ch-logo"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 154 150" width="48" height="47" fill="none" aria-hidden="true">
<path d="M149.534 107.505L135.041 93.0124C131.729 88.0435 129.865 81.8323 129.865 75.207C129.865 67.7536 132.35 60.7143 136.284 55.5383C137.733 53.4679 139.182 51.3975 140.217 49.1201C142.495 44.7722 143.737 39.5963 143.737 34.2132C143.737 20.5486 135.87 9.16147 125.104 6.05588C123.24 5.43476 121.17 5.22772 118.892 5.22772C105.228 5.22772 94.0476 18.2712 94.0476 34.4203C94.0476 40.0103 95.4969 45.3934 97.7743 49.9482C98.8095 52.0186 100.052 54.089 101.501 55.7453C105.435 60.9213 107.919 67.7536 107.919 75.207C107.919 82.0393 106.056 88.2505 102.743 93.0124L88.2505 107.298C83.4886 112.06 83.4886 119.928 88.2505 124.689L110.197 146.429C114.959 151.19 122.826 151.19 127.795 146.429L149.741 124.689C154.296 120.135 154.296 112.267 149.534 107.505Z" fill="white"/>
<path d="M49.9482 92.8054C46.8427 87.8365 44.9793 81.8323 44.9793 75.2071C44.9793 68.1677 47.0497 61.9565 50.3623 56.9876L64.441 42.9089C69.2029 38.147 69.2029 30.2795 64.441 25.5176L42.7019 3.57143C37.94 -1.19048 30.0725 -1.19048 25.3106 3.57143L3.57143 25.3106C-1.19048 30.0725 -1.19048 37.94 3.57143 42.7019L17.6501 56.7805C20.9627 61.7495 23.0331 68.1677 23.0331 75C23.0331 81.6253 21.1698 87.8365 18.0642 92.5984L3.57143 107.091C-1.19048 111.853 -1.19048 119.721 3.57143 124.482L25.3106 146.222C30.0725 150.983 37.94 150.983 42.7019 146.222L64.441 124.482C69.2029 119.721 69.2029 111.853 64.441 107.091L49.9482 92.8054Z" fill="white"/>
<path d="M88.0435 42.7019C83.2816 37.94 83.2816 30.0725 88.0435 25.3106L109.99 3.57143C114.752 -1.19048 122.619 -1.19048 127.588 3.57143L149.534 25.3106C154.296 30.0725 154.296 37.94 149.534 42.7019L127.588 64.441C122.826 69.2029 114.959 69.2029 109.99 64.441L88.0435 42.7019Z" fill="#ED106D"/>
</svg></template>
)CHLOGO";
#endif

static const char CH_CAPTIVE_BOOT[] = R"CHBOOT(
<script>
document.addEventListener('DOMContentLoaded',function(){
var w=document.querySelector('.wrap');if(!w)return;
var t=document.getElementById('ch-logo'),h=w.querySelector('h1');
if(t&&h&&!w.querySelector('.ch-logo')){
  var d=document.createElement('div');d.className='ch-logo';
  d.appendChild(t.content.cloneNode(true));
  h.parentNode.insertBefore(d,h);
}
var meta=document.querySelector('meta[name="ch-mdns"]');
var host=meta&&meta.getAttribute('content');
var body=(document.body.innerText||'');
if(host&&/saved|trying to connect|credentials/i.test(body)&&!w.querySelector('.ch-next')){
  var n=document.createElement('p');n.className='ch-sub ch-next';
  n.innerHTML='This setup Wi-Fi will close. On your home network open <b>http://'+host+'.local</b> — the miner also appears with that name on your router.';
  w.appendChild(n);
}
var items=[],ch=w.children,i,el;
for(i=0;i<ch.length;i++){
  el=ch[i];
  if(el.tagName==='DIV'&&el.querySelector&&el.querySelector('a[onclick],a[href="#p"]'))items.push(el);
}
if(items.length){
  var box=document.createElement('div');box.className='ch-scan';
  items[0].parentNode.insertBefore(box,items[0]);
  for(i=0;i<items.length;i++)box.appendChild(items[i]);
  w.className+=(w.className?' ':'')+'ch-has-scan';
}
var fs=w.querySelectorAll('form');
for(i=0;i<fs.length;i++){
  if((fs[i].getAttribute('action')||'').indexOf('wifi')<0)continue;
  fs[i].addEventListener('submit',function(){
    var b=this.querySelector('button');
    if(b){b.textContent='Scanning networks…';b.disabled=true;}
  });
}
});
</script>
)CHBOOT";

static WiFiManager* s_captiveWm = nullptr;
static char s_captiveHead[10240];
static char s_captiveMenu[720];
static char s_apTitle[33];

inline void ch_send_captive_info() {
  String html;
  html.reserve(2800);
  html += F("<!DOCTYPE html><html lang='en-US'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += s_captiveHead[0] ? s_captiveHead : CH_CAPTIVE_HEAD;
  html += F("</head><body><div class='wrap'><h1>");
  html += WiFi.softAPSSID();
  html += F("</h1><h3>About</h3><dl>");
  html += F("<dt>Product</dt><dd>");
  html += CH_PRODUCT_NAME;
  html += F("</dd><dt>Firmware</dt><dd>" CH_VERSION "</dd>");
  html += F("<dt>Hardware</dt><dd>" CH_HARDWARE "</dd>");
  html += F("<dt>Setup AP</dt><dd>");
  html += WiFi.softAPSSID();
  html += F("</dd><dt>AP IP</dt><dd>");
  html += WiFi.softAPIP().toString();
  html += F("</dd><dt>Chip</dt><dd>");
  {
    uint64_t mac = ESP.getEfuseMac();
    char chip[13];
    snprintf(chip, sizeof(chip), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
    html += chip;
  }
  html += F("</dd><dt>Free heap</dt><dd>");
  html += String(ESP.getFreeHeap());
  html += F("</dd><dt>Dashboard after Wi-Fi</dt><dd>http://");
  html += ch_mdns_hostname();
  html += F(".local</dd>");
#ifndef CH_UNBRANDED
  html += F("<dt>License</dt><dd>GPL-3.0 (code). Brand assets remain Cripto Host.</dd>");
  html += F("<dt>Upstream</dt><dd>Fork of NerdMiner_v2 by BitMaker, GPL-3.0.</dd>");
#else
  html += F("<dt>License</dt><dd>GPL-3.0</dd>");
  html += F("<dt>Upstream</dt><dd>Fork of NerdMiner_v2 (BitMaker).</dd>");
#endif
  html += F("</dl><p class='ch-sub'>Pool, wallet and timezone ship with factory defaults. "
            "Change them in the dashboard Config page after Wi-Fi is up. "
            "Firmware updates use the OTA page there — not this setup portal.</p>");
  html += F("<form action='/' method='get'><button type='submit'>Back</button></form>");
  html += F("</div></body></html>");
  s_captiveWm->server->send(200, "text/html", html);
}

inline void ch_on_captive_server() {
  s_captiveWm->server->on("/info", ch_send_captive_info);
}

inline void ch_apply_captive_theme(WiFiManager& wm) {
  s_captiveWm = &wm;
  String host = ch_mdns_hostname();
#ifndef CH_UNBRANDED
  snprintf(s_captiveHead, sizeof(s_captiveHead),
           "<meta name=\"ch-mdns\" content=\"%s\">%s%s%s",
           host.c_str(), CH_CAPTIVE_HEAD, CH_CAPTIVE_LOGO_BOOT, CH_CAPTIVE_BOOT);
#else
  snprintf(s_captiveHead, sizeof(s_captiveHead),
           "<meta name=\"ch-mdns\" content=\"%s\">%s%s",
           host.c_str(), CH_CAPTIVE_HEAD, CH_CAPTIVE_BOOT);
#endif
  strncpy(s_apTitle, ch_ap_ssid().c_str(), sizeof(s_apTitle) - 1);
  s_apTitle[sizeof(s_apTitle) - 1] = '\0';
  wm.setTitle(s_apTitle);
  wm.setCustomHeadElement(s_captiveHead);
  wm.setShowInfoUpdate(false);

  wm._preloadwifiscan = true;
  wm._asyncScan = true;
  wm._scancachetime = 45000;

  const char* menu[] = {"custom", "wifi", "info", "exit"};
  wm.setMenu(menu, 4);

#ifndef CH_UNBRANDED
  snprintf(s_captiveMenu, sizeof(s_captiveMenu),
           "<p class='ch-sub'>Connect this miner to Wi-Fi. This setup network then closes — "
           "on your home Wi-Fi open <b>http://%s.local</b> (same name on your router). "
           "Pool, wallet and timezone already use Cripto Host defaults.</p>",
           host.c_str());
#else
  snprintf(s_captiveMenu, sizeof(s_captiveMenu),
           "<p class='ch-sub'>Connect this miner to Wi-Fi. Afterwards open "
           "<b>http://%s.local</b> on your home network.</p>",
           host.c_str());
#endif
  wm.setCustomMenuHTML(s_captiveMenu);

  wm.setWebServerCallback(ch_on_captive_server);
}

#endif // CH_CAPTIVE_H
