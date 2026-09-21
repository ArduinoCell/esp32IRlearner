#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-U IR Learner</title>
<style>
  :root{--bg:#0f172a;--card:#1e293b;--border:#334155;--text:#f1f5f9;--muted:#94a3b8;--accent:#38bdf8;--accent2:#0284c7;--green:#22c55e;--red:#ef4444;--yellow:#fbbf24;--code:#090e1a}
  *{box-sizing:border-box;margin:0;padding:0;font-family:system-ui,-apple-system,sans-serif}
  body{background:var(--bg);color:var(--text);min-height:100vh;padding:16px 20px 40px}
  .topbar{display:flex;align-items:center;justify-content:space-between;margin-bottom:6px}
  h1{font-size:1.4rem;color:var(--accent)}
  .sub{color:var(--muted);font-size:.85rem;margin-bottom:20px}
  /* ─ Tabs ─ */
  .tabs{display:flex;gap:4px;margin-bottom:16px;border-bottom:1px solid var(--border);padding-bottom:0}
  .tab{padding:8px 16px;border-radius:8px 8px 0 0;cursor:pointer;font-size:.9rem;color:var(--muted);border:1px solid transparent;border-bottom:none;position:relative;top:1px}
  .tab.active{background:var(--card);color:var(--accent);border-color:var(--border)}
  .tab:hover:not(.active){color:var(--text)}
  .page{display:none}.page.active{display:block}
  /* ─ Cards / Grid ─ */
  .grid{display:grid;gap:14px;grid-template-columns:1fr}
  @media(min-width:700px){.grid2{grid-template-columns:1fr 1fr}}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px}
  .card h2{font-size:.8rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-bottom:12px}
  /* ─ Status rows ─ */
  .row{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid var(--border);font-size:.88rem}
  .row:last-child{border-bottom:none}
  .badge{padding:2px 9px;border-radius:20px;font-size:.78rem;font-weight:600}
  .ok{background:rgba(34,197,94,.15);color:var(--green)}
  .warn{background:rgba(251,191,36,.12);color:var(--yellow)}
  /* ─ Forms ─ */
  label{display:block;font-size:.8rem;color:var(--muted);margin:10px 0 3px}
  input{width:100%;padding:8px 11px;background:var(--code);border:1px solid var(--border);border-radius:7px;color:var(--text);font-size:.9rem;outline:none}
  input:focus{border-color:var(--accent)}
  /* ─ Buttons ─ */
  .btn{display:block;width:100%;margin-top:10px;padding:9px;font-weight:700;font-size:.9rem;border:none;border-radius:8px;cursor:pointer;transition:background .15s}
  .btn-primary{background:var(--accent);color:#0f172a}.btn-primary:hover{background:var(--accent2);color:#fff}
  .btn-outline{background:transparent;color:var(--accent);border:1px solid var(--accent)}.btn-outline:hover{background:var(--accent);color:#0f172a}
  .btn-danger{background:transparent;color:var(--red);border:1px solid var(--red)}.btn-danger:hover{background:var(--red);color:#fff}
  /* ─ Signal display ─ */
  .pronto{background:var(--code);border:1px solid var(--border);border-radius:8px;padding:10px;font-family:monospace;font-size:.75rem;color:#7dd3fc;word-break:break-all;min-height:44px;line-height:1.7}
  .detail{font-size:.85rem;margin:3px 0}.detail span{color:var(--accent);font-weight:600}
  /* ─ History table ─ */
  table{width:100%;border-collapse:collapse;font-size:.82rem}
  th{text-align:left;padding:6px 10px;color:var(--muted);border-bottom:1px solid var(--border);font-weight:600;font-size:.75rem;text-transform:uppercase}
  td{padding:7px 10px;border-bottom:1px solid var(--border);vertical-align:top}
  tr:last-child td{border-bottom:none}
  tr:hover td{background:rgba(255,255,255,.03)}
  .mono{font-family:monospace;font-size:.78rem;color:#7dd3fc;word-break:break-all;max-width:260px}
  /* ─ RSSI bar ─ */
  .rssi-bar{height:6px;background:var(--border);border-radius:4px;overflow:hidden;flex:1;margin-left:8px}
  .rssi-fill{height:100%;border-radius:4px;transition:width .4s}
  /* ─ Toast ─ */
  #toast{position:fixed;bottom:20px;right:20px;background:var(--card);border:1px solid var(--accent);color:var(--text);padding:9px 16px;border-radius:8px;font-size:.88rem;display:none;z-index:999;max-width:280px}
  /* ─ Pulse ─ */
  .pulse{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--green);margin-right:6px;animation:blink 1.4s infinite}
  @keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
</style>
</head>
<body>

<div class="topbar">
  <div>
    <h1>&#128225; ESP32-U IR Learner</h1>
    <p class="sub">Captures IR remote signals &amp; sends to Homey Pro 2023</p>
  </div>
  <div style="text-align:right;font-size:.8rem;color:var(--muted)">
    <div id="uptime">--</div>
    <div id="heapInfo">-- KB free</div>
  </div>
</div>

<!-- Tabs -->
<div class="tabs">
  <div class="tab active" onclick="switchTab('live')">Live Capture</div>
  <div class="tab" onclick="switchTab('history')">History</div>
  <div class="tab" onclick="switchTab('settings')">Settings</div>
</div>

<!-- ══════════════════════════════════════════════ LIVE ══ -->
<div id="page-live" class="page active">
  <div class="grid grid2">

    <div class="card">
      <h2>System Status</h2>
      <div class="row"><span>Wi-Fi</span><span id="wifiStatus" class="badge ok">Connecting...</span></div>
      <div class="row"><span>IP Address</span><span id="ip" style="color:var(--accent);font-family:monospace">--</span></div>
      <div class="row">
        <span>Signal (RSSI)</span>
        <div style="display:flex;align-items:center;width:120px">
          <span id="rssi" style="font-size:.82rem;min-width:40px">-- dBm</span>
          <div class="rssi-bar"><div id="rssiFill" class="rssi-fill" style="width:0%;background:var(--green)"></div></div>
        </div>
      </div>
      <div class="row"><span>IR Pin</span><span>GPIO 15</span></div>
      <div class="row"><span>Captures</span><span id="captureCount" style="color:var(--accent)">0</span></div>
    </div>

    <div class="card">
      <h2>Last Captured Signal</h2>
      <div id="noSignal" style="color:var(--muted);font-size:.88rem;padding:6px 0">
        <span class="pulse"></span>Waiting for IR remote button press...
      </div>
      <div id="signalInfo" style="display:none">
        <div class="detail">Protocol: <span id="proto">--</span></div>
        <div class="detail">Bits: <span id="bits">--</span></div>
        <div class="detail">Hex Code: <span id="hexCode" style="font-family:monospace">--</span></div>
        <div class="detail">Captured at: <span id="ts">--</span></div>
        <div style="margin-top:10px;font-size:.8rem;color:var(--muted)">Pronto HEX (paste into Homey):</div>
        <div class="pronto" id="pronto">--</div>
        <button class="btn btn-primary" onclick="sendToHomey()">&#9889; Send to Homey Now</button>
        <button class="btn btn-outline" onclick="copyPronto()">&#128203; Copy Pronto HEX</button>
      </div>
    </div>

  </div>
</div>

<!-- ══════════════════════════════════════════════ HISTORY ══ -->
<div id="page-history" class="page">
  <div class="card">
    <h2>Capture History (last 10)</h2>
    <div style="overflow-x:auto">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Protocol</th>
            <th>Bits</th>
            <th>Hex Code</th>
            <th>Pronto HEX</th>
            <th></th>
          </tr>
        </thead>
        <tbody id="historyBody">
          <tr><td colspan="6" style="color:var(--muted);text-align:center;padding:20px">No captures yet</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>

<!-- ══════════════════════════════════════════════ SETTINGS ══ -->
<div id="page-settings" class="page">
  <div class="grid grid2">

    <div class="card">
      <h2>Homey Configuration</h2>
      <label>Homey Pro IP Address</label>
      <input id="homeyIp" type="text" placeholder="e.g. 192.168.1.50">
      <label>Webhook Event Name</label>
      <input id="webhookTag" type="text" placeholder="ir_learned">
      <button class="btn btn-primary" onclick="saveConfig()">Save Settings</button>
    </div>

    <div class="card">
      <h2>Wi-Fi</h2>
      <p style="font-size:.85rem;color:var(--muted);line-height:1.6">
        Hold the <strong style="color:var(--text)">BOOT button (GPIO 0)</strong> for
        <strong style="color:var(--text)">3 seconds</strong> to erase saved
        Wi-Fi credentials. The device will restart and broadcast the
        <strong style="color:var(--text)">ESP32-IR-Learner</strong> access point
        for re-configuration.
      </p>
      <div style="margin-top:12px;font-size:.82rem;color:var(--muted)">
        Current network: <span id="settingsIp" style="color:var(--accent);font-family:monospace">--</span>
      </div>
    </div>

  </div>
</div>

<div id="toast"></div>

<script>
  let lastSigKey = "";

  // ── Tab switching ──────────────────────────────────────────────────────────
  function switchTab(name) {
    document.querySelectorAll('.tab').forEach((t,i) => {
      const pages = ['live','history','settings'];
      t.classList.toggle('active', pages[i] === name);
      document.getElementById('page-'+pages[i]).classList.toggle('active', pages[i] === name);
    });
    if (name === 'history') loadHistory();
  }

  // ── Toast ──────────────────────────────────────────────────────────────────
  function toast(msg) {
    const el = document.getElementById('toast');
    el.innerText = msg;
    el.style.display = 'block';
    clearTimeout(el._t);
    el._t = setTimeout(() => el.style.display = 'none', 3000);
  }

  // ── Status poll ────────────────────────────────────────────────────────────
  function fetchStatus() {
    fetch('/api/status').then(r => r.json()).then(d => {
      document.getElementById('ip').innerText         = d.ip;
      document.getElementById('settingsIp').innerText = d.ip;
      document.getElementById('uptime').innerText     = 'Uptime ' + d.uptime;
      document.getElementById('heapInfo').innerText   = Math.round(d.free_heap/1024) + ' KB free';
      document.getElementById('captureCount').innerText = d.capture_count || 0;
      document.getElementById('homeyIp').value    = d.homey_ip || '';
      document.getElementById('webhookTag').value = d.webhook_tag || 'ir_learned';

      const rssi = d.rssi || -100;
      document.getElementById('rssi').innerText = rssi + ' dBm';
      const pct = Math.max(0, Math.min(100, (rssi + 100) * 2));
      const color = pct > 60 ? '#22c55e' : pct > 30 ? '#fbbf24' : '#ef4444';
      document.getElementById('rssiFill').style.width = pct + '%';
      document.getElementById('rssiFill').style.background = color;

      const badge = document.getElementById('wifiStatus');
      badge.innerText   = 'Connected';
      badge.className   = 'badge ok';
    }).catch(() => {
      document.getElementById('wifiStatus').className = 'badge warn';
      document.getElementById('wifiStatus').innerText = 'Offline';
    });
  }

  // ── Last signal poll ───────────────────────────────────────────────────────
  function pollSignal() {
    fetch('/api/last-signal').then(r => r.json()).then(d => {
      if (!d.valid) return;
      const key = d.hex_code + d.bits + d.timestamp;
      if (key === lastSigKey) return;
      lastSigKey = key;

      document.getElementById('noSignal').style.display  = 'none';
      document.getElementById('signalInfo').style.display = 'block';
      document.getElementById('proto').innerText   = d.protocol;
      document.getElementById('bits').innerText    = d.bits;
      document.getElementById('hexCode').innerText = d.hex_code;
      document.getElementById('ts').innerText      = d.timestamp;
      document.getElementById('pronto').innerText  = d.pronto;
    }).catch(console.error);
  }

  // ── History ────────────────────────────────────────────────────────────────
  function loadHistory() {
    fetch('/api/history').then(r => r.json()).then(rows => {
      const tbody = document.getElementById('historyBody');
      if (!rows.length) {
        tbody.innerHTML = '<tr><td colspan="6" style="color:var(--muted);text-align:center;padding:20px">No captures yet</td></tr>';
        return;
      }
      tbody.innerHTML = rows.map((r,i) => `
        <tr>
          <td style="white-space:nowrap;color:var(--muted)">${r.timestamp}</td>
          <td><span style="color:var(--accent)">${r.protocol}</span></td>
          <td>${r.bits}</td>
          <td style="font-family:monospace;font-size:.8rem">${r.hex_code}</td>
          <td class="mono">${r.pronto.substring(0,40)}${r.pronto.length>40?'…':''}</td>
          <td><button style="padding:3px 10px;font-size:.78rem;border-radius:6px;background:transparent;color:var(--accent);border:1px solid var(--accent);cursor:pointer" onclick="copyText('${r.pronto}')">Copy</button></td>
        </tr>`).join('');
    }).catch(console.error);
  }

  // ── Actions ────────────────────────────────────────────────────────────────
  function saveConfig() {
    const ip  = document.getElementById('homeyIp').value;
    const tag = document.getElementById('webhookTag').value;
    fetch('/api/config', {
      method: 'POST',
      headers: {'Content-Type':'application/x-www-form-urlencoded'},
      body: 'homey_ip='+encodeURIComponent(ip)+'&webhook_tag='+encodeURIComponent(tag)
    }).then(() => toast('Settings saved!')).catch(() => toast('Save failed!'));
  }

  function sendToHomey() {
    fetch('/api/trigger-homey', {method:'POST'})
      .then(r => r.text()).then(msg => toast(msg)).catch(() => toast('Send failed!'));
  }

  function copyPronto() {
    copyText(document.getElementById('pronto').innerText);
  }

  function copyText(text) {
    navigator.clipboard.writeText(text)
      .then(() => toast('Copied to clipboard!'))
      .catch(() => toast('Copy failed'));
  }

  // ── Init ──────────────────────────────────────────────────────────────────
  fetchStatus();
  setInterval(pollSignal,  1200);
  setInterval(fetchStatus, 8000);
</script>
</body>
</html>
)rawliteral";
