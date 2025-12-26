/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 5.1 (Fixed: UI Indentation & Calibration Logic)
 * [작성자] 21학번 류재홍
 * [수정사항] 터미널 출력 계단 현상 수정, 보정값 조절 UI 개선
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ============================================================
// [0] ANSI 컬러 & UI 정의
// ============================================================
#define C_RESET  "\033[0m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_BLUE   "\033[34m"
#define C_MAGENTA "\033[35m"
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

// 타이머 설정
unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;   
const long MAX_RUN_TIME   = 40000; // 안전장치

// ============================================================
// [2] 전역 변수
// ============================================================
HX711 scale;
float calibration_factor = 430.0; // 초기값

int currentMode = 0;       
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;   

// 데모 모드용 변수
int demoStep = 0;
unsigned long prevDemoMillis = 0;

// 타이머 변수
unsigned long prevMotorMillis = 0; 
unsigned long prevWifiMillis = 0;
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; 
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

  // 멋진 부팅 로고
  Serial.println("\n\n");
  Serial.println(C_MAGENTA "****************************************" C_RESET);
  Serial.println(C_BOLD    "     🎓 SMART DIFFUSER SYSTEM V5.1 🎓     " C_RESET);
  Serial.println(C_MAGENTA "****************************************" C_RESET);
  Serial.println("Initializing System...");
  
  connectWiFi();
  
  Serial.print(C_YELLOW "[System] Loadcell Calibrating..." C_RESET);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  Serial.println(C_GREEN " DONE!" C_RESET);
  
  printMainMenu(); 
}

// ============================================================
// [4] 메인 루프 (Loop)
// ============================================================
void loop() {
  manageWiFi();      
  systemHeartbeat(); 
  
  // 모드별 동작 분기
  if (currentMode == 5) {
    runAutoDemoLoop(); // 전시회용 오토 데모
  }
  else if (isRunning) {
    runSprayLogic();   // 일반 분사 로직
    checkSafety();     
  }

  checkSerialInput(); 
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

// 전시회용 오토 데모 (순차 작동)
void runAutoDemoLoop() {
  unsigned long currentMillis = millis();
  
  // 5초마다 다음 단계로 넘어감
  if (currentMillis - prevDemoMillis >= 5000) {
    prevDemoMillis = currentMillis;
    forceAllOff(); // 이전꺼 끄기
    
    demoStep++;
    if (demoStep > 4) demoStep = 1; // 1~4 반복

    int target = -1;
    String name = "";
    
    if (demoStep == 1) { target = PIN_SUNNY; name = "☀️ 맑음"; }
    else if (demoStep == 2) { target = PIN_CLOUDY; name = "☁️ 흐림"; }
    else if (demoStep == 3) { target = PIN_RAIN; name = "☔ 비"; }
    else if (demoStep == 4) { target = PIN_SNOW; name = "❄️ 눈"; }

    Serial.print(C_MAGENTA "[Auto Demo] " C_RESET);
    Serial.println(name + " 모드 작동 (3초)");
    
    digitalWrite(target, LOW); // 켜기
    delay(3000); // 데모는 간단하게 delay 사용
    digitalWrite(target, HIGH); // 끄기
  }
}

void checkSafety() {
  if (millis() - startTimeMillis > MAX_RUN_TIME) {
    Serial.println(C_RED "\n🚨 [Emergency] 안전 타이머 작동! 강제 종료." C_RESET);
    stopSystem();
    currentMode = 0;
    printMainMenu();
  }
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    char c = Serial.peek(); // 첫 글자 살짝 보기
    
    // [New] 4번(설정) 모드일 때 실시간 키 입력 처리
    if (currentMode == 4) {
       char inputChar = Serial.read(); // 한 글자 읽기
       if (inputChar == '\n' || inputChar == '\r') return; // 엔터 무시
       
       if (inputChar == '+') {
         calibration_factor += 10;
         scale.set_scale(calibration_factor);
         // [수정됨] 계단 현상 방지를 위해 print + println 사용
         Serial.print("🔺 보정값 증가: ");
         Serial.print(calibration_factor, 1);
         Serial.print(" | 현재 무게: ");
         Serial.print(scale.get_units(), 2);
         Serial.println(" g"); 
       }
       else if (inputChar == '-') {
         calibration_factor -= 10;
         scale.set_scale(calibration_factor);
         // [수정됨] 계단 현상 방지를 위해 print + println 사용
         Serial.print("🔻 보정값 감소: ");
         Serial.print(calibration_factor, 1);
         Serial.print(" | 현재 무게: ");
         Serial.print(scale.get_units(), 2);
         Serial.println(" g");
       }
       else if (inputChar == 't') {
         scale.tare();
         Serial.println(C_GREEN "⚖️ 영점 조절 완료 (Tare)" C_RESET);
       }
       // [추가] 디버깅용 Raw 값 확인 (r 키 누름)
       else if (inputChar == 'r') {
         Serial.print("🔍 Raw Value: ");
         Serial.println(scale.read());
       }
       else if (inputChar == '0') {
         currentMode = 0;
         printMainMenu();
       }
       return; 
    }

    // 일반 입력 처리
    delay(50);
    String input = Serial.readStringUntil('\n');
    input.trim();
    
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
  while(WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(200); Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(C_GREEN "\n[System] WiFi Connected! (Signal: " C_RESET);
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm)");
  } else {
    Serial.println(C_RED "\n[System] WiFi Failed. (Offline Mode)" C_RESET);
  }
}

void manageWiFi() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevWifiMillis >= 30000) {
    prevWifiMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println(C_RED "\n[Warning] WiFi Reconnecting..." C_RESET);
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
      currentMode = 1; Serial.println(C_BLUE "\n--- [ Mode 1: 수동 제어 ] (1~4 입력) ---" C_RESET);
    } else if (input == "2") {
      currentMode = 2; Serial.println(C_BLUE "\n--- [ Mode 2: 감성 모드 ] (1~4 입력) ---" C_RESET);
    } else if (input == "3") {
      currentMode = 3; Serial.println(C_BLUE "\n--- [ Mode 3: 날씨 모드 ] (지역명 입력) ---" C_RESET);
    } else if (input == "4") {
      currentMode = 4;
      Serial.println(C_YELLOW "\n--- [ 🛠️ 실시간 정밀 세팅 ] ---" C_RESET);
      Serial.println("👉 '+' : 보정값 증가 / '-' : 보정값 감소");
      Serial.println("👉 't' : 영점 잡기 (Tare)");
      Serial.println("👉 'r' : 센서 원본값 확인 (Raw Data)"); // 도움말 추가
      Serial.println("👉 '0' : 나가기");
    } else if (input == "5") {
      currentMode = 5;
      demoStep = 0;
      Serial.println(C_MAGENTA "\n--- [ ✨ 전시회 오토 데모 모드 ] ---" C_RESET);
      Serial.println("자동으로 모든 향기를 순환합니다. (종료: 0)");
    } else {
      Serial.println(C_RED "❌ 잘못된 입력입니다." C_RESET); printMainMenu();
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
  Serial.println(C_RED "\n⛔ [System] 시스템 대기 상태." C_RESET);
}

void startInterval(int pin, unsigned long duration) {
  forceAllOff();
  activePin = pin;
  isRunning = true;
  isSpraying = true; 
  sprayDuration = duration; 
  prevMotorMillis = millis(); 
  startTimeMillis = millis(); 
  
  digitalWrite(activePin, LOW); 
  Serial.println(C_GREEN "[Loop] 작동 시작 (중단: '0')" C_RESET);
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
  Serial.println(C_YELLOW "[Emotion] 서버 분석 요청..." C_RESET);
  sendServerRequest(json); 
}

void runWeatherMode(String region) {
  float w = 0.0;
  if (scale.is_ready()) w = scale.get_units(5);
  String json = "{\"mode\": \"weather\", \"region\": \"" + region + "\", \"weight\": " + String(w) + "}";
  Serial.println(C_YELLOW "[Weather] 서버 날씨 조회..." C_RESET);
  sendServerRequest(json); 
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

    Serial.println(C_GREEN "✅ " + txt + C_RESET);
    
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY;
    else if (cmd == 2) target = PIN_CLOUDY;
    else if (cmd == 3) target = PIN_RAIN;
    else if (cmd == 4) target = PIN_SNOW;

    if (target != -1) startInterval(target, dur * 1000);
    else { Serial.println(C_RED "⚠️ 명령 없음 (대기)" C_RESET); stopSystem(); }
  } else {
    Serial.printf(C_RED "🚨 통신 에러 (Code: %d)\n" C_RESET, code);
  }
  http.end();
}

void printMainMenu() {
  Serial.println(C_CYAN "\n=== 🕹️ MAIN MENU (V5.1) 🕹️ ===" C_RESET);
  Serial.println(" [1] 수동   [2] 감성   [3] 날씨");
  Serial.println(" [4] 🛠️ 정밀 세팅 (Calibration)");
  Serial.println(" [5] ✨ 오토 데모 (Auto Show)");
  Serial.println(C_YELLOW "👉 명령 입력 >>" C_RESET);
}
