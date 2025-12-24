#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"

// ================= [설정 정보] =================
const char* ssid     = "Jaehong_WiFi";    
const char* password = "12345678";        

// AWS 함수 URL
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// ================= [핀 번호 설정] =================
const int PIN_SUNNY  = 26; // 맑음
const int PIN_CLOUDY = 27; // 흐림
const int PIN_RAIN   = 14; // 비
const int PIN_SNOW   = 13; // 눈

const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN = 4;   

HX711 scale;
float calibration_factor = 430.0; 

void setup() {
  Serial.begin(115200);

  // 1. 핀 모드 설정
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);

  // 2. 초기 상태: 모두 끄기 (Active LOW: HIGH가 꺼짐)
  digitalWrite(PIN_SUNNY, HIGH); 
  digitalWrite(PIN_CLOUDY, HIGH); 
  digitalWrite(PIN_RAIN, HIGH); 
  digitalWrite(PIN_SNOW, HIGH); 

  // 3. 와이파이 연결
  WiFi.begin(ssid, password);
  Serial.print("WiFi 연결 중");
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 10) {
    delay(500); Serial.print("."); retry++;
  }
  if(WiFi.status() == WL_CONNECTED) Serial.println("\n연결 성공!");
  
  // 4. 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  // 5. 안내 메시지
  Serial.println("\n=============================================");
  Serial.println("📢 [준비 완료] 원하시는 지역 이름을 입력해주세요.");
  Serial.println("👉 예시: 서울, 부산, 대구... (또는 테스트비, 테스트눈)");
  Serial.println("=============================================\n");
}

void loop() {
  // 1. 와이파이 끊김 체크 및 재접속
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); WiFi.reconnect(); delay(1000); return; 
  }

  // 2. 사용자의 입력을 기다림
  if (Serial.available() > 0) {
    
    // 입력된 글자 읽기
    String inputRegion = Serial.readStringUntil('\n');
    inputRegion.trim(); // 공백 제거

    // [중복 입력 방지] 0.1초 대기 후 찌꺼기 제거
    delay(100); 
    while(Serial.available() > 0) {
      Serial.read(); 
    }

    if (inputRegion.length() > 0) {
      Serial.print("✅ 입력 확인: [");
      Serial.print(inputRegion);
      Serial.println("] 날씨를 조회합니다...");

      // 3. 서버 요청 시작
      checkWeather(inputRegion);
      
      Serial.println("\n---------------------------------------------");
      Serial.println("📢 다음 지역을 입력해주세요:");
      Serial.println("---------------------------------------------\n");
    }
  }
}

// 날씨 조회 및 모터 제어 함수
void checkWeather(String region) {
  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  float weight = 0.0;
  if (scale.is_ready()) weight = scale.get_units(5);
  
  // 입력받은 지역(region)을 JSON에 넣어서 보냄
  String jsonPayload = "{\"weight\": " + String(weight) + ", \"region\": \"" + region + "\", \"message\": \"User_Input\"}";
  int httpResponseCode = http.POST(jsonPayload);

  if(httpResponseCode > 0){
    String response = http.getString();
    Serial.println("\n========== [서버 응답 원본 데이터] ==========");
    Serial.println(response); 
    Serial.println("===========================================");

    // ==========================================================
    // [★수정 완료] doc을 먼저 만들고 -> 그 다음에 사용합니다.
    // ==========================================================
    JsonDocument doc; // 1. 그릇(변수) 먼저 만들기
    
    DeserializationError error = deserializeJson(doc, response); // 2. 담기

    if (error) {
      Serial.print("❌ JSON 파싱 실패(형식 오류): ");
      Serial.println(error.c_str());
      http.end();
      return;
    }

    int command = doc["spray"]; 
    String weatherText = doc["weather"];
    String regionName = doc["region"];

    Serial.print(">>> 결과: ["); Serial.print(regionName); Serial.print(" / "); Serial.print(weatherText); Serial.println("]");

    // 핀 제어 (상태 유지 모드)
    if (command == 1) { // 맑음
      Serial.println("    └─ 26번(맑음) ON, 나머지 OFF");
      digitalWrite(PIN_SUNNY, LOW);   
      digitalWrite(PIN_CLOUDY, HIGH); 
      digitalWrite(PIN_RAIN, HIGH);
      digitalWrite(PIN_SNOW, HIGH);
    }
    else if (command == 2) { // 흐림
      Serial.println("    └─ 27번(흐림) ON, 나머지 OFF");
      digitalWrite(PIN_SUNNY, HIGH);
      digitalWrite(PIN_CLOUDY, LOW);  
      digitalWrite(PIN_RAIN, HIGH);
      digitalWrite(PIN_SNOW, HIGH);
    }
    else if (command == 3) { // 비
      Serial.println("    └─ 14번(비) ON, 나머지 OFF");
      digitalWrite(PIN_SUNNY, HIGH);
      digitalWrite(PIN_CLOUDY, HIGH);
      digitalWrite(PIN_RAIN, LOW);    
      digitalWrite(PIN_SNOW, HIGH);
    }
    else if (command == 4) { // 눈
      Serial.println("    └─ 13번(눈) ON, 나머지 OFF");
      digitalWrite(PIN_SUNNY, HIGH);
      digitalWrite(PIN_CLOUDY, HIGH);
      digitalWrite(PIN_RAIN, HIGH);
      digitalWrite(PIN_SNOW, LOW);    
    }
    else {
      Serial.println("⚠️ 지원하지 않는 지역이거나 에러입니다. (모두 끔)");
      digitalWrite(PIN_SUNNY, HIGH);
      digitalWrite(PIN_CLOUDY, HIGH);
      digitalWrite(PIN_RAIN, HIGH);
      digitalWrite(PIN_SNOW, HIGH);
    }
  } else {
    Serial.print("통신 실패 에러코드: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}
