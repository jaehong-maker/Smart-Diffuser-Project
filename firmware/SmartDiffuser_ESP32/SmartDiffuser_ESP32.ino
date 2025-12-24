/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 2.2 (Safety Interlock Added)
 * [수정내용] 모터 작동 전 '나머지 강제 종료' 로직 추가 (과부하 방지)
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
unsigned long sprayDuration = 3000; // 서버에서 받은 분사 시간
const long REST_TIME      = 5000;   // 휴식 시간

// ============================================================
// [3] 핀 번호 할당
// ============================================================
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 

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

  // 초기 상태: 안전하게 모두 끄기
  forceAllOff(); 

  Serial.println("\n\n========================================");
  Serial.println("      🌿 SMART DIFFUSER SYSTEM 🌿      ");
  Serial.println("========================================");
  
  // WiFi 연결
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

  // 타이머 로직
  if (isRunning && activePin != -1) {
    unsigned long currentMillis = millis();
    
    if (isSpraying) {
      // 분사 -> 휴식
      if (currentMillis - previousMillis >= sprayDuration) {
        digitalWrite(activePin, HIGH); // 끄기
        isSpraying = false;
        previousMillis = currentMillis;
        Serial.println("      └── [Idle] ⏳ 휴식 중...");
      }
    } 
    else {
      // 휴식 -> 분사
      if (currentMillis - previousMillis >= REST_TIME) {
        
        // ★ [안전장치] 켜기 전에 무조건 다 끄고 시작 (중복 방지)
        forceAllOff(); 
        
        // 타겟 핀만 켜기
        digitalWrite(activePin, LOW); 
        
        isSpraying = true;
        previousMillis = currentMillis;
        Serial.print("      ┌── [Action] 💨 분사 시작! (");
        Serial.print(sprayDuration / 1000);
        Serial.println("초)");
      }
    }
  }

  // 사용자 입력 처리
  if (Serial.available() > 0) {
    delay(200); 
    String input = Serial.readStringUntil('\n');
    input.trim();
    while(Serial.available() > 0) Serial.read(); 

    if (input.length() > 0) {
      if (input == "0") {
        stopSystem(); 
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
      Serial.println("👉 현재 기분을 번호로 선택하세요. (종료: 0)");
      Serial.println("   [1] 신남 (Happy)");
      Serial.println("   [2] 편안 (Relaxed)");
      Serial.println("   [3] 화남 (Angry)");
      Serial.println("   [4] 슬픔 (Sad)");
    }
    else if (input == "3") {
      currentMode = 3;
      Serial.println("\n---------------- [ Mode 3: 날씨 모드 ] ----------------");
      Serial.println("👉 지역 이름을 입력하세요 (예: 서울, 부산). (종료: 0)");
    }
    else {
      Serial.println("❌ [Error] 잘못된 입력입니다.");
      printMainMenu();
    }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) runEmotionMode(input); 
  else if (currentMode == 3) runWeatherMode(input); 
}

// ============================================================
// [8] 세부 기능 함수들
// ============================================================

void printMainMenu() {
  Serial.println("\n========================================");
  Serial.println("        🕹️  M A I N   M E N U  🕹️        ");
  Serial.println("========================================");
  Serial.println("  [1] 수동 모드 (Manual Control)");
  Serial.println("  [2] 감성 모드 (Time-based Logic)");
  Serial.println("  [3] 날씨 모드 (Weather Loop)");
  Serial.println("========================================");
  Serial.println("👉 모드 번호를 입력하세요 >>");
}

// ★ [안전 함수 1] 물리적으로 모든 핀 끄기 (변수 변경 없음)
void forceAllOff() {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
}

// ★ [안전 함수 2] 시스템 논리 정지 (변수 초기화 포함)
void stopSystem() {
  forceAllOff(); // 물리적 끄기
  isRunning = false;
  activePin = -1;
  isSpraying = false;
  Serial.println("\n⛔ [System] 작동 정지. 메인 메뉴로 복귀합니다.");
}

// [모드 1] 수동 제어
void runManualMode(String input) {
  isRunning = false; 
  int pin = -1;
  String modeName = "";
  
  if (input == "1") { pin = PIN_SUNNY; modeName = "1번"; }
  else if (input == "2") { pin = PIN_CLOUDY; modeName = "2번"; }
  else if (input == "3") { pin = PIN_RAIN; modeName = "3번"; }
  else if (input == "4") { pin = PIN_SNOW; modeName = "4번"; }
  else { Serial.println("⚠️ 1~4번 사이의 숫자를 입력해주세요."); return; }

  // ★ 수동 작동 전에도 안전하게 다 끄기
  forceAllOff();

  Serial.println("\n[Manual] 수동 테스트 동작");
  Serial.print("   Target: "); Serial.println(modeName);
  
  digitalWrite(pin, LOW); // 타겟만 켜기
  delay(3000); 
  digitalWrite(pin, HIGH);
  
  Serial.println("   Status: ✅ 테스트 완료");
}

// [모드 2] 감성 모드
void runEmotionMode(String emotionInput) {
  if (emotionInput != "1" && emotionInput != "2" && emotionInput != "3" && emotionInput != "4") {
    Serial.println("⚠️ 1~4 사이의 번호를 입력해주세요.");
    return;
  }
  String jsonPayload = "{\"mode\": \"emotion\", \"user_emotion\": \"" + emotionInput + "\", \"device\": \"ESP32\"}";
  Serial.print("\n[Emotion] 서버 분석 요청 중... Input: "); Serial.println(emotionInput);
  sendServerRequest(jsonPayload); 
}

// [모드 3] 날씨 모드
void runWeatherMode(String region) {
  float weight = 0.0;
  if (scale.is_ready()) weight = scale.get_units(5);
  String jsonPayload = "{\"mode\": \"weather\", \"region\": \"" + region + "\", \"weight\": " + String(weight) + "}";
  Serial.print("\n[Weather] 서버 날씨 조회 중... Region: "); Serial.println(region);
  sendServerRequest(jsonPayload); 
}

// [통합] 서버 요청
void sendServerRequest(String payload) {
  HTTPClient http;
  http.setTimeout(5000); 
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);

  if(httpCode > 0){
    String response = http.getString();
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, response);

    if(!error) {
      int command = doc["spray"]; 
      String text = doc["result_text"];
      int durationSec = doc["duration"]; 

      Serial.println("완료 ✅");
      Serial.println("[Result] 서버 응답 결과");
      Serial.print("   상태/시간 : "); Serial.println(text);
      Serial.print("   분사 시간 : "); Serial.print(durationSec); Serial.println("초");
      Serial.print("   모터 번호 : "); Serial.println(command);

      int targetPin = -1;
      if (command == 1) targetPin = PIN_SUNNY;
      else if (command == 2) targetPin = PIN_CLOUDY;
      else if (command == 3) targetPin = PIN_RAIN;
      else if (command == 4) targetPin = PIN_SNOW;

      if (targetPin != -1) {
        startInterval(targetPin, durationSec * 1000); 
      } else {
        Serial.println("⚠️ [System] 작동 코드가 0입니다. (정지/에러)");
        stopSystem();
      }
    } else {
       Serial.println("실패 ❌ (JSON 파싱 에러)");
    }
  } else {
    Serial.print("실패 ❌ (HTTP Code: "); Serial.print(httpCode); Serial.println(")");
  }
  http.end();
}

// 인터벌 작동 시작
void startInterval(int pin, unsigned long duration) {
  // ★ 시작할 때도 안전하게 초기화
  forceAllOff();

  activePin = pin;
  isRunning = true;
  isSpraying = true; 
  sprayDuration = duration; 
  previousMillis = millis(); 
  
  digitalWrite(activePin, LOW); // 즉시 시작
  Serial.println("[Loop] 반복 작동 시작 (중단하려면 '0' 입력)");
}
