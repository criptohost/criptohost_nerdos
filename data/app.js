/* CriptoHost NerdOS — app.js (todas as páginas; roteia por body[data-page]) */
(function () {
  "use strict";
  var page = document.body.dataset.page;
  var $ = function (id) { return document.getElementById(id); };

  function fmtUptime(s) {
    var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600),
        m = Math.floor(s % 3600 / 60), ss = s % 60;
    return d + "d " + String(h).padStart(2, "0") + ":" + String(m).padStart(2, "0") + ":" + String(ss).padStart(2, "0");
  }

  function setWorker(st) { var el = $("worker"); if (el) el.textContent = st.worker + " · " + st.fw; }

  // ---------- HOME ----------
  function renderStatus(st) {
    setWorker(st);
    $("hashrate").textContent = st.hashrate_khs.toFixed(1);
    // gauge: 400 kH/s = 100% (teto DevKit V1 com HW SHA)
    $("gauge").style.width = Math.min(100, st.hashrate_khs / 400 * 100) + "%";
    $("status-line").textContent = st.status + " · " + st.hardware + " · " + st.ip;
    $("sh-found").textContent = st.shares.found;
    $("sh-sent").textContent = st.shares.sent;
    $("sh-accepted").textContent = st.shares.accepted;
    $("sh-rejected").textContent = st.shares.rejected;
    $("sh-pending").textContent = st.shares.pending;
    var tot = st.shares.accepted + st.shares.rejected;
    $("sh-eff").textContent = tot ? (st.shares.accepted / tot * 100).toFixed(1) + "%" : "—";
    $("bestdiff").textContent = st.best_difficulty;
    $("templates").textContent = st.templates;
    $("valids").textContent = st.valid_blocks;
    $("uptime").textContent = fmtUptime(st.uptime_s);
    $("temp").textContent = st.temp_c.toFixed(1) + " °C";
    $("rssi").textContent = st.rssi_dbm + " dBm";
    $("pool").textContent = st.pool;
  }

  function renderEvents(evs) {
    var log = $("log");
    log.innerHTML = evs.map(function (e) {
      return '<div class="' + e.type + '">[' + fmtUptime(e.t) + "] " + e.msg + "</div>";
    }).join("");
  }

  function initHome() {
    function poll() {
      fetch("/api/status").then(function (r) { return r.json(); }).then(renderStatus).catch(function () {});
      fetch("/api/events").then(function (r) { return r.json(); }).then(renderEvents).catch(function () {});
    }
    // WebSocket com fallback para polling 5s
    try {
      var ws = new WebSocket("ws://" + location.host + "/ws");
      ws.onmessage = function (m) {
        var d = JSON.parse(m.data);
        if (d.status) { renderStatus(d.status); renderEvents(d.events || []); }
        else renderStatus(d);
      };
      ws.onerror = ws.onclose = function () { setInterval(poll, 5000); };
    } catch (e) { setInterval(poll, 5000); }
    poll();
    marketCard();
  }

  // Preços via CoinGecko direto do browser (poupa heap do ESP32), cache 5 min
  function marketCard() {
    var KEY = "ch-market", TTL = 5 * 60 * 1000;
    function render(d, ts) {
      $("px-btc").textContent = "$" + d.bitcoin.usd.toLocaleString();
      $("px-dgb").textContent = "$" + d.digibyte.usd.toFixed(5);
      $("px-xec").textContent = "$" + d.ecash.usd.toFixed(6);
      $("mkt-age").textContent = "· updated " + Math.round((Date.now() - ts) / 60000) + "m ago";
    }
    var c = null;
    try { c = JSON.parse(localStorage.getItem(KEY)); } catch (e) {}
    if (c && Date.now() - c.ts < TTL) { render(c.d, c.ts); return; }
    fetch("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,digibyte,ecash&vs_currencies=usd")
      .then(function (r) { return r.json(); })
      .then(function (d) {
        localStorage.setItem(KEY, JSON.stringify({ d: d, ts: Date.now() }));
        render(d, Date.now());
      })
      .catch(function () { if (c) render(c.d, c.ts); });
    setTimeout(marketCard, TTL);
  }

  // ---------- FLEET ----------
  function initFleet() {
    function card(st, ip) {
      var eff = st.shares.accepted + st.shares.rejected;
      return '<div class="ch-card ch-fleet-card">' +
        "<h3>" + st.worker + ' <span class="' + (st.status === "mining" ? "ch-badge--mining" : "ch-badge--offline") + '">● ' + st.status + "</span></h3>" +
        '<div class="ch-kv"><span>Hashrate</span><b>' + st.hashrate_khs.toFixed(1) + " kH/s</b></div>" +
        '<div class="ch-kv"><span>Temp</span><b>' + st.temp_c.toFixed(0) + " °C</b></div>" +
        '<div class="ch-kv"><span>Wi-Fi</span><b>' + st.rssi_dbm + " dBm</b></div>" +
        '<div class="ch-kv"><span>Pool</span><b style="font-size:0.7rem">' + st.pool + "</b></div>" +
        '<div class="ch-kv"><span>Firmware</span><b>' + st.fw + " · " + st.hardware + "</b></div>" +
        '<div class="ch-actions">' +
        '<button class="ch-btn ch-btn--ghost" data-act="identify" data-ip="' + ip + '">Identify</button>' +
        '<button class="ch-btn ch-btn--ghost" data-act="restart" data-ip="' + ip + '">Restart</button>' +
        '<a class="ch-btn ch-btn--ghost" href="http://' + ip + '/config.html">Config</a>' +
        '<a class="ch-btn ch-btn--ghost" href="http://' + ip + '/ota.html">OTA</a>' +
        "</div></div>";
    }

    function refresh() {
      fetch("/api/fleet").then(function (r) { return r.json(); }).then(function (fleet) {
        setWorker(fleet.self);
        var nodes = [{ st: fleet.self, ip: location.host }];
        var peers = fleet.peers.filter(function (p) { return p.worker !== fleet.self.worker; });
        Promise.all(peers.map(function (p) {
          return fetch("http://" + p.ip + ":" + p.port + "/api/status", { signal: AbortSignal.timeout(4000) })
            .then(function (r) { return r.json(); })
            .then(function (st) { return { st: st, ip: p.ip }; })
            .catch(function () { return null; });
        })).then(function (res) {
          res.filter(Boolean).forEach(function (n) { nodes.push(n); });
          $("fleet-cards").innerHTML = nodes.map(function (n) { return card(n.st, n.ip); }).join("");
          var mining = nodes.filter(function (n) { return n.st.status === "mining"; });
          $("agg-online").textContent = nodes.length;
          $("agg-hash").textContent = nodes.reduce(function (a, n) { return a + n.st.hashrate_khs; }, 0).toFixed(1);
          $("agg-temp").textContent = nodes.length
            ? (nodes.reduce(function (a, n) { return a + n.st.temp_c; }, 0) / nodes.length).toFixed(0) : "—";
        });
      }).catch(function () {});
    }

    document.addEventListener("click", function (ev) {
      var b = ev.target.closest("[data-act]");
      if (!b) return;
      var act = b.dataset.act, ip = b.dataset.ip;
      if (act === "restart" && !confirm("Reiniciar " + ip + "?")) return;
      fetch("http://" + ip.replace(/:.*/, "") + "/api/" + act, { method: "POST" }).catch(function () {});
    });

    refresh();
    setInterval(refresh, 10000);
  }

  // ---------- CONFIG ----------
  function initConfig() {
    fetch("/api/config").then(function (r) { return r.json(); }).then(function (c) {
      $("pool").value = c.pool;
      $("port").value = c.port;
      $("wallet").value = c.wallet;
      $("password").value = c.password;
      var w = $("worker"); if (w) w.textContent = c.hardware + " · " + c.fw;
    });

    $("profile").addEventListener("change", function () {
      if (!this.value) return;
      var p = this.value.split("|");
      $("pool").value = p[0];
      $("port").value = p[1];
    });

    $("cfg-form").addEventListener("submit", function (ev) {
      ev.preventDefault();
      $("cfg-msg").textContent = "Salvando…";
      fetch("/api/config", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          pool: $("pool").value.trim(),
          port: +$("port").value,
          wallet: $("wallet").value.trim(),
          password: $("password").value
        })
      }).then(function (r) { return r.json(); }).then(function (d) {
        $("cfg-msg").textContent = d.ok ? "Salvo. Reiniciando — aguarde ~20 s e recarregue." : (d.error || "erro");
      }).catch(function () { $("cfg-msg").textContent = "Falha ao salvar."; });
    });

    $("btn-identify").addEventListener("click", function () { fetch("/api/identify", { method: "POST" }); });
    $("btn-restart").addEventListener("click", function () {
      if (confirm("Reiniciar o dispositivo?")) fetch("/api/restart", { method: "POST" });
    });
    $("btn-factory").addEventListener("click", function () {
      // confirmação dupla (M2-08)
      if (!confirm("Factory reset: apaga Wi-Fi e configuração. Continuar?")) return;
      if (!confirm("Tem certeza? O dispositivo voltará ao portal CriptoHostAP.")) return;
      fetch("/api/factory-reset", { method: "POST" });
    });
  }

  // ---------- OTA ----------
  function initOta() {
    fetch("/api/config").then(function (r) { return r.json(); }).then(function (c) {
      $("fw-hw").textContent = c.hardware;
      $("fw-cur").textContent = c.fw;
      var w = $("worker"); if (w) w.textContent = c.hardware + " · " + c.fw;
    });

    $("ota-form").addEventListener("submit", function (ev) {
      ev.preventDefault();
      var f = $("ota-file").files[0];
      if (!f) return;
      if (!/\.bin$/i.test(f.name)) { $("ota-msg").textContent = "Selecione um arquivo .bin"; return; }
      $("ota-btn").disabled = true;
      $("ota-msg").textContent = "Enviando… NÃO desligue o dispositivo.";
      var fd = new FormData();
      fd.append("firmware", f, f.name);
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "/api/ota");
      xhr.upload.onprogress = function (e) {
        if (e.lengthComputable) {
          var pct = Math.round(e.loaded / e.total * 100);
          $("ota-bar").style.width = pct + "%";
          $("ota-msg").textContent = "Enviando… " + pct + "%";
        }
      };
      xhr.onload = function () {
        if (xhr.status === 200) {
          $("ota-bar").style.width = "100%";
          $("ota-msg").textContent = "✅ Update aplicado. Reiniciando — recarregue em ~30 s.";
          setTimeout(function () { location.href = "/"; }, 30000);
        } else {
          var msg = "Falha no update";
          try { msg = JSON.parse(xhr.responseText).error || msg; } catch (e) {}
          $("ota-msg").textContent = "❌ " + msg + " — dispositivo segue no firmware atual.";
          $("ota-btn").disabled = false;
        }
      };
      xhr.onerror = function () {
        $("ota-msg").textContent = "❌ Conexão perdida durante o envio. Tente novamente.";
        $("ota-btn").disabled = false;
      };
      xhr.send(fd);
    });
  }

  if (page === "home") initHome();
  else if (page === "fleet") initFleet();
  else if (page === "config") initConfig();
  else if (page === "ota") initOta();
})();
