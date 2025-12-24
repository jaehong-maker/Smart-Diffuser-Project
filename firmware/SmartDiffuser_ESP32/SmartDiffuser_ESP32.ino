#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ================= [설정 정보] =================
const char* ssid     = "Jaehong_WiFi";    
const char* password = "12345678";        

String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// ================= [시간 설정] =================
const long SPRAY_TIME = 5000; // 5초 작동
const long REST_TIME  = 5000; // 5초 휴식

// ================= [핀 번호 설정] =================
const int PIN_SUNNY  = 26; // 맑음 / 기쁨
const int PIN_CLOUDY = 27; // 흐림 / 평온
const int PIN_RAIN   = 14; // 비   / 슬픔
const int PIN_SNOW   = 13; // 눈   / 화남

const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN = 4;   

HX711 scale;
float calibration_factor = 430.0; 

// ================= [전역 변수 (상태 관리용)] =================
int currentMode = 0;      // 0:메뉴, 1:수동, 2:감성, 3:날씨
bool isRunning = false;   // 현재 작동 중인지 여부
int activePin = -1;       // 현재 작동 중인 핀 번호
bool isSpraying = false;  // 현재 분사 중(ON)인지 휴식 중(OFF)인지
unsigned long previousMillis = 0; // 마지막으로 상태가 바뀐 시간 저장

void setup() {
  Serial.begin(115200);

  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);

  allStop(); // 초기화

  WiFi.begin(ssid, password);
  Serial.print("WiFi 연결 중");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n연결 성공!");
  
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  printMainMenu(); 
}

void loop() {
  // 1. 와이파이 체크 (끊기면 재접속)
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); return; 
  }

  // 2. [핵심] 반복 작동 로직 (millis 사용 - 멈추지 않음)
  if (isRunning && activePin != -1) {
    unsigned long currentMillis = millis();
    
    // 분사 중일 때 -> 5초 지났는지 체크
    if (isSpraying) {
      if (currentMillis - previousMillis >= SPRAY_TIME) {
        // 시간이 됐으면 끈다 (휴식 시작)
        digitalWrite(activePin, HIGH); 
        isSpraying = false;
        previousMillis = currentMillis;
        Serial.println("   완료 -> 💤 휴식 중...");
      }
    } 
    // 휴식 중일 때 -> 5초 지났는지 체크
    else {
      if (currentMillis - previousMillis >= REST_TIME) {
        // 시간이 됐으면 켠다 (분사 시작)
        digitalWrite(activePin, LOW);
        isSpraying = true;
        previousMillis = currentMillis;
        Serial.println("   💦 분사 시작 (ON)!");
      }
    }
  }

  // 3. 사용자 입력 처리
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // 버퍼 비우기 (delay 없이 빠르게 처리)
    while(Serial.available()) Serial.read(); 

    if (input.length() > 0) {
      // [공통] 0 입력 시 즉시 중단 및 메인 메뉴 복귀
      if (input == "0") {
        allStop(); // 모든 작동 멈춤 (변수 초기화)
        currentMode = 0;
        printMainMenu();
        return;
      }

      // 모드 선택 및 실행 로직
      handleInput(input);
    }
  }
}

// ======================================================
// [함수 모음]
// ======================================================

void handleInput(String input) {
  // 메인 메뉴 상태일 때
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println("\n[1. 수동 모드] 핀 번호(1~4)를 입력하세요. (종료: 0)");
    }
    else if (input == "2") {
      currentMode = 2;
      Serial.println("\n[2. 감성 모드] 기분을 입력하세요. (종료: 0)");
    }
    else if (input == "3") {
      currentMode = 3;
      Serial.println("\n[3. 날씨 모드] 지역을 입력하세요. (종료: 0)");
    }
    else {
      Serial.println("❌ 1, 2, 3 중 선택하세요.");
      printMainMenu();
    }
  }
  // 각 모드별 상세 입력 처리
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) setupEmotionMode(input);
  else if (currentMode == 3) setupWeatherMode(input);
}

void printMainMenu() {
  Serial.println("\n=== 🌟 스마트 디퓨저 메뉴 ===");
  Serial.println("[1] 수동 모드");
  Serial.println("[2] 감성 모드 (반복)");
  Serial.println("[3] 날씨 모드 (반복)");
  Serial.println("👉 선택(1~3):");
}

// 모든 상태 초기화 (멈춤)
void allStop() {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
  
  isRunning = false;
  activePin = -1;
  isSpraying = false;
  Serial.println("⛔ 작동 중지. 대기 상태.");
}

// [모드 1] 수동 제어 (단발성)
void runManualMode(String input) {
  // 수동 모드는 반복 아니므로 기존 작동 멈춤
  isRunning = false; 
  int pin = -1;
  
  if (input == "1") pin = PIN_SUNNY;
  else if (input == "2") pin = PIN_CLOUDY;
  else if (input == "3") pin = PIN_RAIN;
  else if (input == "4") pin = PIN_SNOW;
  else { Serial.println("⚠️ 1~4 입력."); return; }

  Serial.print(">>> 수동 5초 작동: "); Serial.println(input);
  digitalWrite(pin, LOW);
  delay(5000); // 수동은 딱 한번이니 delay 써도 무방 (간단 구현)
  digitalWrite(pin, HIGH);
  Serial.println(">>> 완료.");
}

// [모드 2] 감성 모드 설정 (작동 시작 X, 설정만 함)
void setupEmotionMode(String emotion) {
  int targetPin = -1;
  
  if (emotion == "기쁨" || emotion == "행복") targetPin = PIN_SUNNY;
  else if (emotion == "평온" || emotion == "휴식") targetPin = PIN_CLOUDY;
  else if (emotion == "슬픔" || emotion == "우울") targetPin = PIN_RAIN;
  else if (emotion == "화남" || emotion == "스트레스") targetPin = PIN_SNOW;
  else { Serial.println("⚠️ 알 수 없는 감정"); return; }

  Serial.print("💖 감정 설정: "); Serial.println(emotion);
  startInterval(targetPin); // 반복 작동 시작
}

// [모드 3] 날씨 모드 설정
void setupWeatherMode(String region) {
  HTTPClient http;
  http.setTimeout(3000);
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  float weight = 0.0;
  if (scale.is_ready()) weight = scale.get_units(5);
  
  String jsonPayload = "{\"weight\": " + String(weight) + ", \"region\": \"" + region + "\", \"message\": \"Mode3\"}";
  int httpCode = http.POST(jsonPayload);

  if(httpCode > 0){
    String response = http.getString();
    JsonDocument doc; 
    deserializeJson(doc, response);

    int command = doc["spray"]; 
    String weather = doc["weather"];
    String regionName = doc["region"];

    Serial.print("🌤 날씨: "); Serial.print(regionName); Serial.print("/"); Serial.println(weather);

    int targetPin = -1;
    if (command == 1) targetPin = PIN_SUNNY;
    else if (command == 2) targetPin = PIN_CLOUDY;
    else if (command == 3) targetPin = PIN_RAIN;
    else if (command == 4) targetPin = PIN_SNOW;

    if (targetPin != -1) startInterval(targetPin);
    else Serial.println("⚠️ 작동 조건 아님");
  } else {
    Serial.println("🚨 통신 실패");
  }
  http.end();
}

// [공통] 반복 작동 시작 함수
void startInterval(int pin) {
  // 기존에 돌던게 있으면 끄고 새로 시작
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);

  activePin = pin;
  isRunning = true;
  isSpraying = true; // 켜면서 시작
  previousMillis = millis(); // 현재 시간 저장
  
  digitalWrite(activePin, LOW); // 즉시 켜기
  Serial.println("🔁 반복 작동 시작 (멈추려면 0 입력)");
  Serial.println("   💦 분사 시작 (ON)!");
}
