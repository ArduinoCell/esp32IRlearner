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
#define CAPTURE_BUF_SIZE  1024   // max raw pulse pairs
#define TIMEOUT_MS        50     // inter-pulse timeout (ms)

// ─── Globals ─────────────────────────────────────────────────────────────────
IRrecv         irrecv(IR_RECV_PIN, CAPTURE_BUF_SIZE, TIMEOUT_MS, true);
decode_results results;
WebServer      server(80);
Preferences    prefs;

String homeyIp      = "";
String webhookTag   = "ir_learned";

struct IRSignal {
    bool    valid    = false;
    String  protocol = "";
    uint32_t bits    = 0;
    String  hexCode  = "";
    String  pronto   = "";
    uint16_t rawLen  = 0;
} lastSignal;

// ─── Forward declarations ─────────────────────────────────────────────────────
void handleRoot();
void handleStatus();
void handleLastSignal();
void handleConfig();
void handleTriggerHomey();
void pushToHomey(const IRSignal &sig);

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n\n=== ESP32-U IR Learner for Homey Pro 2023 ===");

    // Load persisted config
    prefs.begin("ir-cfg", false);
    homeyIp    = prefs.getString("homey_ip",   "");
    webhookTag = prefs.getString("webhook_tag", "ir_learned");

    // ── Wi-Fi via WiFiManager captive-portal ─────────────────────────────────
    // On first boot (or after reset) the ESP32 creates a hotspot named
    // "ESP32-IR-Learner" – connect with your phone and enter your Wi-Fi creds.
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    wm.setAPCallback([](WiFiManager *mgr) {
        Serial.printf("Config AP started: %s\n", mgr->getConfigPortalSSID().c_str());
    });

    if (!wm.autoConnect("ESP32-IR-Learner")) {
        Serial.println("Wi-Fi connection failed – restarting in 5 s");
        delay(5000);
        ESP.restart();
    }

    Serial.printf("Connected to Wi-Fi.  IP: %s\n", WiFi.localIP().toString().c_str());

    // ── mDNS ─────────────────────────────────────────────────────────────────
    if (MDNS.begin("esp32-ir")) {
        Serial.println("mDNS: http://esp32-ir.local");
    }

    // ── Start IR receiver ────────────────────────────────────────────────────
    irrecv.enableIRIn();
    Serial.printf("IR receiver active on GPIO %d\n", IR_RECV_PIN);

    // ── Web server routes ────────────────────────────────────────────────────
    server.on("/",                  HTTP_GET,  handleRoot);
    server.on("/api/status",        HTTP_GET,  handleStatus);
    server.on("/api/last-signal",   HTTP_GET,  handleLastSignal);
    server.on("/api/config",        HTTP_POST, handleConfig);
    server.on("/api/trigger-homey", HTTP_POST, handleTriggerHomey);
    server.begin();
    Serial.println("HTTP server started on port 80");
    Serial.println("=============================================");
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {
    server.handleClient();

    if (irrecv.decode(&results)) {
        if (results.overflow) {
            Serial.println("[IR] Buffer overflow – code too long");
        } else {
            // Populate last signal struct
            lastSignal.valid    = true;
            lastSignal.protocol = typeToString(results.decode_type);
            lastSignal.bits     = results.bits;
            lastSignal.hexCode  = resultToHexidecimal(&results);

            uint16_t *raw  = resultToRawArray(&results);
            uint16_t  rLen = getCorrectedRawLength(&results);
            lastSignal.rawLen = rLen;
            lastSignal.pronto = rawToProntoHex(raw, rLen);

            Serial.println("\n[IR] ── Signal captured ─────────────────");
            Serial.printf("     Protocol : %s (%u bits)\n",
                          lastSignal.protocol.c_str(), lastSignal.bits);
            Serial.printf("     Hex Code : %s\n", lastSignal.hexCode.c_str());
            Serial.printf("     Pronto   : %s\n", lastSignal.pronto.c_str());
            Serial.println("[IR] ─────────────────────────────────────");

            // Auto-push to Homey if configured
            if (homeyIp.length() > 0) {
                pushToHomey(lastSignal);
            }
        }
        irrecv.resume();
    }
}

// ─── HTTP Handlers ────────────────────────────────────────────────────────────

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    StaticJsonDocument<256> doc;
    doc["ip"]          = WiFi.localIP().toString();
    doc["free_heap"]   = ESP.getFreeHeap();
    doc["homey_ip"]    = homeyIp;
    doc["webhook_tag"] = webhookTag;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

void handleLastSignal() {
    StaticJsonDocument<1024> doc;
    doc["valid"]    = lastSignal.valid;
    doc["protocol"] = lastSignal.protocol;
    doc["bits"]     = lastSignal.bits;
    doc["hex_code"] = lastSignal.hexCode;
    doc["pronto"]   = lastSignal.pronto;
    doc["raw_len"]  = lastSignal.rawLen;
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
// Homey Pro 2023 built-in Webhook URL format:
//   POST http://<HOMEY_IP>/api/manager/logic/webhook/<EVENT_NAME>
//   Body: JSON  { "event": "...", "pronto": "...", ... }
//
// In Homey you create a Flow:
//   WHEN  Webhook "<EVENT_NAME>" is received
//   THEN  IR blaster: Send IR signal  (paste the Pronto HEX from the tag)
//
void pushToHomey(const IRSignal &sig) {
    if (homeyIp.isEmpty()) return;

    String url = "http://" + homeyIp
               + "/api/manager/logic/webhook/" + webhookTag;

    StaticJsonDocument<512> body;
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
