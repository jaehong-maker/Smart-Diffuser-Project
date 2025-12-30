/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 8.0 (Audio Edition: 4D Experience)
 * [작성자] 21학번 류재홍
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h>
#include "DFRobotDFPlayerMini.h"

// ============================================================
// [0] 설정
// ============================================================
#define C_RESET  "\033[0m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_BLUE   "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN   "\033[36m"
#define C_BOLD   "\033[1m"

#define DEVICE_ID "ESP32-001"          // ✅ 추가
#define WEATHER_INTERVAL 3600000UL     // ✅ 1시간

const char* ssid = "Jaehong_WiFi";
const char* password = "12345678";
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 릴레이
const int PIN_SUNNY  = 26;
const int PIN_CLOUDY = 27;
const int PIN_RAIN   = 14;
const int PIN_SNOW   = 13;
const int PIN_LED    = 2;

// 로드셀
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;

// DFPlayer
const int DFPLAYER_RX_PIN = 32;
const int DFPLAYER_TX_PIN = 33;
HardwareSerial mySoftwareSerial(2);
DFRobotDFPlayerMini myDFPlayer;

// ============================================================
// [전역 변수]
// ============================================================
HX711 scale;
Preferences prefs;
WiFiServer webServer(80);

unsigned long sprayDuration = 3000;
const long REST_TIME = 5000;
const long MAX_RUN_TIME = 270000;

unsigned long lastWeatherMillis = 0;   // ✅ 추가
unsigned long prevMotorMillis = 0;
unsigned long prevWifiMillis = 0;
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0;

float calibration_factor = 430.0;
bool isSimulation = false;

int currentMode = 0;
bool isRunning = false;
bool isSpraying = false;
int activePin = -1;

// ============================================================
// [Setup]
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  forceAllOff();

  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  myDFPlayer.begin(mySoftwareSerial);
  myDFPlayer.volume(20);

  connectWiFi();
  webServer.begin();

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  printMainMenu();
}

// ============================================================
// [Loop]
// ============================================================
void loop() {
  manageWiFi();
  systemHeartbeat();
  handleWebClient();

  // ✅ 날씨 모드 1시간 자동 호출
  if (currentMode == 3) {
    if (millis() - lastWeatherMillis >= WEATHER_INTERVAL) {
      lastWeatherMillis = millis();

      float w = isSimulation ? 500.0 : scale.get_units(10);
      Serial.printf(C_YELLOW "[Weather] ⏰ 1시간 주기 Lambda 호출 (%.1fg)\r\n" C_RESET, w);

      sendServerRequest(
        String("{\"device\":\"") + DEVICE_ID +
        "\",\"mode\":\"weather\",\"region\":\"서울\",\"weight\":" + String(w) + "}"
      );
    }
  }

  if (isRunning) {
    runSprayLogic();
    checkSafety();
  }

  checkSerialInput();
}

// ============================================================
// [Lambda 통신]
// ============================================================
void sendServerRequest(String payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🚨 WiFi 연결 안됨");
    return;
  }

  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(payload);
  if (code > 0) {
    JsonDocument doc;
    deserializeJson(doc, http.getString());

    int cmd = doc["spray"];
    int dur = doc["duration"];

    int pin =
      (cmd == 1) ? PIN_SUNNY :
      (cmd == 2) ? PIN_CLOUDY :
      (cmd == 3) ? PIN_RAIN :
      (cmd == 4) ? PIN_SNOW : -1;

    if (pin != -1) {
      forceAllOff();
      activePin = pin;
      sprayDuration = dur * 1000;
      isRunning = true;
      isSpraying = true;
      startTimeMillis = millis();
      prevMotorMillis = millis();
      digitalWrite(activePin, LOW);
      myDFPlayer.play(cmd);
    }
  }
  http.end();
}

// ============================================================
// [기존 함수들 – 수정 없음]
// ============================================================
void forceAllOff() {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
}

void stopSystem() {
  forceAllOff();
  isRunning = false;
  isSpraying = false;
  activePin = -1;
  myDFPlayer.stop();
}

void manageWiFi() {
  if (millis() - prevWifiMillis > 30000) {
    prevWifiMillis = millis();
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

void systemHeartbeat() {
  if (millis() - prevLedMillis > 200) {
    prevLedMillis = millis();
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
  }
}
