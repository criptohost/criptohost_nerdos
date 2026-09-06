#!/usr/bin/env node
// Gera docs/screenshots/*.png (desktop 1440 e mobile 390, 2x) de um dashboard servido — placa real ou
// tools/mock/mock_server.py com PROXY=<ip>. Usa o Chrome via DevTools Protocol e só captura quando a
// página terminou de carregar dados (badge "Mining", "Scan complete" no Fleet, etc.).
// Uso: node tools/screenshots.js [http://localhost:8091] [docs/screenshots]
const { spawn } = require("child_process");
const fs = require("fs");
const http = require("http");

const BASE = process.argv[2] || "http://localhost:8091";
const OUT = process.argv[3] || "docs/screenshots";
const CHROME = process.env.CHROME || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const PAGES = [
  ["home", "/", "!document.body.innerText.includes('connecting') && /[1-9]\\d\\d\\.\\d kH\\/s/.test(document.body.innerText)"],
  ["fleet", "/fleet.html", "document.body.innerText.includes('Scan complete')"],
  ["config", "/config.html", "!document.body.innerText.includes('…') && document.getElementById('pool').value.length > 3"],
  ["ota", "/ota.html", "/current: v\\d/i.test(document.body.innerText) && /\\bMining\\b/.test(document.body.innerText)"],
];
const SIZES = [["desktop", 1440], ["mobile", 390]];

const getJson = (url) => new Promise((res, rej) => http.get(url, (r) => { let b = ""; r.on("data", (d) => b += d); r.on("end", () => res(JSON.parse(b))); }).on("error", rej));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const profile = fs.mkdtempSync("/tmp/ch-shots-");
  const chrome = spawn(CHROME, ["--headless=new", "--disable-gpu", "--hide-scrollbars", "--remote-debugging-port=9333", `--user-data-dir=${profile}`, "about:blank"], { stdio: "ignore" });
  let targets = [];
  for (let i = 0; i < 30 && !targets.length; i++) { await sleep(500); try { targets = (await getJson("http://127.0.0.1:9333/json/list")).filter((t) => t.type === "page"); } catch (e) {} }
  const ws = new WebSocket(targets[0].webSocketDebuggerUrl);
  await new Promise((r) => (ws.onopen = r));
  let id = 0; const pending = {};
  ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending[d.id]) { pending[d.id](d); delete pending[d.id]; } };
  const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending[i] = r; ws.send(JSON.stringify({ id: i, method, params })); });
  const evaluate = async (expr) => (await send("Runtime.evaluate", { expression: expr, returnByValue: true })).result?.result?.value;

  await send("Page.enable"); await send("Runtime.enable");
  fs.mkdirSync(OUT, { recursive: true });
  for (const [name, path, ready] of PAGES) for (const [label, width] of SIZES) {
    await send("Emulation.setDeviceMetricsOverride", { width, height: 900, deviceScaleFactor: 2, mobile: width < 768 });
    await send("Page.navigate", { url: BASE + path });
    let ok = false;
    for (let i = 0; i < 90 && !ok; i++) { await sleep(1000); ok = !!(await evaluate(ready)); }
    await sleep(name === "home" ? 25000 : 1500);  // Home: acumula amostras do gráfico de temperatura
    const height = Math.ceil(await evaluate("document.documentElement.scrollHeight"));
    await send("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor: 2, mobile: width < 768 });
    await sleep(500);
    const shot = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true });
    const file = `${OUT}/esp32-${name}-${label}.png`;
    fs.writeFileSync(file, Buffer.from(shot.result.data, "base64"));
    console.log(`${ok ? "ok " : "TIMEOUT"} ${file} ${width}x${height}`);
  }
  ws.close(); chrome.kill();
})().catch((e) => { console.error(e); process.exit(1); });
