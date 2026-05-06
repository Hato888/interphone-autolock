#include <Wire.h>
#include <BH1750.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>
#include "secrets.h"  // 追加

const char* SSID = WIFI_SSID;
const char* PASS = WIFI_PASS;
const char* SB_TOKEN_STR  = SB_TOKEN;
const char* SB_SECRET_STR = SB_SECRET;
const char* DEVICE_ID = SB_DEVICE_ID;
const char* DISCORD_WEBHOOK_URL = DISCORD_WEBHOOK;

// 以下は変更なし

BH1750 lightMeter(0x23);
WebServer server(80);
Preferences prefs;

float threshold = 50.0;
float baselineLux = 0;
float currentLux = 0;
bool autoUnlock = false;
unsigned long lastTrigger = 0;
const unsigned long COOLDOWN = 30000;

String generateSign(String token, String secret, String nonce, long long timestamp) {
  String data = token + String(timestamp) + nonce;
  byte hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, (byte*)secret.c_str(), secret.length());
  mbedtls_md_hmac_update(&ctx, (byte*)data.c_str(), data.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);
  size_t outLen;
  byte encoded[64];
  mbedtls_base64_encode(encoded, sizeof(encoded), &outLen, hmac, 32);
  return String((char*)encoded).substring(0, outLen);
}

void notifyDiscord(String message) {
  HTTPClient http;
  http.begin(DISCORD_WEBHOOK);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"content\":\"" + message + "\"}";
  int code = http.POST(body);
  Serial.printf("Discord: %d\n", code);
  http.end();
}

void triggerSwitchBot() {
  if (millis() - lastTrigger < COOLDOWN) return;
  lastTrigger = millis();
  Serial.println("DETECTED! → SwitchBot起動");

  long long ts = (long long)time(nullptr) * 1000;
  String nonce = "abc123";
  String sign = generateSign(SB_TOKEN, SB_SECRET, nonce, ts);

  HTTPClient http;
  String url = "https://api.switch-bot.com/v1.1/devices/" + String(DEVICE_ID) + "/commands";
  http.begin(url);
  http.addHeader("Authorization", SB_TOKEN);
  http.addHeader("sign", sign);
  http.addHeader("nonce", nonce);
  http.addHeader("t", String(ts));
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"command\":\"turnOn\",\"parameter\":\"default\",\"commandType\":\"command\"}");
  Serial.printf("SwitchBot: %d\n", code);
  http.end();

  notifyDiscord("🔔 インターホン検知 → オートロック解除しました");
}

void calibrate() {
  float sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += lightMeter.readLightLevel();
    delay(100);
  }
  baselineLux = sum / 20;
}

void handleRoot() {
  float delta = currentLux - baselineLux;
  bool detected = delta > threshold;

  String html = R"(<!DOCTYPE html><html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>インターホン監視</title>
<style>
  body{font-family:monospace;background:#111;color:#eee;padding:20px;}
  h1{color:#0f0;}
  .card{background:#222;border-radius:8px;padding:16px;margin:12px 0;}
  .val{font-size:2em;font-weight:bold;}
  .detected{color:#f00;}
  .normal{color:#0f0;}
  .on{color:#0f0;font-weight:bold;}
  .off{color:#888;}
  input[type=number]{background:#333;color:#eee;border:1px solid #555;padding:8px;border-radius:4px;font-size:1.2em;width:100px;}
  button{background:#0a0;color:#fff;border:none;padding:10px 24px;border-radius:4px;font-size:1em;cursor:pointer;margin-top:8px;}
  button.off{background:#555;}
  button:hover{opacity:0.8;}
</style>
<script>
  let autoRefresh = true;
  setInterval(()=>{ if(autoRefresh) location.reload(); }, 2000);
  document.addEventListener('focusin', e=>{ if(e.target.tagName==='INPUT') autoRefresh=false; });
  document.addEventListener('focusout', e=>{ if(e.target.tagName==='INPUT') autoRefresh=true; });
</script>
</head>
<body>
<h1>📡 インターホン監視</h1>
<div class='card'>
  <div>現在のLux</div>
  <div class='val'>)";
  html += String(currentLux, 1);
  html += R"( lux</div>
  <div>ベースライン: )";
  html += String(baselineLux, 1);
  html += R"( lux　差分: )";
  html += String(delta, 1);
  html += R"( lux</div>
</div>
<div class='card'>
  <div class=')";
  html += detected ? "detected" : "normal";
  html += R"('>)";
  html += detected ? "🔔 呼び出し検知中！" : "✅ 待機中";
  html += R"(</div>
</div>
<div class='card'>
  <div>自動解除: <span class=')";
  html += autoUnlock ? "on" : "off";
  html += R"('>)";
  html += autoUnlock ? "ON" : "OFF";
  html += R"(</span></div>
  <form action='/toggle' method='GET'>
    <button type='submit' class=')";
  html += autoUnlock ? "" : "off";
  html += R"('>)";
  html += autoUnlock ? "🔓 無効にする" : "🔒 有効にする";
  html += R"(</button>
  </form>
</div>
<div class='card'>
  <form action='/set' method='GET'>
    <div>閾値設定（現在: )";
  html += String(threshold, 1);
  html += R"( lux）</div>
    <input type='number' name='threshold' value=')";
  html += String((int)threshold);
  html += R"(' step='1' min='1'>
    <br><button type='submit'>保存</button>
  </form>
  <form action='/calibrate' method='GET' style='margin-top:8px'>
    <button type='submit' class='off'>🔄 ベースライン再較正</button>
  </form>
</div>
</body></html>)";

  server.send(200, "text/html", html);
}

void handleToggle() {
  autoUnlock = !autoUnlock;
  Serial.printf("自動解除: %s\n", autoUnlock ? "ON" : "OFF");
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleSet() {
  if (server.hasArg("threshold")) {
    threshold = server.arg("threshold").toFloat();
    prefs.putFloat("threshold", threshold);
  }
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleCalibrate() {
  calibrate();
  server.sendHeader("Location", "/");
  server.send(302);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  prefs.begin("ilsettings", false);
  threshold = prefs.getFloat("threshold", 50.0);

  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("IP: " + WiFi.localIP().toString());

  configTime(9 * 3600, 0, "pool.ntp.org");
  delay(2000);

  calibrate();
  Serial.printf("Baseline: %.1f lux, Threshold: %.1f lux\n", baselineLux, threshold);

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/set", handleSet);
  server.on("/calibrate", handleCalibrate);
  server.begin();
}

void loop() {
  currentLux = lightMeter.readLightLevel();
  float delta = currentLux - baselineLux;
  if (autoUnlock && delta > threshold) {
    triggerSwitchBot();
  }
  server.handleClient();
}