#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ArduinoJson.h>

#include "pronto.h"
#include "web_page.h"

// ─── Pin / IR config ────────────────────────────────────────────────────────
#define IR_RECV_PIN       15      // TSOP / VS1838B DATA pin
#define CAPTURE_BUF_SIZE  1024   // max raw pulse pairs in buffer
#define TIMEOUT_MS        50     // inter-pulse gap that signals end of frame (ms)
#define RESET_PIN         0      // GPIO0 / BOOT button – hold 3 s to reset Wi-Fi
#define HISTORY_SIZE      10     // number of past captures to keep

// ─── Globals ─────────────────────────────────────────────────────────────────
IRrecv         irrecv(IR_RECV_PIN, CAPTURE_BUF_SIZE, TIMEOUT_MS, true);
decode_results results;
WebServer      server(80);
Preferences    prefs;

String homeyIp      = "";
String webhookTag   = "ir_learned";

struct IRSignal {
    bool      valid    = false;
    String    protocol = "";
    uint32_t  bits     = 0;
    String    hexCode  = "";
    String    pronto   = "";
    uint16_t  rawLen   = 0;
    String    timestamp = "";
};

IRSignal lastSignal;
IRSignal history[HISTORY_SIZE];
uint8_t  historyHead = 0;   // ring-buffer write index
uint8_t  historyCount = 0;  // total filled slots (max HISTORY_SIZE)

// Wi-Fi reset state
unsigned long resetPressedAt = 0;
bool          resetArmed     = false;

// ─── Forward declarations ─────────────────────────────────────────────────────
void handleRoot();
void handleStatus();
void handleLastSignal();
void handleHistory();
void handleConfig();
void handleTriggerHomey();
void pushToHomey(const IRSignal &sig);
String uptimeString();

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n\n=== ESP32-U IR Learner for Homey Pro 2023 ===");

    // BOOT button used for Wi-Fi credential reset
    pinMode(RESET_PIN, INPUT_PULLUP);

    // Load persisted config
    prefs.begin("ir-cfg", false);
    homeyIp    = prefs.getString("homey_ip",    "");
    webhookTag = prefs.getString("webhook_tag", "ir_learned");

    // ── Wi-Fi via WiFiManager captive-portal ─────────────────────────────────
    // First boot → ESP32 hosts AP "ESP32-IR-Learner" → connect from phone
    // and enter your Wi-Fi credentials in the captive portal.
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setAPCallback([](WiFiManager *mgr) {
        Serial.printf("[WiFi] Config AP: %s  –  connect to configure\n",
                      mgr->getConfigPortalSSID().c_str());
    });

    if (!wm.autoConnect("ESP32-IR-Learner")) {
        Serial.println("[WiFi] Connection failed – restarting in 5 s");
        delay(5000);
        ESP.restart();
    }

    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

    // ── mDNS ─────────────────────────────────────────────────────────────────
    if (MDNS.begin("esp32-ir")) {
        Serial.println("[mDNS] http://esp32-ir.local");
    }

    // ── Start IR receiver ────────────────────────────────────────────────────
    irrecv.enableIRIn();
    Serial.printf("[IR]   Receiver active on GPIO %d\n", IR_RECV_PIN);

    // ── Web server routes ────────────────────────────────────────────────────
    server.on("/",                  HTTP_GET,  handleRoot);
    server.on("/api/status",        HTTP_GET,  handleStatus);
    server.on("/api/last-signal",   HTTP_GET,  handleLastSignal);
    server.on("/api/history",       HTTP_GET,  handleHistory);
    server.on("/api/config",        HTTP_POST, handleConfig);
    server.on("/api/trigger-homey", HTTP_POST, handleTriggerHomey);
    server.begin();
    Serial.println("[HTTP] Server started on port 80");
    Serial.println("=============================================");
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
    server.handleClient();

    // ── Wi-Fi reset: hold BOOT button for 3 seconds ───────────────────────
    if (digitalRead(RESET_PIN) == LOW) {
        if (!resetArmed) {
            resetArmed     = true;
            resetPressedAt = millis();
        } else if (millis() - resetPressedAt >= 3000) {
            Serial.println("[WiFi] Reset button held – clearing credentials");
            WiFiManager wm;
            wm.resetSettings();
            delay(500);
            ESP.restart();
        }
    } else {
        resetArmed = false;
    }

    // ── IR capture ────────────────────────────────────────────────────────
    if (irrecv.decode(&results)) {
        if (results.overflow) {
            Serial.println("[IR] Buffer overflow – signal too long for buffer");
        } else {
            // Build signal struct
            IRSignal sig;
            sig.valid     = true;
            sig.protocol  = typeToString(results.decode_type);
            sig.bits      = results.bits;
            sig.hexCode   = resultToHexidecimal(&results);
            sig.timestamp = uptimeString();

            // Convert raw buffer to Pronto HEX then immediately free heap memory
            uint16_t *raw  = resultToRawArray(&results);
            uint16_t  rLen = getCorrectedRawLength(&results);
            sig.rawLen     = rLen;
            sig.pronto     = rawToProntoHex(raw, rLen);
            delete[] raw;  // free heap allocation from resultToRawArray

            lastSignal = sig;

            // Store in ring-buffer history
            history[historyHead] = sig;
            historyHead          = (historyHead + 1) % HISTORY_SIZE;
            if (historyCount < HISTORY_SIZE) historyCount++;

            Serial.println("\n[IR] ── Signal captured ─────────────────");
            Serial.printf("     Protocol  : %s (%u bits)\n", sig.protocol.c_str(), sig.bits);
            Serial.printf("     Hex Code  : %s\n",            sig.hexCode.c_str());
            Serial.printf("     Raw len   : %u samples\n",    sig.rawLen);
            Serial.printf("     Pronto    : %s\n",            sig.pronto.c_str());
            Serial.println("[IR] ─────────────────────────────────────");

            // Auto-push to Homey if IP is configured
            if (!homeyIp.isEmpty()) {
                pushToHomey(sig);
            }
        }
        irrecv.resume();
    }
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
String uptimeString() {
    unsigned long s = millis() / 1000;
    char buf[20];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             s / 3600, (s % 3600) / 60, s % 60);
    return String(buf);
}

// ─── HTTP Handlers ────────────────────────────────────────────────────────────

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    StaticJsonDocument<256> doc;
    doc["ip"]           = WiFi.localIP().toString();
    doc["rssi"]         = WiFi.RSSI();
    doc["free_heap"]    = ESP.getFreeHeap();
    doc["uptime"]       = uptimeString();
    doc["homey_ip"]     = homeyIp;
    doc["webhook_tag"]  = webhookTag;
    doc["capture_count"] = historyCount;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

void handleLastSignal() {
    // Use a generous buffer – Pronto HEX for long HVAC codes can reach ~600 chars
    DynamicJsonDocument doc(2048);
    doc["valid"]    = lastSignal.valid;
    doc["protocol"] = lastSignal.protocol;
    doc["bits"]     = lastSignal.bits;
    doc["hex_code"] = lastSignal.hexCode;
    doc["pronto"]   = lastSignal.pronto;
    doc["raw_len"]  = lastSignal.rawLen;
    doc["timestamp"] = lastSignal.timestamp;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

void handleHistory() {
    // Returns up to HISTORY_SIZE past captures newest-first
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();

    // Walk ring buffer newest → oldest
    for (uint8_t i = 0; i < historyCount; i++) {
        int idx = ((int)historyHead - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
        const IRSignal &s = history[idx];
        JsonObject obj = arr.createNestedObject();
        obj["protocol"]  = s.protocol;
        obj["bits"]      = s.bits;
        obj["hex_code"]  = s.hexCode;
        obj["pronto"]    = s.pronto;
        obj["raw_len"]   = s.rawLen;
        obj["timestamp"] = s.timestamp;
    }
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

void handleConfig() {
    if (server.hasArg("homey_ip"))    homeyIp    = server.arg("homey_ip");
    if (server.hasArg("webhook_tag")) webhookTag = server.arg("webhook_tag");
    prefs.putString("homey_ip",    homeyIp);
    prefs.putString("webhook_tag", webhookTag);
    server.send(200, "text/plain", "Saved!");
}

void handleTriggerHomey() {
    if (!lastSignal.valid) {
        server.send(400, "text/plain", "No signal captured yet.");
        return;
    }
    pushToHomey(lastSignal);
    server.send(200, "text/plain", "Signal sent to Homey!");
}

// ─── Homey Webhook Push ───────────────────────────────────────────────────────
//
// Homey Pro 2023 built-in Webhook endpoint:
//   POST  http://<HOMEY_IP>/api/manager/logic/webhook/<EVENT_NAME>
//   Body: JSON { "event": "...", "pronto": "...", ... }
//
// In Homey Flows:
//   WHEN  Webhook "ir_learned" is received
//   THEN  IR Blaster → Send IR signal  (use [[tag]] variable = Pronto HEX)
//
void pushToHomey(const IRSignal &sig) {
    if (homeyIp.isEmpty()) return;

    String url = "http://" + homeyIp
               + "/api/manager/logic/webhook/" + webhookTag
               + "?tag=" + sig.pronto;   // Homey exposes the query 'tag' as a Flow token

    DynamicJsonDocument body(1024);
    body["event"]    = webhookTag;
    body["protocol"] = sig.protocol;
    body["bits"]     = sig.bits;
    body["hex_code"] = sig.hexCode;
    body["pronto"]   = sig.pronto;
    String payload;
    serializeJson(body, payload);

    Serial.printf("[Homey] POST %s\n", url.c_str());

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    if (code > 0) {
        Serial.printf("[Homey] Response: %d\n", code);
    } else {
        Serial.printf("[Homey] Error: %s\n", http.errorToString(code).c_str());
    }
    http.end();
}
