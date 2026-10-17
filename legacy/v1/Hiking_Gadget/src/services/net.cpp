#include "net.h"
#include "clock.h"
#include "log.h"
#include "../config.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../hal/buttons.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>

static Preferences prefs;
static WebServer   server(80);
static DNSServer   dns;
static const IPAddress AP_IP(192, 168, 4, 1);

// ── Credential storage (NVS, separate from EEPROM settings) ───────────────────
static String loadSSID() { prefs.begin("net", true);  String s = prefs.getString("ssid", ""); prefs.end(); return s; }
static String loadPASS() { prefs.begin("net", true);  String s = prefs.getString("pass", ""); prefs.end(); return s; }
static void   saveCreds(const String& s, const String& p) {
    prefs.begin("net", false);
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
    prefs.end();
}

bool Net::hasCreds() { return loadSSID().length() > 0; }

// ── Small OLED status helper ──────────────────────────────────────────────────
static void status(const char* l1, const char* l2, const char* l3) {
    Display::clear();
    UI::header("WIFI");
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_6x10_tf);
    // Three evenly spaced lines, shared left margin.
    if (l1) { u8g2.setCursor(4, 26); u8g2.print(l1); }
    if (l2) { u8g2.setCursor(4, 40); u8g2.print(l2); }
    if (l3) { u8g2.setCursor(4, 54); u8g2.print(l3); }
    Display::show();
}

// ── NTP: wait until system time becomes valid, then mark Clock synced ─────────
static bool runNTP() {
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);   // UTC; tz applied in Clock
    uint32_t deadline = millis() + 8000;
    while (millis() < deadline) {
        time_t now = time(nullptr);
        if (now > 1700000000UL) {
            Clock::setEpoch((uint32_t)now);          // marks synced + persists
            LOGI("NTP", "time set epoch=%lu", (unsigned long)now);
            return true;
        }
        delay(120);
    }
    LOGW("NTP", "no response in 8s");
    return false;
}

// ── Join a network with timeout ───────────────────────────────────────────────
static bool joinWiFi(const String& ssid, const String& pass) {
    LOGI("WIFI", "joining '%s'...", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
        delay(150);
    }
    bool ok = WiFi.status() == WL_CONNECTED;
    if (ok) LOGI("WIFI", "connected, ip=%s", WiFi.localIP().toString().c_str());
    else    LOGW("WIFI", "join failed (timeout)");
    return ok;
}

static void wifiOff() {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}

bool Net::syncTimeNow() {
    String ssid = loadSSID();
    if (ssid.length() == 0) { status("No network saved.", "Open WiFi setup", "in Settings."); delay(1500); return false; }

    status("Connecting to:", ssid.c_str(), nullptr);
    bool ok = joinWiFi(ssid, loadPASS());
    if (!ok) { status("WiFi failed.", "Check setup.", nullptr); wifiOff(); delay(1500); return false; }

    status("Connected.", "Getting time...", nullptr);
    bool synced = runNTP();
    wifiOff();                                       // power WiFi back off
    status(synced ? "Time synced!" : "NTP failed.",
           synced ? "WiFi off." : "Try again.", nullptr);
    delay(1200);
    return synced;
}

// ── Captive portal pages ──────────────────────────────────────────────────────
static const char* PAGE_HEAD =
    "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:16px}"
    "h2{margin:0 0 12px}select,input,button{width:100%;box-sizing:border-box;padding:10px;"
    "margin:6px 0;border-radius:8px;border:1px solid #444;background:#222;color:#eee;font-size:16px}"
    "button{background:#2a7;border:0;font-weight:bold}</style>";

static void handleRoot() {
    String h = String(PAGE_HEAD) + "<h2>Hike Gadget WiFi</h2><form action='/save' method='POST'>"
               "<label>Network</label><select name='ssid'>";
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        h += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) +
             "  (" + String(WiFi.RSSI(i)) + "dBm)</option>";
    }
    h += "</select><label>Password</label><input name='pass' type='password' placeholder='WiFi password'>"
         "<button type='submit'>Save &amp; Connect</button></form>"
         "<p style='color:#888;font-size:13px'>After saving, the device will join your "
         "network, set the clock over the internet, then turn WiFi off.</p>";
    server.send(200, "text/html", h);
}

static volatile bool g_saved = false;
static String        g_ssid, g_pass;

static void handleSave() {
    g_ssid = server.arg("ssid");
    g_pass = server.arg("pass");
    String h = String(PAGE_HEAD) + "<h2>Saved</h2><p>Connecting to <b>" + g_ssid +
               "</b> and syncing time. You can close this page.</p>";
    server.send(200, "text/html", h);
    g_saved = true;
}

void Net::startPortal() {
    g_saved = false;
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
    dns.start(53, "*", AP_IP);                       // captive: route all DNS to us

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);                   // captive portal catch-all
    server.begin();

    LOGI("WIFI", "portal up: AP '%s' at 192.168.4.1", WIFI_AP_SSID);
    status("AP: " WIFI_AP_SSID, "pass: " WIFI_AP_PASS, "go 192.168.4.1");

    // Portal loop — runs until the user saves a network or presses K3 to exit.
    while (true) {
        dns.processNextRequest();
        server.handleClient();

        Buttons::poll();
        bool exitReq = false;
        while (Buttons::hasEvent()) {
            if (Buttons::dequeue() == ButtonEvent::K3_SHORT) exitReq = true;
        }
        if (exitReq) break;

        if (g_saved) {
            server.stop();
            dns.stop();
            saveCreds(g_ssid, g_pass);
            status("Connecting to:", g_ssid.c_str(), nullptr);
            if (joinWiFi(g_ssid, g_pass)) {
                status("Connected.", "Getting time...", nullptr);
                bool s = runNTP();
                status(s ? "All set!" : "Got WiFi,", s ? "WiFi off." : "no NTP.", nullptr);
            } else {
                status("Wrong password?", "Saved anyway.", nullptr);
            }
            delay(1500);
            break;
        }
        delay(5);
    }
    wifiOff();
}
