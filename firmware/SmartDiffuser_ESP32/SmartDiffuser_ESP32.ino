/*
 * [프로젝트] 스마트 디퓨저 (Smart Diffuser)
 * [버  전] 8.3 Final
 * [작성자] 21학번 류재홍
 * [기  능] 모드별(수동/감성/날씨) 제어, 정밀 세팅, 웹 대시보드, 백스페이스 지원 입력 시스템
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h>
#include "DFRobotDFPlayerMini.h" 

// ============================================================
// [0] 설정 및 핀 정의
// ============================================================
#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN    "\033[36m"
#define C_BOLD    "\033[1m"

const char* ssid     = "Jaehong_WiFi";       
const char* password = "12345678";        
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 하드웨어 핀 맵핑
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

const int DFPLAYER_RX_PIN = 32; 
const int DFPLAYER_TX_PIN = 33; 

// 전역 객체 및 변수
HardwareSerial mySoftwareSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
HX711 scale;
Preferences prefs;
WiFiServer webServer(80); 

unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;   
const long MAX_RUN_TIME   = 270000; 

float calibration_factor = 430.0; 
int currentMode = 0;       
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;   
String lastWebMessage = "시스템 준비 완료 (Ready)";

// 데모 및 타이머 변수
int demoStep = 0;
unsigned long prevDemoMillis = 0;
unsigned long prevMotorMillis = 0; 
unsigned long lastCheckTime = 0; // WiFi 체크용
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; 
int ledBrightness = 0;
int ledFadeAmount = 5;

// 날씨 자동 호출 설정
unsigned long lastWeatherCallMillis = 0;
const unsigned long WEATHER_INTERVAL = 3600000; // 1시간
String lastWeatherRegion = "서울";              

// 함수 원형
void bootAnimation(); 
void handleInput(String input);
void stopSystem();
void printMainMenu();
void sendServerRequest(String payload);
void printCalibrationInfo();
void runManualMode(String input);
void forceAllOff();
void manageWiFi();
void systemHeartbeat();
void handleWebClient();
void runSprayLogic();
void checkSafety();
void runAutoDemoLoop();
void redrawInputLine(String &buffer);
void checkSerialInput();
void connectWiFi();
void printDashboard();
void playSound(int trackNum);
void autoWeatherScheduler();

// ============================================================
// [1] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5000); 
  
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT); 
  forceAllOff(); 

  Serial.print("\r\n\r\n");
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);
  Serial.printf(C_BOLD    "   🏆 SMART DIFFUSER V8.3 (FINAL) 🏆    \r\n" C_RESET);
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);

  // Audio 초기화
  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.print(C_YELLOW "[System] Audio Module Init..." C_RESET);
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(C_RED "FAILED! (Check Connection & SD Card)" C_RESET);
  } else {
    Serial.println(C_GREEN " DONE!" C_RESET);
    myDFPlayer.volume(20); 
  }

  // 설정값 로드 및 WiFi 연결
  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;

  connectWiFi();
  webServer.begin(); 
  
  // 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  bootAnimation();
  printMainMenu(); 
}

void bootAnimation() {
    for(int i=0; i<3; i++) {
        digitalWrite(PIN_LED, HIGH); delay(100);
        digitalWrite(PIN_LED, LOW);  delay(100);
    }
}

// ============================================================
// [2] 핵심 입력 시스템 (ANSI 제어)
// ============================================================

// 입력창 UI 갱신 (커서 이동 -> 줄 삭제 -> 프롬프트 재출력)
void redrawInputLine(String &buffer) {
  Serial.print("\r");       
  Serial.print("\033[K");   
  Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); 
  Serial.print(buffer);     
}

// 시리얼 입력 처리 (백스페이스 지원 및 즉시 반응 모드 분리)
void checkSerialInput() {
  
  // 모드 4(정밀 세팅): 엔터 없이 즉시 반응
  if (currentMode == 4) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') return; 

      if (c == '+') { calibration_factor += 10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
      else if (c == '-') { calibration_factor -= 10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
      else if (c == 't') { scale.tare(); Serial.printf(C_GREEN "\r\n⚖️ 영점 조절 완료 (Tare)\r\n" C_RESET); printCalibrationInfo(); }
      else if (c == 'w') { printCalibrationInfo(); }
      else if (c == 's') { prefs.putFloat("cal_factor", calibration_factor); Serial.printf(C_BLUE "\r\n💾 [Save] 설정값 저장 완료!\r\n" C_RESET); }
      else if (c == '0') { currentMode = 0; printMainMenu(); }
    }
    return;
  }

  // 일반 모드: 버퍼링 및 백스페이스 처리
  static String inputBuffer = ""; 

  while (Serial.available() > 0) {
    char c = Serial.read(); 

    if (c == '\n' || c == '\r') { // 엔터 입력 시 실행
      if (inputBuffer.length() > 0) {
        Serial.println(); 
        if (inputBuffer == "0") { stopSystem(); currentMode = 0; printMainMenu(); } 
        else { handleInput(inputBuffer); }
        inputBuffer = ""; 
      }
    }
    else if (c == '\b' || c == 0x7F) { // 백스페이스 처리
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1); 
        redrawInputLine(inputBuffer); 
      }
    }
    else { // 일반 문자
      inputBuffer += c;
      redrawInputLine(inputBuffer); 
    }
  }
}

// ============================================================
// [3] 메인 루프
// ============================================================
void loop() {
  manageWiFi();      
  systemHeartbeat(); 
  handleWebClient(); 
  autoWeatherScheduler();
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) {
    runSprayLogic();   
    checkSafety();     
  }
  checkSerialInput(); 
}

// ============================================================
// [4] 주요 기능 구현
// ============================================================

// WiFi 상태 감지 및 자동 재연결
void manageWiFi() {
  static bool wasConnected = true; 

  if (millis() - lastCheckTime >= 1000) {
    lastCheckTime = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) { 
        wasConnected = false;
        Serial.print("\r\n");
        Serial.printf(C_RED "🚨 [System] WiFi 연결 끊김! 재연결 시도 중...\r\n" C_RESET);
        WiFi.disconnect(); WiFi.reconnect();
        Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); 
      }
    } else {
      if (!wasConnected) { 
        wasConnected = true;
        Serial.print("\r\n");
        Serial.printf(C_GREEN "✅ [System] WiFi 재연결 성공! (신호: %d dBm)\r\n" C_RESET, WiFi.RSSI());
        Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); 
      }
    }
  }
}

void autoWeatherScheduler() {
  if (currentMode != 3 || isRunning) return;

  unsigned long now = millis();
  if (now - lastWeatherCallMillis >= WEATHER_INTERVAL) {
    lastWeatherCallMillis = now;
    float w = scale.get_units(10); 
    Serial.printf(C_YELLOW "\r\n[AUTO WEATHER] 1시간 주기 자동 호출 (%s)\r\n" C_RESET, lastWeatherRegion.c_str());
    
    String json = "{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}";
    sendServerRequest(json);
  }
}

void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") { 
        currentMode = 1; 
        Serial.printf(C_BLUE "\r\n--- [ Mode 1: 수동 제어 ] ---\r\n" C_RESET); 
        Serial.println(C_YELLOW "👉 실행할 번호를 입력하세요: [1]맑음 [2]흐림 [3]비 [4]눈" C_RESET); 
    }
    else if (input == "2") { 
        currentMode = 2; 
        Serial.printf(C_BLUE "\r\n--- [ Mode 2: 감성 모드 ] ---\r\n" C_RESET); 
        Serial.println(C_YELLOW "👉 현재 기분을 입력하세요 (예: 행복해, 우울해, 신남, 피곤)" C_RESET);
    }
    else if (input == "3") { 
        currentMode = 3; 
        Serial.printf(C_BLUE "\r\n--- [ Mode 3: 날씨 모드 ] ---\r\n" C_RESET);
        Serial.println(C_YELLOW "👉 검색할 지역명(예: 서울, 제주, 부산)을 입력하세요." C_RESET);
    }
    else if (input == "4") { 
        currentMode = 4; 
        Serial.printf(C_YELLOW "\r\n--- [ 🛠️ 정밀 세팅 ] ---\r\n" C_RESET); 
        Serial.println("👉 +/-:조절, w:무게확인, t:영점, s:저장, 0:종료"); 
    }
    else if (input == "5") { 
        currentMode = 5; demoStep=0; 
        Serial.printf(C_MAGENTA "\r\n--- [ ✨ 오토 데모 ] ---\r\n" C_RESET); 
    }
    else if (input == "9") { printDashboard(); } 
    else { Serial.printf(C_RED "❌ 잘못된 입력\r\n" C_RESET); printMainMenu(); }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) { 
      Serial.printf(C_YELLOW "[Emotion] 분석 요청...\r\n" C_RESET); 
      sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}"); 
  }
  else if (currentMode == 3) { 
      lastWeatherRegion = input;
      float w = scale.get_units(10); 
      Serial.printf(C_YELLOW "[Weather] 날씨 조회 (%.1fg - CH4)\r\n" C_RESET, w);
      String json = "{\"mode\": \"weather\", \"region\": \"" + input + "\", \"w4\": " + String(w) + "}";
      sendServerRequest(json);
  }
}

// 웹 서버 요청 처리 (AJAX 및 HTML 렌더링)
void handleWebClient() {
  WiFiClient client = webServer.available();
  if (!client) return;

  unsigned long startTime = millis();
  while (!client.available() && millis() - startTime < 1000) { delay(1); }

  String currentLine = "";
  String request = "";
  
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          
          // API 요청 처리 (버튼 클릭 등)
          if (request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0) {
              if (request.indexOf("GET /RUN_SUNNY") >= 0) { runManualMode("1"); lastWebMessage = "수동 명령: 맑음 실행"; }
              if (request.indexOf("GET /RUN_CLOUDY") >= 0) { runManualMode("2"); lastWebMessage = "수동 명령: 흐림 실행"; }
              if (request.indexOf("GET /RUN_RAIN") >= 0) { runManualMode("3"); lastWebMessage = "수동 명령: 비 실행"; }
              if (request.indexOf("GET /RUN_SNOW") >= 0) { runManualMode("4"); lastWebMessage = "수동 명령: 눈 실행"; }
              if (request.indexOf("GET /STOP") >= 0) { stopSystem(); currentMode=0; printMainMenu(); lastWebMessage = "⛔ 시스템 강제 정지"; }

              client.println("HTTP/1.1 204 No Content\r\nConnection: close\r\n");
          }
          // HTML 페이지 렌더링
          else {
              client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");
              
              // HTML/CSS 내용 (스타일 생략 없이 기능 유지를 위해 최소화하여 포함)
              client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>");
              client.println("<style>body{font-family:sans-serif;text-align:center;background:#1a1a1a;color:white;padding:15px;}.btn{display:block;width:100%;max-width:400px;margin:12px auto;padding:20px;font-size:19px;border-radius:12px;border:none;color:white;font-weight:bold;cursor:pointer;}.status-box{background:#333;color:#00ff00;padding:15px;margin:10px auto;border-radius:10px;border:1px solid #555;max-width:400px;}</style>");
              client.println("<style>.blue{background:#2980b9}.purple{background:#8e44ad}.orange{background:#d35400}.grey{background:#7f8c8d}.teal{background:#16a085}.red{background:#c0392b}.sunny{background:#f2c94c;color:#333}.cloudy{background:#95a5a6}.rain{background:#3498db}.snow{background:#ecf0f1;color:#333}.back{background:#333;border:1px solid #555;margin-bottom:25px}</style>");
              client.println("<script>function send(url){fetch(url);setTimeout(function(){location.reload();},500);}</script></head><body>");

              client.print("<div class='status-box'>📢 상태: "); client.print(lastWebMessage); client.println("</div>");

              if (request.indexOf("GET /PAGE_MANUAL") >= 0) {
                  if(currentMode != 1) { currentMode = 1; Serial.println(C_BLUE "\r\n[Web Sync] 수동 제어 모드" C_RESET); }
                  client.println("<h1>🎮 수동 제어</h1><a href='/'><button class='btn back'>🏠 메인 메뉴</button></a>");
                  client.println("<button class='btn sunny' onclick=\"send('/RUN_SUNNY')\">☀️ 맑음</button><button class='btn cloudy' onclick=\"send('/RUN_CLOUDY')\">☁️ 흐림</button>");
                  client.println("<button class='btn rain' onclick=\"send('/RUN_RAIN')\">☔ 비</button><button class='btn snow' onclick=\"send('/RUN_SNOW')\">❄️ 눈</button><br><button class='btn red' onclick=\"send('/STOP')\">⛔ 정지</button>");
              }
              else if (request.indexOf("GET /PAGE_DASHBOARD") >= 0) {
                  client.println("<h1>📊 대시보드</h1><a href='/'><button class='btn back'>🏠 메인 메뉴</button></a>");
                  client.printf("<div style='text-align:left;background:#333;padding:20px;border-radius:10px;'><p>📡 WiFi: <b>%d dBm</b></p>", WiFi.RSSI());
                  client.printf("<p>⚖️ 무게(CH4): <b>%.2f g</b></p></div>", scale.get_units(5));
                  client.println("<br><button class='btn grey' onclick='location.reload()'>🔄 새로고침</button>");
              }
              else {
                  if (currentMode != 0) { currentMode = 0; Serial.println(C_CYAN "\r\n[Web Sync] 🏠 메인 복귀" C_RESET); printMainMenu(); }
                  client.printf("<h1>Smart Diffuser V8.3</h1><p style='color:#888;'>IP: %s</p>", WiFi.localIP().toString().c_str());
                  client.println("<a href='/PAGE_MANUAL'><button class='btn blue'>[1] 🎮 수동 제어</button></a><button class='btn purple' onclick=\"alert('터미널 이용');\">[2] 💜 감성 모드</button>");
                  client.println("<button class='btn orange' onclick=\"alert('터미널 이용');\">[3] 🌦️ 날씨 모드</button><a href='/PAGE_DASHBOARD'><button class='btn teal'>[9] 📊 대시보드</button></a>");
              }
              client.println("</body></html>");
          }
          break;
        } else { currentLine = ""; }
      } else if (c != '\r') { currentLine += c; }
    }
  }
  delay(20); client.stop();
}

void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) { 
      Serial.printf(C_RED "🚨 WiFi 연결 안됨!\r\n" C_RESET); 
      lastWebMessage = "🚨 에러: WiFi 연결 끊김";
      return; 
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
    
    Serial.printf(C_GREEN "✅ 서버 응답: %s\r\n" C_RESET, txt.c_str());
    
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY; else if (cmd == 2) target = PIN_CLOUDY; 
    else if (cmd == 3) target = PIN_RAIN; else if (cmd == 4) target = PIN_SNOW;
    
    if (target != -1) { 
        forceAllOff(); activePin = target; isRunning = true; isSpraying = true; 
        sprayDuration = dur * 1000; prevMotorMillis = millis(); startTimeMillis = millis(); 
        digitalWrite(activePin, LOW); playSound(cmd); 
        Serial.printf(C_GREEN "[Loop] 💦 분사 시작! (%d초)\r\n" C_RESET, dur); 
        lastWebMessage = "✅ 성공: " + txt + " (" + String(dur) + "초)";
    } else { 
        Serial.printf(C_YELLOW "⚠️ 대기 상태 (%s)\r\n" C_RESET, txt.c_str());
        lastWebMessage = "⚠️ 대기: " + txt;
        stopSystem(); 
    }
  } else { 
      Serial.printf(C_RED "🚨 통신 에러: %d\r\n" C_RESET, code); 
      lastWebMessage = "🚨 통신 에러 (" + String(code) + ")";
  }
  http.end();
}

// ============================================================
// [5] 유틸리티 및 제어 함수
// ============================================================

void runSprayLogic() {
  if (activePin == -1) return;
  unsigned long currentMillis = millis();
  
  if (isSpraying) {
    if (currentMillis - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH); isSpraying = false; prevMotorMillis = currentMillis;
      if (currentMode == 1) { Serial.printf(C_CYAN "      └── [Manual] 동작 완료.\r\n" C_RESET); stopSystem(); return; }
      Serial.printf(C_CYAN "      └── [Idle] ⏳ 휴식 중...\r\n" C_RESET);
    }
  } else {
    if (currentMillis - prevMotorMillis >= REST_TIME) {
      forceAllOff(); digitalWrite(activePin, LOW); isSpraying = true; prevMotorMillis = currentMillis;
      Serial.printf(C_GREEN "      ┌── [Action] 💨 재분사 시작!\r\n" C_RESET);
    }
  }
}

void runAutoDemoLoop() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevDemoMillis >= 4000) {
    prevDemoMillis = currentMillis;
    forceAllOff(); 
    demoStep++; if (demoStep > 4) demoStep = 1; 

    int target = -1; String name = "";
    if (demoStep == 1) { target = PIN_SUNNY; name = "☀️ 맑음"; playSound(1); }
    else if (demoStep == 2) { target = PIN_CLOUDY; name = "☁️ 흐림"; playSound(2); }
    else if (demoStep == 3) { target = PIN_RAIN; name = "☔ 비"; playSound(3); }
    else if (demoStep == 4) { target = PIN_SNOW; name = "❄️ 눈"; playSound(4); }

    Serial.printf(C_MAGENTA "[Auto Demo] %s 모드\r\n" C_RESET, name.c_str());
    lastWebMessage = "데모 모드: " + name; 
    digitalWrite(target, LOW); 
  }
}

void checkSafety() {
  if (millis() - startTimeMillis > MAX_RUN_TIME) {
    Serial.printf(C_RED "\r\n🚨 [Emergency] 안전 타이머 작동!\r\n" C_RESET);
    stopSystem(); currentMode = 0; printMainMenu();
  }
}

void printCalibrationInfo() {
    Serial.printf("📡 보정값: %.1f | 현재 무게: %.2f g\r\n", calibration_factor, scale.get_units(5));
}

void connectWiFi() {
  Serial.printf(C_YELLOW "[System] WiFi Connecting" C_RESET);
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 15) { delay(200); Serial.print("."); retry++; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\r\n" C_GREEN "[System] Connected! (%d dBm)\r\n" C_RESET, WiFi.RSSI());
    Serial.printf(C_CYAN "🌐 Web: http://%s/\r\n" C_RESET, WiFi.localIP().toString().c_str());
  } else { Serial.printf(C_RED "\r\n[System] WiFi Failed.\r\n" C_RESET); }
}

void systemHeartbeat() {
  if (isRunning) {
    if (millis() - prevLedMillis >= 200) { prevLedMillis = millis(); digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }
  } else {
    if (millis() - prevLedMillis >= 30) {
      prevLedMillis = millis();
      ledBrightness += ledFadeAmount;
      if (ledBrightness <= 0 || ledBrightness >= 255) ledFadeAmount = -ledFadeAmount;
      analogWrite(PIN_LED, ledBrightness); 
    }
  }
}

void printDashboard() {
    Serial.printf(C_CYAN "\r\n📊 [ SYSTEM DASHBOARD ] 📊\r\n" C_RESET);
    Serial.printf(" ├─ WiFi RSSI   : %d dBm\r\n", WiFi.RSSI());
    Serial.printf(" ├─ Web Server  : http://%s\r\n", WiFi.localIP().toString().c_str());
    Serial.printf(" ├─ Cal.Factor  : %.1f\r\n", calibration_factor);
    Serial.printf(" └─ Weight(CH4) : %.2f g\r\n", scale.get_units(10));
    Serial.printf("----------------------------\r\n");
    printMainMenu();
}

void playSound(int trackNum) {
  myDFPlayer.play(trackNum);
}

void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; myDFPlayer.stop(); Serial.printf(C_RED "\r\n⛔ 정지.\r\n" C_RESET); }

void runManualMode(String input) {
  int pin = -1, track = 0;
  if (input == "1") { pin = PIN_SUNNY; track = 1; }
  else if (input == "2") { pin = PIN_CLOUDY; track = 2; }
  else if (input == "3") { pin = PIN_RAIN; track = 3; }
  else if (input == "4") { pin = PIN_SNOW; track = 4; }
  else { Serial.printf(C_RED "⚠️ 1~4 입력\r\n" C_RESET); return; }
  
  forceAllOff(); activePin = pin; isRunning = true; isSpraying = true; 
  sprayDuration = 3000; prevMotorMillis = millis(); startTimeMillis = millis(); 
  digitalWrite(activePin, LOW); playSound(track); 
  Serial.printf(C_GREEN "[Loop] 분사 시작 (BGM %d)\r\n" C_RESET, track);
}

void printMainMenu() {
  Serial.printf(C_CYAN "\r\n=== 🕹️ MAIN MENU (V8.3 Final) 🕹️ ===\r\n" C_RESET);
  Serial.printf(" [1] 수동   [2] 감성   [3] 날씨\r\n [4] 🛠️ 설정   [5] ✨ 데모   [9] 📊 대시보드\r\n" C_YELLOW "👉 명령 입력 >>" C_RESET);
}
