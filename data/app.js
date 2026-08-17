/* CriptoHost NerdOS — app.js (todas as páginas; roteia por body[data-page]) */
(function () {
  "use strict";
  var page = document.body.dataset.page;
  var $ = function (id) { return document.getElementById(id); };
  var set = function (id, v) { var el = $(id); if (el) el.textContent = v; };

  function fmtUptime(s) {
    var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600),
        m = Math.floor(s % 3600 / 60), ss = s % 60;
    return d + "d " + String(h).padStart(2, "0") + ":" + String(m).padStart(2, "0") + ":" + String(ss).padStart(2, "0");
  }

  function statusPill(status) {
    var pill = $("statuspill");
    if (!pill) return;
    pill.textContent = status === "mining" ? "Mining" : status;
    pill.className = "ch-pill " + (status === "mining" ? "ch-pill--mining" : "ch-pill--offline");
  }

  // Nav "Pool" → site da pool configurada
  function poolLink(pool) {
    var a = $("pool-link");
    if (a && pool) a.href = "https://" + String(pool).split(":")[0];
  }

  function tempClass(t) { return t < 65 ? "temp-ok" : t < 85 ? "temp-warn" : "temp-hot"; }

  // ---------- HOME ----------
  var tempHist = [];        // sparkline: últimos ~40 pontos (≈3 min)
  var lastAccepted = -1, lastAcceptedAt = null;

  function renderStatus(st) {
    statusPill(st.status);
    poolLink(st.pool);
    set("hashrate", st.hashrate_khs.toFixed(1));

    // anel: 400 kH/s = volta completa (teto DevKit V1 com HW SHA)
    var ring = $("ring");
    if (ring) {
      var C = 452.4, frac = Math.min(1, st.hashrate_khs / 400);
      ring.style.strokeDashoffset = (C * (1 - frac)).toFixed(1);
    }

    var pct = $("ring-pct");
    if (pct) pct.textContent = Math.round(Math.min(1, st.hashrate_khs / 400) * 100) + "%";

    var line = $("status-line");
    if (line) {
      line.textContent = st.status + " · " + st.hardware;
      line.className = "ch-status-line " + (st.status === "mining" ? "ch-badge--mining" : "ch-badge--offline");
    }

    set("sh-accepted", st.shares.accepted);
    var tot = st.shares.accepted + st.shares.rejected;
    set("sh-eff", tot ? (st.shares.accepted / tot * 100).toFixed(1) + "%" : "—");

    // last share: cronômetro desde a última mudança em accepted
    if (st.shares.accepted !== lastAccepted) {
      if (lastAccepted >= 0) lastAcceptedAt = Date.now();
      lastAccepted = st.shares.accepted;
    }
    tickLastShare();

    // stream
    set("st-found", st.shares.found);
    set("st-sent", st.shares.sent);
    set("st-pending", st.shares.pending);
    set("st-accepted", st.shares.accepted);
    set("st-rejected", st.shares.rejected);

    // wi-fi
    set("rssi", st.rssi_dbm);
    var q = st.rssi_dbm >= -50 ? 5 : st.rssi_dbm >= -60 ? 4 : st.rssi_dbm >= -67 ? 3 : st.rssi_dbm >= -75 ? 2 : 1;
    set("wifi-q", ["", "Weak", "Fair", "OK", "Good", "Excellent"][q]);
    var bars = $("wifibars");
    if (bars) [].forEach.call(bars.children, function (b, i) { b.className = i < q ? "on" : ""; });

    // temperatura + sparkline
    set("temp", st.temp_c.toFixed(1));
    tempHist.push(st.temp_c);
    if (tempHist.length > 40) tempHist.shift();
    var sp = $("spark-line");
    if (sp && tempHist.length > 1) {
      var mn = Math.min.apply(null, tempHist) - 1, mx = Math.max.apply(null, tempHist) + 1;
      sp.setAttribute("points", tempHist.map(function (t, i) {
        return (i / (tempHist.length - 1) * 200).toFixed(1) + "," + (38 - (t - mn) / (mx - mn) * 36).toFixed(1);
      }).join(" "));
    }

    set("uptime", fmtUptime(st.uptime_s));
    set("bestdiff", st.best_difficulty);
    set("templates", st.templates);
    set("valids", st.valid_blocks);
    set("pool-conn", st.status === "mining" ? "Mining" : st.status);
    set("fw", st.fw);

    set("w-worker", st.worker);
    set("w-pool", st.pool);
    set("w-hw", st.hardware);
    set("w-ip", st.ip);
    set("w-status", st.status);
  }

  function renderEvents(evs) {
    var log = $("log");
    if (log) log.innerHTML = evs.map(function (e) {
      return '<div class="' + e.type + '">[' + fmtUptime(e.t) + "] " + e.msg + "</div>";
    }).join("");

    // stream track: últimos 12 eventos de share como pontos
    var track = $("stream-track");
    if (track) {
      var dots = evs.filter(function (e) { return ["share", "accept", "reject"].indexOf(e.type) >= 0; }).slice(0, 12);
      track.innerHTML = dots.map(function (e, i) {
        var left = 6 + (i / Math.max(1, dots.length - 1)) * 88;
        return '<i class="' + e.type + '" style="left:' + left.toFixed(1) + '%;animation-delay:' + (i * 0.2) + 's"></i>';
      }).join("");
    }
  }

  function tickLastShare() {
    set("last-share", lastAcceptedAt ? Math.round((Date.now() - lastAcceptedAt) / 1000) + " s" : "—");
  }

  function initHome() {
    function poll() {
      fetch("/api/status").then(function (r) { return r.json(); }).then(renderStatus).catch(function () {});
      fetch("/api/events").then(function (r) { return r.json(); }).then(renderEvents).catch(function () {});
    }
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
    wireActions();
    setInterval(tickLastShare, 1000);
    // wallet não faz parte do /api/status (contrato congelado) — vem da config
    fetch("/api/config").then(function (r) { return r.json(); }).then(function (c) {
      var w = String(c.wallet).split(".")[0];
      set("w-wallet", w.length > 14 ? w.slice(0, 8) + "…" + w.slice(-4) : w);
    }).catch(function () {});
  }

  // Preços via CoinGecko direto do browser (poupa heap do ESP32), cache 5 min
  function marketCard() {
    var KEY = "ch-market", TTL = 5 * 60 * 1000;
    function chg(id, v) {
      var el = $(id);
      if (!el || v == null) return;
      el.textContent = (v >= 0 ? "▲ " : "▼ ") + Math.abs(v).toFixed(2) + "%";
      el.className = "chg " + (v >= 0 ? "up" : "down");
    }
    function render(d, ts) {
      set("px-btc", "$" + d.bitcoin.usd.toLocaleString("en-US"));
      set("px-dgb", "$" + d.digibyte.usd.toFixed(5));
      set("px-xec", "$" + d.ecash.usd.toFixed(6));
      chg("chg-btc", d.bitcoin.usd_24h_change);
      chg("chg-dgb", d.digibyte.usd_24h_change);
      chg("chg-xec", d.ecash.usd_24h_change);
      set("mkt-age", "● market updated " + Math.round((Date.now() - ts) / 60000) + "m ago");
    }
    var c = null;
    try { c = JSON.parse(localStorage.getItem(KEY)); } catch (e) {}
    if (c && Date.now() - c.ts < TTL) { render(c.d, c.ts); }
    else {
      fetch("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,digibyte,ecash&vs_currencies=usd&include_24hr_change=true")
        .then(function (r) { return r.json(); })
        .then(function (d) {
          localStorage.setItem(KEY, JSON.stringify({ d: d, ts: Date.now() }));
          render(d, Date.now());
        })
        .catch(function () { if (c) render(c.d, c.ts); });
    }
    setTimeout(marketCard, TTL);
  }

  function wireActions() {
    var bi = $("btn-identify"), br = $("btn-restart"), bf = $("btn-factory");
    if (bi) bi.addEventListener("click", function () { fetch("/api/identify", { method: "POST" }); });
    if (br) br.addEventListener("click", function () {
      if (confirm("Restart this device?")) fetch("/api/restart", { method: "POST" });
    });
    if (bf) bf.addEventListener("click", function () {
      // confirmação dupla (M2-08)
      if (!confirm("Factory reset: this erases Wi-Fi and configuration. Continue?")) return;
      if (!confirm("Are you sure? The device will return to the CriptoHostAP portal.")) return;
      fetch("/api/factory-reset", { method: "POST" });
    });
  }

  // ---------- FLEET ----------
  function initFleet() {
    function card(st, ip) {
      var host = st.worker.toLowerCase().replace(/\./g, "-");
      return '<article class="ch-card ch-devcard">' +
        "<h3>" + st.worker +
        ' <span class="' + (st.status === "mining" ? "ch-badge--mining" : "ch-badge--offline") + '" style="font-size:0.72rem;font-weight:700">● ' +
        (st.status === "mining" ? "Online" : st.status) + "</span></h3>" +
        '<div class="host">' + host + ".local</div>" +
        '<div class="ip">' + st.ip + "</div>" +
        '<div class="ch-devstrip">' +
        "<div><span>Hashrate</span>" + st.hashrate_khs.toFixed(1) + " kH/s</div>" +
        '<div><span>Temp</span><span class="' + tempClass(st.temp_c) + '">' + st.temp_c.toFixed(0) + " °C</span></div>" +
        "<div><span>Wi-Fi</span>" + st.rssi_dbm + " dBm</div>" +
        "<div><span>Status</span>" + st.status + "</div>" +
        "</div>" +
        '<div class="meta"><span>Pool</span><b>' + st.pool + "</b></div>" +
        '<div class="meta"><span>Version</span><b>' + st.fw + " · " + st.hardware + "</b></div>" +
        '<div class="ch-actions">' +
        '<button type="button" class="ch-btn ch-btn--ghost" data-act="identify" data-ip="' + ip + '">Identify</button>' +
        '<a class="ch-btn ch-btn--ghost" href="http://' + ip + '/config.html">Config</a>' +
        '<a class="ch-btn ch-btn--ghost" href="http://' + ip + '/ota.html">OTA</a>' +
        '<button type="button" class="ch-btn ch-btn--danger" data-act="restart" data-ip="' + ip + '">Restart</button>' +
        "</div></article>";
    }

    function refresh() {
      fetch("/api/fleet").then(function (r) { return r.json(); }).then(function (fleet) {
        statusPill(fleet.self.status);
        poolLink(fleet.self.pool);
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
          set("agg-online", nodes.length);
          set("agg-hash", nodes.reduce(function (a, n) { return a + n.st.hashrate_khs; }, 0).toFixed(1));
          set("agg-temp", nodes.length
            ? (nodes.reduce(function (a, n) { return a + n.st.temp_c; }, 0) / nodes.length).toFixed(1) : "—");
          set("scanline", "Scan complete — " + nodes.length + " device(s). Sorted by worker name; new scan every 10 s.");
        });
      }).catch(function () {});
    }

    document.addEventListener("click", function (ev) {
      var b = ev.target.closest("[data-act]");
      if (!b) return;
      var act = b.dataset.act, ip = b.dataset.ip;
      if (act === "restart" && !confirm("Restart " + ip + "?")) return;
      fetch("http://" + ip.replace(/:.*/, "") + "/api/" + act, { method: "POST" }).catch(function () {});
    });
    var rescan = $("btn-rescan");
    if (rescan) rescan.addEventListener("click", function () {
      set("scanline", "Scanning the network via mDNS _criptohost._tcp…");
      refresh();
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
      poolLink(c.pool + ":" + c.port);
    });
    fetch("/api/status").then(function (r) { return r.json(); }).then(function (st) { statusPill(st.status); }).catch(function () {});

    $("profile").addEventListener("change", function () {
      if (!this.value) return;
      var p = this.value.split("|");
      $("pool").value = p[0];
      $("port").value = p[1];
    });

    $("cfg-form").addEventListener("submit", function (ev) {
      ev.preventDefault();
      set("cfg-msg", "Saving…");
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
        set("cfg-msg", d.ok ? "Saved. Restarting — wait ~20 s and reload." : (d.error || "error"));
      }).catch(function () { set("cfg-msg", "Failed to save."); });
    });
  }

  // ---------- OTA ----------
  function initOta() {
    fetch("/api/config").then(function (r) { return r.json(); }).then(function (c) {
      set("fw-hw", c.hardware);
      set("fw-cur", c.fw);
      poolLink(c.pool + ":" + c.port);
    });
    fetch("/api/status").then(function (r) { return r.json(); }).then(function (st) { statusPill(st.status); }).catch(function () {});

    var drop = $("ota-drop"), file = $("ota-file"), fname = $("ota-filename");
    var picked = null;
    function showFile(f) {
      picked = f || null;
      if (fname) fname.textContent = f ? f.name : "Drop the .bin here or click to choose";
    }
    if (file) file.addEventListener("change", function () { showFile(this.files[0]); });
    if (drop) {
      ["dragenter", "dragover"].forEach(function (ev) {
        drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.add("is-drag"); });
      });
      ["dragleave", "drop"].forEach(function (ev) {
        drop.addEventListener(ev, function (e) { e.preventDefault(); drop.classList.remove("is-drag"); });
      });
      drop.addEventListener("drop", function (e) {
        var f = e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0];
        if (!f) return;
        showFile(f);
      });
    }

    $("ota-form").addEventListener("submit", function (ev) {
      ev.preventDefault();
      var f = picked || (file && file.files[0]);
      if (!f) return;
      if (!/\.bin$/i.test(f.name)) { set("ota-msg", "Select a .bin file"); return; }
      $("ota-btn").disabled = true;
      set("ota-msg", "Uploading… Do not power off the device.");
      var fd = new FormData();
      fd.append("firmware", f, f.name);
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "/api/ota");
      xhr.upload.onprogress = function (e) {
        if (e.lengthComputable) {
          var pct = Math.round(e.loaded / e.total * 100);
          $("ota-bar").style.width = pct + "%";
          set("ota-msg", "Uploading… " + pct + "%");
        }
      };
      xhr.onload = function () {
        if (xhr.status === 200) {
          $("ota-bar").style.width = "100%";
          set("ota-msg", "Update applied. Restarting — reload in ~30 s.");
          setTimeout(function () { location.href = "/"; }, 30000);
        } else {
          var msg = "Update failed";
          try { msg = JSON.parse(xhr.responseText).error || msg; } catch (e) {}
          set("ota-msg", msg + " — device remains on the current firmware.");
          $("ota-btn").disabled = false;
        }
      };
      xhr.onerror = function () {
        set("ota-msg", "Connection lost during upload. Try again.");
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
