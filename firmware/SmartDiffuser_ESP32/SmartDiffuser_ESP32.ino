/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 3.0 (System Stability & Modularization)
 * [작성자] 21학번 류재홍
 * [수정내용] 
 * 1. Non-blocking WiFi 재접속 로직 적용 (끊겨도 모터는 돌아감)
 * 2. 시스템 상태 표시 LED (Heartbeat) 추가
 * 3. Loop 함수 최적화 및 모듈화
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ============================================================
// [1] 설정 정보 (Configuration)
// ============================================================
const char* ssid     = "Jaehong_WiFi";      
const char* password = "12345678";        

String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 핀 번호 설정
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  // [New] ESP32 내장 LED (상태 표시용)

// 로드셀 설정
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

// 타이머 설정
unsigned long sprayDuration = 3000; // 가변 분사 시간
const long REST_TIME      = 5000;   // 고정 휴식 시간

// ============================================================
// [2] 전역 변수 (Global Variables)
// ============================================================
HX711 scale;
float calibration_factor = 430.0; 

// 상태 관리
int currentMode = 0;       
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;   

// 타이머 관리 변수
unsigned long prevMotorMillis = 0; 
unsigned long prevWifiMillis = 0;
unsigned long prevLedMillis = 0;
bool ledState = false;

// ============================================================
// [3] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);

  // 핀 모드 설정
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT); // [New] LED

  forceAllOff(); // 안전 초기화

  Serial.println("\n\n========================================");
  Serial.println("      🌿 SMART DIFFUSER V3.0 🌿        ");
  Serial.println("========================================");
  
  // WiFi 연결
  connectWiFi();
  
  // 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  printMainMenu(); 
}

// ============================================================
// [4] 메인 루프 (Loop) - 아주 깔끔해짐!
// ============================================================
void loop() {
  // 1. WiFi 관리 (끊기면 백그라운드 재접속)
  manageWiFi();

  // 2. 시스템 상태 LED 깜빡임 (살아있음 표시)
  systemHeartbeat();

  // 3. 모터 타이머 로직 (작동 중일 때만)
  if (isRunning) {
    runSprayLogic();
  }

  // 4. 사용자 입력 감지 및 처리
  checkSerialInput();
}

// ============================================================
// [5] 핵심 기능 함수들 (Modules)
// ============================================================

// [기능 1] 모터 타이머 로직 (핵심)
void runSprayLogic() {
  if (activePin == -1) return;

  unsigned long currentMillis = millis();
  
  if (isSpraying) {
    // 분사 -> 휴식 전환
    if (currentMillis - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH); // 끄기
      isSpraying = false;
      prevMotorMillis = currentMillis;
      Serial.println("      └── [Idle] ⏳ 휴식 중...");
    }
  } 
  else {
    // 휴식 -> 분사 전환
    if (currentMillis - prevMotorMillis >= REST_TIME) {
      forceAllOff(); // [Safety] 중복 방지
      digitalWrite(activePin, LOW); // 켜기
      isSpraying = true;
      prevMotorMillis = currentMillis;
      Serial.print("      ┌── [Action] 💨 분사 시작! (");
      Serial.print(sprayDuration / 1000);
      Serial.println("초)");
    }
  }
}

// [기능 2] 시리얼 입력 감지
void checkSerialInput() {
  if (Serial.available() > 0) {
    delay(100); // 데이터 수신 대기
    String input = Serial.readStringUntil('\n');
    input.trim();
    while(Serial.available() > 0) Serial.read(); // 버퍼 비우기

    if (input.length() > 0) {
      if (input == "0") {
        stopSystem();
        currentMode = 0;
        printMainMenu();
      } else {
        handleInput(input);
      }
    }
  }
}

// [기능 3] WiFi 연결 및 재접속 관리
void connectWiFi() {
  Serial.print("[System] WiFi Connecting");
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(250); Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[System] WiFi Connected! 📶");
  } else {
    Serial.println("\n[System] WiFi Failed. (Offline Mode)");
  }
}

void manageWiFi() {
  // 30초마다 WiFi 상태 체크 (멈춤 없이)
  unsigned long currentMillis = millis();
  if (currentMillis - prevWifiMillis >= 30000) {
    prevWifiMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n[Warning] WiFi 끊김. 재연결 시도...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

// [기능 4] 시스템 하트비트 (LED 깜빡임)
void systemHeartbeat() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevLedMillis >= 1000) { // 1초마다
    prevLedMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState);
  }
}

// [기능 5] 사용자 입력 처리 분배
void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println("\n--- [ Mode 1: 수동 제어 ] (1~4 입력) ---");
    } else if (input == "2") {
      currentMode = 2;
      Serial.println("\n--- [ Mode 2: 감성 모드 ] (1~4 입력) ---");
    } else if (input == "3") {
      currentMode = 3;
      Serial.println("\n--- [ Mode 3: 날씨 모드 ] (지역명 입력) ---");
    } else {
      Serial.println("❌ 잘못된 입력입니다."); printMainMenu();
    }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) runEmotionMode(input); 
  else if (currentMode == 3) runWeatherMode(input); 
}

// ============================================================
// [6] 제어 및 통신 함수들
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
  activePin = -1;
  isSpraying = false;
  Serial.println("\n⛔ [System] 작동 정지.");
}

void startInterval(int pin, unsigned long duration) {
  forceAllOff();
  activePin = pin;
  isRunning = true;
  isSpraying = true; 
  sprayDuration = duration; 
  prevMotorMillis = millis(); 
  
  digitalWrite(activePin, LOW); 
  Serial.println("[Loop] 반복 작동 시작 (중단: '0')");
}

void runManualMode(String input) {
  isRunning = false; 
  int pin = -1;
  
  if (input == "1") pin = PIN_SUNNY;
  else if (input == "2") pin = PIN_CLOUDY;
  else if (input == "3") pin = PIN_RAIN;
  else if (input == "4") pin = PIN_SNOW;
  else { Serial.println("⚠️ 1~4 입력"); return; }

  forceAllOff();
  Serial.println("[Manual] 3초간 작동...");
  digitalWrite(pin, LOW);
  delay(3000); 
  digitalWrite(pin, HIGH);
  Serial.println("[Manual] 완료");
}

void runEmotionMode(String val) {
  String json = "{\"mode\": \"emotion\", \"user_emotion\": \"" + val + "\"}";
  Serial.println("[Emotion] 서버 요청...");
  sendServerRequest(json); 
}

void runWeatherMode(String region) {
  float w = 0.0;
  if (scale.is_ready()) w = scale.get_units(5);
  String json = "{\"mode\": \"weather\", \"region\": \"" + region + "\", \"weight\": " + String(w) + "}";
  Serial.println("[Weather] 서버 요청...");
  sendServerRequest(json); 
}

void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("🚨 WiFi 연결 안됨!"); return;
  }
  
  HTTPClient http;
  http.setTimeout(5000); 
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(payload);
  if(code > 0){
    String res = http.getString();
    JsonDocument doc; 
    deserializeJson(doc, res);
    int cmd = doc["spray"]; 
    int dur = doc["duration"]; 
    String txt = doc["result_text"];

    Serial.println("✅ 수신 완료: " + txt);
    
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY;
    else if (cmd == 2) target = PIN_CLOUDY;
    else if (cmd == 3) target = PIN_RAIN;
    else if (cmd == 4) target = PIN_SNOW;

    if (target != -1) startInterval(target, dur * 1000);
    else { Serial.println("⚠️ 정지 명령 수신"); stopSystem(); }
  } else {
    Serial.printf("🚨 통신 실패 (Code: %d)\n", code);
  }
  http.end();
}

void printMainMenu() {
  Serial.println("\n=== 🕹️ MAIN MENU 🕹️ ===");
  Serial.println(" [1] 수동  [2] 감성  [3] 날씨");
  Serial.println("👉 입력 >>");
}
