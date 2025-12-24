/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [플랫폼] ESP32 Dev Module
 * [작성일] 2025. 12. 24
 * [버전] Final Release (Log Optimized)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ============================================================
// [1] 네트워크 및 서버 설정
// ============================================================
const char* ssid     = "Jaehong_WiFi";    
const char* password = "12345678";        

String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// ============================================================
// [2] 타이머 설정
// ============================================================
const long SPRAY_TIME = 5000; // 5초 작동
const long REST_TIME  = 5000; // 5초 휴식

// ============================================================
// [3] 핀 번호 할당
// ============================================================
const int PIN_SUNNY  = 26; // 1번
const int PIN_CLOUDY = 27; // 2번
const int PIN_RAIN   = 14; // 3번
const int PIN_SNOW   = 13; // 4번

const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;   

// ============================================================
// [4] 전역 변수 및 객체
// ============================================================
HX711 scale;
float calibration_factor = 430.0; 

int currentMode = 0;      
bool isRunning = false;   
int activePin = -1;       
bool isSpraying = false;  
unsigned long previousMillis = 0; 

// ============================================================
// [5] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);

  allStop(false); // 초기화 (로그 없이 조용히)

  // WiFi 연결
  Serial.println("\n\n========================================");
  Serial.println("      🌿 SMART DIFFUSER SYSTEM 🌿      ");
  Serial.println("========================================");
  Serial.print("[System] WiFi Connecting");
  
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[System] WiFi Connected! 📶");
  
  // 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  printMainMenu(); 
}

// ============================================================
// [6] 메인 루프 (Loop)
// ============================================================
void loop() {
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return; 
  }

  // 반복 작동 로직 (Non-blocking)
  if (isRunning && activePin != -1) {
    unsigned long currentMillis = millis();
    
    if (isSpraying) {
      // 분사 -> 휴식 전환
      if (currentMillis - previousMillis >= SPRAY_TIME) {
        digitalWrite(activePin, HIGH); 
        isSpraying = false;
        previousMillis = currentMillis;
        Serial.println("      └── [Idle] ⏳ 5초간 휴식 중...");
      }
    } 
    else {
      // 휴식 -> 분사 전환
      if (currentMillis - previousMillis >= REST_TIME) {
        digitalWrite(activePin, LOW);
        isSpraying = true;
        previousMillis = currentMillis;
        Serial.println("      ┌── [Action] 💨 5초간 향기 분사!");
      }
    }
  }

  // 사용자 입력 처리
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    while(Serial.available()) Serial.read(); 

    if (input.length() > 0) {
      if (input == "0") {
        allStop(true); // 로그 출력하며 정지
        currentMode = 0;
        printMainMenu();
        return;
      }
      handleInput(input);
    }
  }
}

// ============================================================
// [7] 사용자 입력 처리 핸들러
// ============================================================
void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println("\n---------------- [ Mode 1: 수동 제어 ] ----------------");
      Serial.println("👉 작동시킬 모터 번호(1~4)를 입력하세요. (종료: 0)");
    }
    else if (input == "2") {
      currentMode = 2;
      Serial.println("\n---------------- [ Mode 2: 감성 모드 ] ----------------");
      Serial.println("👉 현재 기분을 입력하세요 (예: 기쁨, 슬픔). (종료: 0)");
    }
    else if (input == "3") {
      currentMode = 3;
      Serial.println("\n---------------- [ Mode 3: 날씨 모드 ] ----------------");
      Serial.println("👉 지역 이름을 입력하세요 (예: 서울, 부산). (종료: 0)");
    }
    else {
      Serial.println("❌ [Error] 잘못된 입력입니다. 1~3 중에서 선택하세요.");
      printMainMenu();
    }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) setupEmotionMode(input);
  else if (currentMode == 3) setupWeatherMode(input);
}

// ============================================================
// [8] 세부 기능 함수들
// ============================================================

void printMainMenu() {
  Serial.println("\n========================================");
  Serial.println("       🕹️  M A I N   M E N U  🕹️       ");
  Serial.println("========================================");
  Serial.println("  [1] 수동 모드 (Manual Control)");
  Serial.println("  [2] 감성 모드 (Emotion Loop)");
  Serial.println("  [3] 날씨 모드 (Weather Loop)");
  Serial.println("========================================");
  Serial.println("👉 모드 번호를 입력하세요 >>");
}

void allStop(bool showLog) {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
  
  isRunning = false;
  activePin = -1;
  isSpraying = false;
  if(showLog) Serial.println("\n⛔ [System] 작동 정지. 메인 메뉴로 복귀합니다.");
}

// [모드 1] 수동 제어
void runManualMode(String input) {
  isRunning = false; 
  int pin = -1;
  String modeName = "";
  
  if (input == "1") { pin = PIN_SUNNY; modeName = "1번 (맑음/기쁨)"; }
  else if (input == "2") { pin = PIN_CLOUDY; modeName = "2번 (흐림/평온)"; }
  else if (input == "3") { pin = PIN_RAIN; modeName = "3번 (비/슬픔)"; }
  else if (input == "4") { pin = PIN_SNOW; modeName = "4번 (눈/화남)"; }
  else { Serial.println("⚠️ 1~4번 사이의 숫자를 입력해주세요."); return; }

  Serial.println("\n[Manual] 수동 제어 시작");
  Serial.print("   Target: "); Serial.println(modeName);
  Serial.println("   Status: 💨 5초간 분사 중...");
  
  digitalWrite(pin, LOW);
  delay(5000); 
  digitalWrite(pin, HIGH);
  
  Serial.println("   Status: ✅ 작동 완료 (대기)");
}

// [모드 2] 감성 모드
void setupEmotionMode(String emotion) {
  int targetPin = -1;
  String scentName = "";
  
  if (emotion == "기쁨" || emotion == "행복") {
    targetPin = PIN_SUNNY; scentName = "🍊 상큼한 시트러스";
  }
  else if (emotion == "평온" || emotion == "휴식") {
    targetPin = PIN_CLOUDY; scentName = "☁️ 포근한 코튼";
  }
  else if (emotion == "슬픔" || emotion == "우울") {
    targetPin = PIN_RAIN; scentName = "💧 차분한 아쿠아";
  }
  else if (emotion == "화남" || emotion == "스트레스") {
    targetPin = PIN_SNOW; scentName = "🌿 시원한 민트";
  }
  else { 
    Serial.println("⚠️ [System] 인식할 수 없는 감정 키워드입니다."); 
    return; 
  }

  Serial.println("\n[Emotion] 감정 분석 결과");
  Serial.print("   Input : "); Serial.println(emotion);
  Serial.print("   Scent : "); Serial.println(scentName);
  
  startInterval(targetPin); 
}

// [모드 3] 날씨 모드
void setupWeatherMode(String region) {
  HTTPClient http;
  http.setTimeout(3000); 
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  float weight = 0.0;
  if (scale.is_ready()) weight = scale.get_units(5);
  
  String jsonPayload = "{\"weight\": " + String(weight) + ", \"region\": \"" + region + "\", \"message\": \"Mode3\"}";
  
  Serial.print("\n[API] 서버 데이터 요청 중... ");
  int httpCode = http.POST(jsonPayload);

  if(httpCode > 0){
    String response = http.getString();
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, response);

    if(!error) {
      int command = doc["spray"]; 
      String weather = doc["weather"];
      String regionName = doc["region"];
      
      Serial.println("완료 ✅");
      Serial.println("[API] 데이터 수신 결과");
      Serial.print("   Region  : "); Serial.println(regionName);
      Serial.print("   Weather : "); Serial.println(weather);

      int targetPin = -1;
      if (command == 1) targetPin = PIN_SUNNY;
      else if (command == 2) targetPin = PIN_CLOUDY;
      else if (command == 3) targetPin = PIN_RAIN;
      else if (command == 4) targetPin = PIN_SNOW;

      if (targetPin != -1) startInterval(targetPin);
      else Serial.println("⚠️ [System] 해당 날씨에 매칭된 동작이 없습니다.");
    } else {
       Serial.println("실패 ❌");
       Serial.println("⚠️ [Error] JSON 파싱 에러");
    }
  } else {
    Serial.println("실패 ❌");
    Serial.print("⚠️ [Error] 서버 통신 실패 (Code: "); Serial.print(httpCode); Serial.println(")");
  }
  http.end();
}

// [공통] 인터벌 작동 시작
void startInterval(int pin) {
  // 기존 핀 끄기
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);

  activePin = pin;
  isRunning = true;
  isSpraying = true; 
  previousMillis = millis(); 
  
  digitalWrite(activePin, LOW); // 즉시 시작
  Serial.println("[System] 반복 작동 모드 시작 (중단: '0')");
  Serial.println("      ┌── [Action] 💨 5초간 향기 분사!");
}
