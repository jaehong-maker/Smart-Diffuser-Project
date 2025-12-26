/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 4.0 (The Master Piece)
 * [작성자] 21학번 류재홍
 * [업그레이드] 
 * 1. ANSI Color Log 적용 (시각적 디버깅 강화)
 * 2. 로드셀 실시간 영점 조절(Tare) 메뉴 추가
 * 3. Max Run-Time Safety (30초 강제 종료 왓치독)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ============================================================
// [0] ANSI 컬러 코드 (로그 꾸미기용)
// ============================================================
#define C_RESET  "\033[0m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_BLUE   "\033[34m"
#define C_CYAN   "\033[36m"
#define C_BOLD   "\033[1m"

// ============================================================
// [1] 설정 정보
// ============================================================
const char* ssid     = "Jaehong_WiFi";      
const char* password = "12345678";        

String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 핀 번호
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

// 로드셀
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

// 타이머 및 안전 설정
unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;   
const long MAX_RUN_TIME   = 30000; // [Safety] 30초 이상 작동 시 강제 종료

// ============================================================
// [2] 전역 변수
// ============================================================
HX711 scale;
float calibration_factor = 430.0; 

int currentMode = 0;       
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;   

unsigned long prevMotorMillis = 0; 
unsigned long prevWifiMillis = 0;
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; // [Safety] 작동 시작 시간 기록
bool ledState = false;

// ============================================================
// [3] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  forceAllOff(); 

  Serial.println("\n\n");
  Serial.println(C_CYAN "========================================" C_RESET);
  Serial.println(C_BOLD "      🌿 SMART DIFFUSER V4.0 🌿        " C_RESET);
  Serial.println(C_CYAN "========================================" C_RESET);
  
  connectWiFi();
  
  Serial.print(C_YELLOW "[System] 로드셀 초기화 중..." C_RESET);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  Serial.println(C_GREEN " 완료!" C_RESET);
  
  printMainMenu(); 
}

// ============================================================
// [4] 메인 루프 (Loop)
// ============================================================
void loop() {
  manageWiFi();      // WiFi 관리
  systemHeartbeat(); // LED 깜빡임
  
  if (isRunning) {
    runSprayLogic(); // 모터 제어
    checkSafety();   // [Safety] 왓치독 체크
  }

  checkSerialInput(); // 사용자 입력
}

// ============================================================
// [5] 핵심 기능 함수들
// ============================================================

void runSprayLogic() {
  if (activePin == -1) return;
  unsigned long currentMillis = millis();
  
  if (isSpraying) {
    if (currentMillis - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH); // OFF
      isSpraying = false;
      prevMotorMillis = currentMillis;
      Serial.println(C_CYAN "      └── [Idle] ⏳ 휴식 중..." C_RESET);
    }
  } 
  else {
    if (currentMillis - prevMotorMillis >= REST_TIME) {
      forceAllOff(); 
      digitalWrite(activePin, LOW); // ON
      isSpraying = true;
      prevMotorMillis = currentMillis;
      Serial.printf(C_GREEN "      ┌── [Action] 💨 분사 시작! (%d초)\n" C_RESET, sprayDuration / 1000);
    }
  }
}

// [Safety] 30초 이상 연속 작동 시 강제 종료 (물리적 오류 대비)
void checkSafety() {
  if (millis() - startTimeMillis > MAX_RUN_TIME) {
    Serial.println(C_RED "\n🚨 [Emergency] 최대 작동 시간 초과! 시스템 강제 정지." C_RESET);
    stopSystem();
    currentMode = 0;
    printMainMenu();
  }
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    delay(100); 
    String input = Serial.readStringUntil('\n');
    input.trim();
    while(Serial.available() > 0) Serial.read(); 

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

void connectWiFi() {
  Serial.print(C_YELLOW "[System] WiFi Connecting" C_RESET);
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(250); Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(C_GREEN "\n[System] WiFi Connected! 📶" C_RESET);
  } else {
    Serial.println(C_RED "\n[System] WiFi Failed. (Offline Mode)" C_RESET);
  }
}

void manageWiFi() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevWifiMillis >= 30000) {
    prevWifiMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(C_RED "\n[Warning] WiFi 끊김. 재연결 시도..." C_RESET);
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}

void systemHeartbeat() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevLedMillis >= 1000) { 
    prevLedMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState);
  }
}

void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println(C_BLUE "\n--- [ Mode 1: 수동 제어 ] (1~4 입력) ---" C_RESET);
    } else if (input == "2") {
      currentMode = 2;
      Serial.println(C_BLUE "\n--- [ Mode 2: 감성 모드 ] (1~4 입력) ---" C_RESET);
    } else if (input == "3") {
      currentMode = 3;
      Serial.println(C_BLUE "\n--- [ Mode 3: 날씨 모드 ] (지역명 입력) ---" C_RESET);
    } else if (input == "4") { // [New] 로드셀 설정
      Serial.println(C_YELLOW "\n--- [ 무게 센서 설정 ] ---" C_RESET);
      Serial.println("👉 't' 입력: 영점 조절 (Tare)");
      Serial.println("👉 '0' 입력: 메뉴로 복귀");
      currentMode = 4;
    } else {
      Serial.println(C_RED "❌ 잘못된 입력입니다." C_RESET); printMainMenu();
    }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) runEmotionMode(input); 
  else if (currentMode == 3) runWeatherMode(input);
  else if (currentMode == 4) runScaleSetting(input); // [New]
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
  Serial.println(C_RED "\n⛔ [System] 작동 정지." C_RESET);
}

void startInterval(int pin, unsigned long duration) {
  forceAllOff();
  activePin = pin;
  isRunning = true;
  isSpraying = true; 
  sprayDuration = duration; 
  prevMotorMillis = millis(); 
  startTimeMillis = millis(); // 안전 타이머 시작
  
  digitalWrite(activePin, LOW); 
  Serial.println(C_GREEN "[Loop] 반복 작동 시작 (중단: '0')" C_RESET);
}

void runManualMode(String input) {
  isRunning = false; 
  int pin = -1;
  if (input == "1") pin = PIN_SUNNY;
  else if (input == "2") pin = PIN_CLOUDY;
  else if (input == "3") pin = PIN_RAIN;
  else if (input == "4") pin = PIN_SNOW;
  else { Serial.println(C_RED "⚠️ 1~4 입력" C_RESET); return; }

  forceAllOff();
  Serial.println(C_GREEN "[Manual] 3초간 작동..." C_RESET);
  digitalWrite(pin, LOW);
  delay(3000); 
  digitalWrite(pin, HIGH);
  Serial.println(C_CYAN "[Manual] 완료" C_RESET);
}

void runEmotionMode(String val) {
  String json = "{\"mode\": \"emotion\", \"user_emotion\": \"" + val + "\"}";
  Serial.println(C_YELLOW "[Emotion] 서버 요청..." C_RESET);
  sendServerRequest(json); 
}

void runWeatherMode(String region) {
  float w = 0.0;
  if (scale.is_ready()) w = scale.get_units(5);
  String json = "{\"mode\": \"weather\", \"region\": \"" + region + "\", \"weight\": " + String(w) + "}";
  Serial.println(C_YELLOW "[Weather] 서버 요청..." C_RESET);
  sendServerRequest(json); 
}

// [New] 로드셀 영점 잡기 함수
void runScaleSetting(String input) {
  if (input == "t") {
    Serial.print(C_YELLOW "⚖️ 영점 잡는 중..." C_RESET);
    scale.tare();
    Serial.println(C_GREEN " 완료! (현재 무게 0.0g)" C_RESET);
    Serial.println("👉 종료하려면 '0'을 입력하세요.");
  } else {
    Serial.println("⚠️ 't'를 입력하면 영점이 조절됩니다.");
  }
}

void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println(C_RED "🚨 WiFi 연결 안됨!" C_RESET); return;
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

    Serial.println(C_GREEN "✅ 수신 완료: " + txt + C_RESET);
    
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY;
    else if (cmd == 2) target = PIN_CLOUDY;
    else if (cmd == 3) target = PIN_RAIN;
    else if (cmd == 4) target = PIN_SNOW;

    if (target != -1) startInterval(target, dur * 1000);
    else { Serial.println(C_RED "⚠️ 정지 명령 수신" C_RESET); stopSystem(); }
  } else {
    Serial.printf(C_RED "🚨 통신 실패 (Code: %d)\n" C_RESET, code);
  }
  http.end();
}

void printMainMenu() {
  Serial.println(C_CYAN "\n=== 🕹️ MAIN MENU 🕹️ ===" C_RESET);
  Serial.println(" [1] 수동  [2] 감성  [3] 날씨");
  Serial.println(" [4] 무게 센서 설정 (Tare)");
  Serial.println(C_YELLOW "👉 입력 >>" C_RESET);
}
