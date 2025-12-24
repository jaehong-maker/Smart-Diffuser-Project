/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [플랫폼] ESP32 Dev Module
 * [작성일] 2025. 12. 24
 * [설명]
 * - AWS Lambda 서버를 통해 실시간 날씨 정보를 받아 해당 날씨에 맞는 향기를 분사
 * - 사용자의 감정(텍스트)을 입력받아 알맞은 향기를 추천 및 분사
 * - millis()를 활용한 Non-blocking 제어로, 작동 중에도 즉시 정지 가능
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

// AWS Lambda 함수 URL (API Gateway 대체)
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// ============================================================
// [2] 타이머 설정 (밀리초 단위)
// ============================================================
// 인터벌 모드 작동 시간 설정
const long SPRAY_TIME = 5000; // 분사 시간 (5초)
const long REST_TIME  = 5000; // 휴식 시간 (5초)

// ============================================================
// [3] 핀 번호 할당 (Hardware Pin Map)
// ============================================================
// 펌프/모터 제어 핀 (Active LOW 방식)
const int PIN_SUNNY  = 26; // 1번: 맑음 / 기쁨
const int PIN_CLOUDY = 27; // 2번: 흐림 / 평온
const int PIN_RAIN   = 14; // 3번: 비   / 슬픔
const int PIN_SNOW   = 13; // 4번: 눈   / 화남

// 로드셀(무게 센서) 핀
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;   

// ============================================================
// [4] 전역 변수 및 객체 선언
// ============================================================
HX711 scale;
float calibration_factor = 430.0; // 로드셀 보정값

// 상태 관리 변수
int currentMode = 0;      // 0:메뉴대기, 1:수동, 2:감성, 3:날씨
bool isRunning = false;   // 현재 시스템 작동 여부
int activePin = -1;       // 현재 작동 중인 핀 번호
bool isSpraying = false;  // 분사 상태 (true:분사중, false:휴식중)
unsigned long previousMillis = 0; // 비차단 딜레이를 위한 시간 저장 변수

// ============================================================
// [5] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);

  // 핀 모드 설정
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);

  // 초기 상태: 모든 모터 정지 (Active LOW이므로 HIGH가 OFF)
  allStop(); 

  // 와이파이 연결
  WiFi.begin(ssid, password);
  Serial.print("\n📶 WiFi 연결 시도 중");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi 연결 성공!");
  
  // 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  // 메인 메뉴 출력
  printMainMenu(); 
}

// ============================================================
// [6] 메인 루프 (Loop)
// ============================================================
void loop() {
  // 1. 와이파이 연결 상태 확인 (끊김 시 재접속)
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return; 
  }

  // 2. 반복 작동 로직 (Non-blocking: 멈추지 않고 시간 체크)
  if (isRunning && activePin != -1) {
    unsigned long currentMillis = millis();
    
    // [상태 1] 분사 중일 때 -> 설정된 시간이 지나면 휴식으로 전환
    if (isSpraying) {
      if (currentMillis - previousMillis >= SPRAY_TIME) {
        digitalWrite(activePin, HIGH); // 모터 끄기
        isSpraying = false;
        previousMillis = currentMillis;
        Serial.println("   ⏳ 완료 -> 💤 휴식 중...");
      }
    } 
    // [상태 2] 휴식 중일 때 -> 설정된 시간이 지나면 다시 분사
    else {
      if (currentMillis - previousMillis >= REST_TIME) {
        digitalWrite(activePin, LOW); // 모터 켜기
        isSpraying = true;
        previousMillis = currentMillis;
        Serial.println("   💦 분사 시작 (ON)!");
      }
    }
  }

  // 3. 사용자 입력 처리 (시리얼 통신)
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // 시리얼 버퍼 비우기 (중복 입력 방지)
    while(Serial.available()) Serial.read(); 

    if (input.length() > 0) {
      // '0' 입력 시: 긴급 정지 및 메인 메뉴 복귀
      if (input == "0") {
        allStop(); 
        currentMode = 0;
        printMainMenu();
        return;
      }

      // 입력값 처리 핸들러 호출
      handleInput(input);
    }
  }
}

// ============================================================
// [7] 사용자 입력 처리 및 모드 관리
// ============================================================
void handleInput(String input) {
  // 메인 메뉴 상태일 때 모드 선택
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println("\n============== [ 1. 수동 모드 ] ==============");
      Serial.println("👉 작동시킬 핀 번호(1~4)를 입력하세요. (종료: 0)");
    }
    else if (input == "2") {
      currentMode = 2;
      Serial.println("\n============== [ 2. 감성 모드 ] ==============");
      Serial.println("👉 현재 기분을 입력하세요 (예: 기쁨, 슬픔). (종료: 0)");
    }
    else if (input == "3") {
      currentMode = 3;
      Serial.println("\n============== [ 3. 날씨 모드 ] ==============");
      Serial.println("👉 날씨를 조회할 지역을 입력하세요. (종료: 0)");
    }
    else {
      Serial.println("❌ 잘못된 입력입니다. 1, 2, 3 중에서 선택해주세요.");
      printMainMenu();
    }
  }
  // 선택된 모드에 따른 세부 동작 수행
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) setupEmotionMode(input);
  else if (currentMode == 3) setupWeatherMode(input);
}

// ============================================================
// [8] 세부 기능 함수들
// ============================================================

// 메인 메뉴 UI 출력
void printMainMenu() {
  Serial.println("\n##################################################");
  Serial.println("#           🌟 스마트 디퓨저 메인 메뉴 🌟          #");
  Serial.println("##################################################");
  Serial.println("#  [1] 수동 모드 (Manual Control)                #");
  Serial.println("#  [2] 감성 모드 (Emotion Based Loop)            #");
  Serial.println("#  [3] 날씨 모드 (Weather API Loop)              #");
  Serial.println("##################################################");
  Serial.println("👉 원하시는 모드 번호(1, 2, 3)를 입력하세요:");
}

// 모든 모터 정지 및 상태 초기화
void allStop() {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
  
  isRunning = false;
  activePin = -1;
  isSpraying = false;
  Serial.println("⛔ [System] 모든 작동을 중지하고 대기합니다.");
}

// [모드 1] 수동 제어 (1회성 작동)
void runManualMode(String input) {
  // 수동 모드는 반복이 아니므로 반복 루프 비활성화
  isRunning = false; 
  int pin = -1;
  
  if (input == "1") pin = PIN_SUNNY;
  else if (input == "2") pin = PIN_CLOUDY;
  else if (input == "3") pin = PIN_RAIN;
  else if (input == "4") pin = PIN_SNOW;
  else { Serial.println("⚠️ 1~4번 숫자만 입력해주세요."); return; }

  Serial.print(">>> 수동 제어: "); Serial.print(input); Serial.println("번 모터 5초간 작동!");
  
  // 수동 제어는 단순 동작이므로 delay 사용 (간단 구현)
  digitalWrite(pin, LOW);
  delay(5000); 
  digitalWrite(pin, HIGH);
  Serial.println(">>> 작동 완료.");
}

// [모드 2] 감성 모드 설정 (텍스트 -> 핀 매핑)
void setupEmotionMode(String emotion) {
  int targetPin = -1;
  String scentName = "";
  
  if (emotion == "기쁨" || emotion == "행복") {
    targetPin = PIN_SUNNY; scentName = "상큼한 시트러스";
  }
  else if (emotion == "평온" || emotion == "휴식") {
    targetPin = PIN_CLOUDY; scentName = "포근한 코튼";
  }
  else if (emotion == "슬픔" || emotion == "우울") {
    targetPin = PIN_RAIN; scentName = "차분한 아쿠아";
  }
  else if (emotion == "화남" || emotion == "스트레스") {
    targetPin = PIN_SNOW; scentName = "시원한 민트";
  }
  else { 
    Serial.println("⚠️ 인식할 수 없는 감정입니다."); 
    return; 
  }

  Serial.print("💖 감정 인식: "); Serial.println(emotion);
  Serial.print("🌸 추천 향기: "); Serial.println(scentName);
  
  startInterval(targetPin); // 반복 작동 시작
}

// [모드 3] 날씨 모드 설정 (AWS 서버 통신)
void setupWeatherMode(String region) {
  HTTPClient http;
  http.setTimeout(3000); // 3초 타임아웃
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  // 현재 무게 측정
  float weight = 0.0;
  if (scale.is_ready()) weight = scale.get_units(5);
  
  // JSON 데이터 전송
  String jsonPayload = "{\"weight\": " + String(weight) + ", \"region\": \"" + region + "\", \"message\": \"Mode3\"}";
  int httpCode = http.POST(jsonPayload);

  if(httpCode > 0){
    String response = http.getString();
    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, response);

    if(!error) {
      int command = doc["spray"]; 
      String weather = doc["weather"];
      String regionName = doc["region"];

      Serial.print("🌤 날씨 확인: ["); Serial.print(regionName); Serial.print(" / "); Serial.print(weather); Serial.println("]");

      int targetPin = -1;
      if (command == 1) targetPin = PIN_SUNNY;
      else if (command == 2) targetPin = PIN_CLOUDY;
      else if (command == 3) targetPin = PIN_RAIN;
      else if (command == 4) targetPin = PIN_SNOW;

      if (targetPin != -1) startInterval(targetPin); // 반복 작동 시작
      else Serial.println("⚠️ 해당 날씨에 매칭된 동작이 없습니다.");
    } else {
       Serial.println("❌ JSON 파싱 에러");
    }
  } else {
    Serial.print("🚨 서버 통신 실패 (Error code: "); Serial.print(httpCode); Serial.println(")");
  }
  http.end();
}

// [공통] 인터벌 작동 시작 함수 (상태 변수 설정)
void startInterval(int pin) {
  // 기존에 켜져 있던 핀 강제 종료
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);

  // 새로운 핀으로 상태 업데이트
  activePin = pin;
  isRunning = true;
  isSpraying = true; // 켜면서 시작
  previousMillis = millis(); // 현재 시간 기록
  
  digitalWrite(activePin, LOW); // 즉시 켜기
  Serial.println("🔁 반복 작동 모드 시작 (멈추려면 '0' 입력)");
  Serial.println("   💦 분사 시작 (ON)!");
}
