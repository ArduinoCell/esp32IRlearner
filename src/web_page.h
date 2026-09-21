#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-U IR Learner</title>
<style>
  :root{--bg:#0f172a;--card:#1e293b;--border:#334155;--text:#f1f5f9;--muted:#94a3b8;--accent:#38bdf8;--accent2:#0284c7;--green:#22c55e;--red:#ef4444;--code:#090e1a}
  *{box-sizing:border-box;margin:0;padding:0;font-family:system-ui,-apple-system,sans-serif}
  body{background:var(--bg);color:var(--text);min-height:100vh;padding:20px}
  h1{font-size:1.5rem;color:var(--accent);margin-bottom:4px}
  .sub{color:var(--muted);font-size:.9rem;margin-bottom:24px}
  .grid{display:grid;gap:16px;grid-template-columns:1fr}
  @media(min-width:700px){.grid{grid-template-columns:1fr 1fr}}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:18px}
  .card h2{font-size:1rem;color:var(--muted);text-transform:uppercase;letter-spacing:.05em;margin-bottom:14px}
  .row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid var(--border);font-size:.9rem}
  .row:last-child{border-bottom:none}
  .badge{padding:2px 10px;border-radius:20px;font-size:.78rem;font-weight:600}
  .ok{background:rgba(34,197,94,.15);color:var(--green)}
  .err{background:rgba(239,68,68,.15);color:var(--red)}
  label{display:block;font-size:.82rem;color:var(--muted);margin-bottom:4px;margin-top:12px}
  input{width:100%;padding:9px 12px;background:var(--code);border:1px solid var(--border);border-radius:7px;color:var(--text);font-size:.9rem;outline:none}
  input:focus{border-color:var(--accent)}
  .btn{display:block;width:100%;margin-top:14px;padding:10px;background:var(--accent);color:#0f172a;font-weight:700;font-size:.95rem;border:none;border-radius:8px;cursor:pointer;transition:background .2s}
  .btn:hover{background:var(--accent2);color:#fff}
  .btn.secondary{background:transparent;color:var(--accent);border:1px solid var(--accent)}
  .btn.secondary:hover{background:var(--accent);color:#0f172a}
  .pronto{background:var(--code);border:1px solid var(--border);border-radius:8px;padding:12px;font-family:monospace;font-size:.78rem;color:#7dd3fc;word-break:break-all;min-height:52px;line-height:1.6}
  .detail{font-size:.85rem;margin:4px 0}
  .detail span{color:var(--accent);font-weight:600}
  .log{background:var(--code);border:1px solid var(--border);border-radius:8px;padding:12px;font-family:monospace;font-size:.8rem;color:#a5f3fc;max-height:220px;overflow-y:auto;white-space:pre-wrap;word-break:break-all}
  .pulse{display:inline-block;width:10px;height:10px;border-radius:50%;background:var(--green);margin-right:6px;animation:pulse 1.4s infinite}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
  #toast{position:fixed;bottom:24px;right:24px;background:#1e293b;border:1px solid var(--accent);color:var(--text);padding:10px 18px;border-radius:8px;font-size:.9rem;display:none;z-index:999}
</style>
</head>
<body>
<h1>&#128225; ESP32-U IR Learner</h1>
<p class="sub">Captures IR remote signals &amp; sends them to Homey Pro 2023</p>

<div class="grid">
  <div class="card">
    <h2>System Status</h2>
    <div class="row"><span>Wi-Fi</span><span id="wifiStatus" class="badge ok">--</span></div>
    <div class="row"><span>IP Address</span><span id="ip" style="color:var(--accent)">--</span></div>
    <div class="row"><span>IR Receiver Pin</span><span>GPIO 15</span></div>
    <div class="row"><span>Free Heap</span><span id="heap">-- KB</span></div>
    <div class="row"><span>Signals Captured</span><span id="count">0</span></div>

    <h2 style="margin-top:20px">Homey Configuration</h2>
    <label>Homey IP Address</label>
    <input id="homeyIp" type="text" placeholder="e.g. 192.168.1.50">
    <label>Webhook Event Name</label>
    <input id="webhookTag" type="text" placeholder="ir_learned">
    <button class="btn" onclick="saveConfig()">Save to ESP32</button>
  </div>

  <div class="card">
    <h2>Last Captured IR Signal</h2>
    <div id="noSignal" style="color:var(--muted);font-size:.9rem;padding:8px 0">
      <span class="pulse"></span>Waiting for IR remote button press...
    </div>
    <div id="signalInfo" style="display:none">
      <div class="detail">Protocol: <span id="proto">--</span></div>
      <div class="detail">Bits: <span id="bits">--</span></div>
      <div class="detail">Hex Code: <span id="hexCode">--</span></div>
      <div class="detail" style="margin-top:10px">Pronto HEX (paste into Homey):</div>
      <div class="pronto" id="pronto">--</div>
      <button class="btn" style="margin-top:12px" onclick="sendToHomey()">&#9889; Send Now to Homey</button>
      <button class="btn secondary" onclick="copyPronto()">Copy Pronto HEX</button>
    </div>
  </div>
</div>

<div class="card" style="margin-top:16px">
  <h2>Signal Log</h2>
  <div class="log" id="log">Waiting for captures...</div>
</div>

<div id="toast"></div>

<script>
  let captureCount = 0;
  let lastSignalKey = "";

  function toast(msg) {
    const el = document.getElementById('toast');
    el.innerText = msg;
    el.style.display = 'block';
    setTimeout(() => el.style.display = 'none', 3000);
  }

  function fetchStatus() {
    fetch('/api/status').then(r => r.json()).then(d => {
      document.getElementById('ip').innerText = d.ip;
      document.getElementById('heap').innerText = Math.round(d.free_heap / 1024) + ' KB';
      document.getElementById('homeyIp').value = d.homey_ip || '';
      document.getElementById('webhookTag').value = d.webhook_tag || 'ir_learned';
      document.getElementById('wifiStatus').innerText = 'Connected';
    }).catch(() => {
      document.getElementById('wifiStatus').className = 'badge err';
      document.getElementById('wifiStatus').innerText = 'Offline';
    });
  }

  function pollSignal() {
    fetch('/api/last-signal').then(r => r.json()).then(d => {
      if (!d.valid) return;
      const key = d.hex_code + d.proto;
      if (key === lastSignalKey) return;
      lastSignalKey = key;
      captureCount++;
      document.getElementById('count').innerText = captureCount;
      document.getElementById('noSignal').style.display = 'none';
      document.getElementById('signalInfo').style.display = 'block';
      document.getElementById('proto').innerText = d.protocol;
      document.getElementById('bits').innerText = d.bits;
      document.getElementById('hexCode').innerText = d.hex_code;
      document.getElementById('pronto').innerText = d.pronto;
      const log = document.getElementById('log');
      log.innerText = '[' + new Date().toLocaleTimeString() + '] ' + d.protocol
        + ' | ' + d.hex_code + ' | ' + d.bits + ' bits\n' + d.pronto + '\n\n' + log.innerText;
    }).catch(console.error);
  }

  function saveConfig() {
    const ip = document.getElementById('homeyIp').value;
    const tag = document.getElementById('webhookTag').value;
    fetch('/api/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'homey_ip=' + encodeURIComponent(ip) + '&webhook_tag=' + encodeURIComponent(tag)
    }).then(() => toast('Settings saved!')).catch(() => toast('Save failed!'));
  }

  function sendToHomey() {
    fetch('/api/trigger-homey', {method:'POST'})
      .then(r => r.text()).then(msg => toast(msg)).catch(() => toast('Send failed!'));
  }

  function copyPronto() {
    const text = document.getElementById('pronto').innerText;
    navigator.clipboard.writeText(text).then(() => toast('Pronto HEX copied!')).catch(() => toast('Copy failed'));
  }

  fetchStatus();
  setInterval(pollSignal, 1200);
  setInterval(fetchStatus, 10000);
</script>
</body>
</html>
)rawliteral";
