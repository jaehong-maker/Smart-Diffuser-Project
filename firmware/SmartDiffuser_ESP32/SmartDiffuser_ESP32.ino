/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 6.1 (Fixed: Terminal Layout & Staircase Effect)
 * [작성자] 21학번 류재홍
 * [수정사항] 
 * 1. 터미널 출력 시 '\r\n' 강제 적용으로 계단 현상 해결
 * 2. 시스템 대시보드 정렬 깨짐 수정
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h> 

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
const long MAX_RUN_TIME   = 40000; 

// ============================================================
// [2] 전역 변수
// ============================================================
HX711 scale;
Preferences prefs;

float calibration_factor = 430.0; 

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

  // [수정] 줄바꿈을 println으로 통일하여 계단 현상 방지
  Serial.println();
  Serial.println();
  Serial.println(C_MAGENTA "****************************************" C_RESET);
  Serial.println(C_BOLD    "   🏆 SMART DIFFUSER SYSTEM V6.1 🏆     " C_RESET);
  Serial.println(C_MAGENTA "****************************************" C_RESET);
  Serial.println("Initializing System...");

  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  
  if (savedFactor != 0.0) {
    calibration_factor = savedFactor;
    Serial.print(C_CYAN "[Memory] 저장된 보정값 로드됨: ");
    Serial.println(calibration_factor);
    Serial.print(C_RESET); // 색상 초기화
  } else {
    Serial.println(C_YELLOW "[Memory] 저장된 값이 없어 기본값 사용" C_RESET);
  }

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
  
  if (currentMode == 5) {
    runAutoDemoLoop(); 
  }
  else if (isRunning) {
    runSprayLogic();   
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
      digitalWrite(activePin, HIGH); 
      isSpraying = false;
      prevMotorMillis = currentMillis;
      
      if (currentMode == 1) {
          Serial.println(C_CYAN "      └── [Manual] 동작 완료. 대기 상태로 전환." C_RESET);
          stopSystem();
          printMainMenu();
          return;
      }
      Serial.println(C_CYAN "      └── [Idle] ⏳ 휴식 중..." C_RESET);
    }
  } 
  else {
    if (currentMillis - prevMotorMillis >= REST_TIME) {
      forceAllOff(); 
      digitalWrite(activePin, LOW); 
      isSpraying = true;
      prevMotorMillis = currentMillis;
      
      // [수정] printf 대신 print+println 사용 (안전함)
      Serial.print(C_GREEN "      ┌── [Action] 💨 재분사 시작! (");
      Serial.print(sprayDuration / 1000);
      Serial.println("초)" C_RESET);
    }
  }
}

void runAutoDemoLoop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - prevDemoMillis >= 4000) {
    prevDemoMillis = currentMillis;
    forceAllOff(); 
    
    demoStep++;
    if (demoStep > 4) demoStep = 1; 

    int target = -1;
    String name = "";
    
    if (demoStep == 1) { target = PIN_SUNNY; name = "☀️ 맑음"; }
    else if (demoStep == 2) { target = PIN_CLOUDY; name = "☁️ 흐림"; }
    else if (demoStep == 3) { target = PIN_RAIN; name = "☔ 비"; }
    else if (demoStep == 4) { target = PIN_SNOW; name = "❄️ 눈"; }

    Serial.print(C_MAGENTA "[Auto Demo] " C_RESET);
    Serial.print(name);
    Serial.println(" 모드 작동"); // println 사용
    
    digitalWrite(target, LOW); 
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
    char c = Serial.peek(); 
    
    if (currentMode == 4) {
       char inputChar = Serial.read(); 
       if (inputChar == '\n' || inputChar == '\r') return; 
       
       if (inputChar == '+') {
         calibration_factor += 10;
         scale.set_scale(calibration_factor);
         printCalibrationInfo();
       }
       else if (inputChar == '-') {
         calibration_factor -= 10;
         scale.set_scale(calibration_factor);
         printCalibrationInfo();
       }
       else if (inputChar == 't') {
         scale.tare();
         Serial.println(C_GREEN "⚖️ 영점 조절 완료 (Tare)" C_RESET);
       }
       else if (inputChar == 'r') {
         Serial.print("🔍 Raw Value: "); 
         Serial.println(scale.read());
       }
       else if (inputChar == 's') { 
         prefs.putFloat("cal_factor", calibration_factor);
         Serial.println(C_BLUE "💾 [Save] 보정값이 내장 메모리에 저장되었습니다!" C_RESET);
       }
       else if (inputChar == '0') {
         currentMode = 0;
         printMainMenu();
       }
       return; 
    }

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

// [수정] 보정값 출력 포맷 수정
void printCalibrationInfo() {
    Serial.print("📡 보정값: ");
    Serial.print(calibration_factor, 1);
    Serial.print(" | 현재 무게: ");
    Serial.print(scale.get_units(5), 2); 
    Serial.println(" g"); // 여기서 println으로 줄바꿈 확정
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
    Serial.println(); // 줄바꿈
    Serial.print(C_GREEN "[System] WiFi Connected! (" C_RESET);
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
  long interval = isRunning ? 200 : 1000;

  if (currentMillis - prevLedMillis >= interval) { 
    prevLedMillis = currentMillis;
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState);
  }
}

// [수정] 대시보드 출력 방식 전면 수정 (계단현상 방지)
void printDashboard() {
    Serial.println(C_CYAN "\n📊 [ SYSTEM DASHBOARD ] 📊" C_RESET);
    
    // 1. WiFi RSSI
    Serial.print(" ├─ WiFi RSSI  : ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // 2. Uptime
    Serial.print(" ├─ Uptime     : ");
    Serial.print(millis() / 1000);
    Serial.println(" sec");

    // 3. Calibration Factor
    Serial.print(" ├─ Cal.Factor : ");
    Serial.print(calibration_factor, 1);
    Serial.println(" (Saved)");
    
    // 4. Weight
    float w = 0.0;
    if(scale.is_ready()) w = scale.get_units(10); 
    Serial.print(" └─ Weight     : ");
    Serial.print(w, 2);
    Serial.println(" g");
    
    Serial.println("----------------------------");
    printMainMenu();
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
      Serial.println(C_YELLOW "\n--- [ 🛠️ 정밀 세팅 & 저장 ] ---" C_RESET);
      Serial.println("👉 +/- : 보정값 조절");
      Serial.println("👉 't' : 영점 (Tare)");
      Serial.println("👉 's' : 저장 (Save to Memory) ★");
      Serial.println("👉 'r' : Raw Data 확인");
      Serial.println("👉 '0' : 나가기");
    } else if (input == "5") {
      currentMode = 5;
      demoStep = 0;
      Serial.println(C_MAGENTA "\n--- [ ✨ 전시회 오토 데모 모드 ] ---" C_RESET);
      Serial.println("자동으로 모든 향기를 순환합니다. (종료: 0)");
    } else if (input == "9") { 
       printDashboard();
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
  Serial.println(C_GREEN "[Loop] 분사 프로세스 시작 (중단: '0')" C_RESET);
}

void runManualMode(String input) {
  int pin = -1;
  if (input == "1") pin = PIN_SUNNY;
  else if (input == "2") pin = PIN_CLOUDY;
  else if (input == "3") pin = PIN_RAIN;
  else if (input == "4") pin = PIN_SNOW;
  else { Serial.println(C_RED "⚠️ 1~4 입력" C_RESET); return; }

  startInterval(pin, 3000); 
}

void runEmotionMode(String val) {
  String json = "{\"mode\": \"emotion\", \"user_emotion\": \"" + val + "\"}";
  Serial.println(C_YELLOW "[Emotion] 서버 분석 요청..." C_RESET);
  sendServerRequest(json); 
}

void runWeatherMode(String region) {
  float w = 0.0;
  if (scale.is_ready()) w = scale.get_units(10); 
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
    Serial.print(C_RED "🚨 통신 에러 (Code: ");
    Serial.print(code);
    Serial.println(")" C_RESET);
  }
  http.end();
}

void printMainMenu() {
  Serial.println(C_CYAN "\n=== 🕹️ MAIN MENU (V6.1 Layout Fixed) 🕹️ ===" C_RESET);
  Serial.println(" [1] 수동   [2] 감성   [3] 날씨");
  Serial.println(" [4] 🛠️ 정밀 세팅 (값 저장 가능)");
  Serial.println(" [5] ✨ 오토 데모 (Exhibition)");
  Serial.println(" [9] 📊 시스템 대시보드");
  Serial.println(C_YELLOW "👉 명령 입력 >>" C_RESET);
}
