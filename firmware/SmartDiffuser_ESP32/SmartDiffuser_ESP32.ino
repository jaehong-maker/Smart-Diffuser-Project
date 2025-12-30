/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 8.0 (Audio Edition: 4D Experience)
 * [작성자] 21학번 류재홍
 * [추가기능] 
 * - DFPlayer Mini 연동: 모드별 BGM 자동 재생
 * - 파일명: 0001(맑음), 0002(흐림), 0003(비), 0004(눈), 0005(정지)
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h>
#include "DFRobotDFPlayerMini.h" // [New] 오디오 라이브러리

// ============================================================
// [0] 설정 및 핀 정의
// ============================================================
#define C_RESET  "\033[0m"
#define C_RED    "\033[31m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_BLUE   "\033[34m"
#define C_MAGENTA "\033[35m"
#define C_CYAN   "\033[36m"
#define C_BOLD   "\033[1m"

const char* ssid     = "Jaehong_WiFi";      
const char* password = "12345678";        
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// ✅ ADD: 디바이스 ID + 날씨 자동호출 주기(1시간)
#define DEVICE_ID "ESP32-001"
#define WEATHER_INTERVAL 3600000UL  // 1시간

// 릴레이 & LED
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

// 로드셀
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

// [New] 오디오 (Serial2 사용)
const int DFPLAYER_RX_PIN = 32; // ESP32의 RX (모듈의 TX와 연결) -> 아니 반대임. ESP TX->모듈 RX
const int DFPLAYER_TX_PIN = 33; // ESP32의 TX (모듈의 RX와 연결)
HardwareSerial mySoftwareSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;   
const long MAX_RUN_TIME   = 270000; 

// ============================================================
// [2] 전역 변수
// ============================================================
HX711 scale;
Preferences prefs;
WiFiServer webServer(80); 

float calibration_factor = 430.0; 
bool isSimulation = false; // 로드셀 테스트할거면 false로 변경!

int currentMode = 0;       
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;   

int demoStep = 0;
unsigned long prevDemoMillis = 0;
unsigned long prevMotorMillis = 0; 
unsigned long prevWifiMillis = 0;
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; 
int ledBrightness = 0;
int ledFadeAmount = 5;

// ✅ ADD: 날씨 모드 자동 호출 타이머 (1시간)
unsigned long lastWeatherMillis = 0;

// ============================================================
// [3] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  
  // 핀 설정
  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT); 
  forceAllOff(); 

  Serial.print("\r\n\r\n");
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);
  Serial.printf(C_BOLD    "   🏆 SMART DIFFUSER V8.0 (AUDIO) 🏆    \r\n" C_RESET);
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);

  // [New] 오디오 초기화
  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.print(C_YELLOW "[System] Audio Module Init..." C_RESET);
  
  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println(C_RED "FAILED! (Check Connection & SD Card)" C_RESET);
  } else {
    Serial.println(C_GREEN " DONE!" C_RESET);
    myDFPlayer.volume(20); // 볼륨 0~30
  }

  // 저장값 로드
  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;

  connectWiFi();
  webServer.begin(); 
  
  // 로드셀 초기화
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  printMainMenu(); 
}

// ============================================================
// [4] 메인 루프 (Loop)
// ============================================================
void loop() {
  manageWiFi();      
  systemHeartbeat(); 
  handleWebClient(); 

  // ✅ ADD: 날씨 모드(currentMode==3)일 때 1시간마다 자동 Lambda 호출
  // - 기존 "지역 입력 시 호출" 로직은 그대로 두고,
  // - 추가로, 모드 유지 중이면 1시간마다 자동 호출되게 함.
  if (currentMode == 3) {
    if (millis() - lastWeatherMillis >= WEATHER_INTERVAL) {
      lastWeatherMillis = millis();

      float w = isSimulation ? 500.0 : scale.get_units(10);
      Serial.printf(C_YELLOW "[Weather] ⏰ 1시간 주기 자동 Lambda 호출 (%.1fg)\r\n" C_RESET, w);

      // 기본 지역은 서울로 자동 호출 (원하면 여기만 바꾸면 됨)
      sendServerRequest(
        String("{\"device\":\"") + DEVICE_ID + 
        "\", \"mode\": \"weather\", \"region\": \"서울\", \"weight\": " + String(w) + "}"
      );
    }
  }
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) {
    runSprayLogic();   
    checkSafety();     
  }
  checkSerialInput(); 
}

// ============================================================
// [5] 핵심 기능 함수들
// ============================================================

// [New] 음악 재생 헬퍼 함수
void playSound(int trackNum) {
  Serial.printf("🎵 BGM 재생: %04d.mp3\r\n", trackNum);
  myDFPlayer.play(trackNum);
}

void handleWebClient() {
  WiFiClient client = webServer.available();
  if (client) {
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
            
            // [1] 명령 처리 (AJAX)
            if (request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0 || request.indexOf("GET /TOGGLE_SIM") >= 0) {
                
                // 음악과 함께 실행
                if (request.indexOf("GET /RUN_SUNNY") >= 0) { Serial.println("[Web] ☀️ 맑음 실행"); runManualMode("1"); }
                if (request.indexOf("GET /RUN_CLOUDY") >= 0) { Serial.println("[Web] ☁️ 흐림 실행"); runManualMode("2"); }
                if (request.indexOf("GET /RUN_RAIN") >= 0) { Serial.println("[Web] ☔ 비 실행"); runManualMode("3"); }
                if (request.indexOf("GET /RUN_SNOW") >= 0) { Serial.println("[Web] ❄️ 눈 실행"); runManualMode("4"); }
                
                if (request.indexOf("GET /STOP") >= 0) { stopSystem(); currentMode=0; printMainMenu(); }
                if (request.indexOf("GET /TOGGLE_SIM") >= 0) { isSimulation = !isSimulation; Serial.printf("[Web] 시뮬레이션: %s\r\n", isSimulation?"ON":"OFF"); }

                client.println("HTTP/1.1 204 No Content");
                client.println("Connection: close");
                client.println();
            }
            // [2] 페이지 렌더링 (V7.7과 동일, 생략 없이 전체 포함)
            else {
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println();

                client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no'>");
                client.println("<style>");
                client.println("body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: white; padding: 15px; -webkit-tap-highlight-color: transparent; }");
                client.println(".btn { display: block; width: 100%; max-width: 400px; margin: 12px auto; padding: 20px; font-size: 19px; border: none; border-radius: 12px; color: white; font-weight: bold; text-decoration: none; cursor: pointer; touch-action: manipulation; }");
                client.println(".btn:active { opacity: 0.6; transform: scale(0.98); }");
                client.println(".blue { background: #2980b9; } .purple { background: #8e44ad; } .orange { background: #d35400; } .grey { background: #7f8c8d; } .gold { background: #f39c12; color: #333; } .teal { background: #16a085; } .red { background: #c0392b; }");    
                client.println(".sunny { background: #f2c94c; color: #333; } .cloudy { background: #95a5a6; } .rain { background: #3498db; } .snow { background: #ecf0f1; color: #333; }");
                client.println(".back { background: #333; border: 1px solid #555; margin-bottom: 25px; }");
                client.println("</style>");
                client.println("<script>function send(url) { fetch(url); }</script>");
                client.println("</head><body>");

                if (request.indexOf("GET /PAGE_MANUAL") >= 0) {
                    if(currentMode != 1) { currentMode = 1; Serial.println(C_BLUE "\r\n[Web Sync] 수동 제어 모드" C_RESET); }
                    client.println("<h1>🎮 수동 제어</h1>");
                    client.println("<a href='/'><button class='btn back'>🏠 메인 메뉴로</button></a>");
                    client.println("<button class='btn sunny' onclick=\"send('/RUN_SUNNY')\">☀️ 맑음 (SUNNY)</button>");
                    client.println("<button class='btn cloudy' onclick=\"send('/RUN_CLOUDY')\">☁️ 흐림 (CLOUDY)</button>");
                    client.println("<button class='btn rain' onclick=\"send('/RUN_RAIN')\">☔ 비 (RAIN)</button>");
                    client.println("<button class='btn snow' onclick=\"send('/RUN_SNOW')\">❄️ 눈 (SNOW)</button>");
                    client.println("<br><button class='btn red' onclick=\"send('/STOP')\">⛔ 긴급 정지</button>");
                }
                else if (request.indexOf("GET /PAGE_DASHBOARD") >= 0) {
                    client.println("<h1>📊 대시보드</h1>");
                    client.println("<a href='/'><button class='btn back'>🏠 메인 메뉴로</button></a>");
                    client.println("<div style='text-align:left; background:#333; padding:20px; border-radius:10px;'>");
                    client.print("<p>📡 WiFi: <b>"); client.print(WiFi.RSSI()); client.println(" dBm</b></p>");
                    client.print("<p>⚖️ 무게: <b>"); 
                    float w = isSimulation ? 500.0 : scale.get_units(5);
                    client.print(w); client.println(" g</b></p>");
                    client.print("<p>🔄 모드: <b>"); client.print(isSimulation ? "시뮬레이션" : "실제 센서"); client.println("</b></p>");
                    client.println("</div>");
                }
                else {
                    if (currentMode != 0) { currentMode = 0; Serial.println(C_CYAN "\r\n[Web Sync] 🏠 메인 복귀" C_RESET); printMainMenu(); }
                    client.println("<h1>Smart Diffuser V8.0</h1>");
                    client.print("<p style='color:#888; margin-bottom:30px;'>IP: "); client.print(WiFi.localIP()); client.println("</p>");
                    client.println("<a href='/PAGE_MANUAL'><button class='btn blue'>[1] 🎮 수동 제어 (Manual)</button></a>");
                    client.println("<button class='btn purple' onclick=\"alert('터미널 입력 필요');\">[2] 💜 감성 모드</button>");
                    client.println("<button class='btn orange' onclick=\"alert('터미널 입력 필요');\">[3] 🌦️ 날씨 모드</button>");
                    client.println("<a href='/PAGE_DASHBOARD'><button class='btn teal'>[9] 📊 시스템 대시보드</button></a>");
                    client.println("<br>");
                    if (isSimulation) client.println("<button class='btn red' onclick=\"send('/TOGGLE_SIM'); location.reload();\">🔄 현재: 시뮬레이션 (ON)</button>");
                    else client.println("<button class='btn grey' onclick=\"send('/TOGGLE_SIM'); location.reload();\">🔄 현재: 실제 센서 (OFF)</button>");
                }
                client.println("</body></html>");
            }
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    delay(20); 
    client.stop();
  }
}

void runSprayLogic() {
  if (activePin == -1) return;
  unsigned long currentMillis = millis();
  
  if (isSpraying) {
    if (currentMillis - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH); 
      isSpraying = false;
      prevMotorMillis = currentMillis;
      if (currentMode == 1) {
          Serial.printf(C_CYAN "      └── [Manual] 동작 완료.\r\n" C_RESET);
          stopSystem();
          return;
      }
      Serial.printf(C_CYAN "      └── [Idle] ⏳ 휴식 중...\r\n" C_RESET);
    }
  } 
  else {
    if (currentMillis - prevMotorMillis >= REST_TIME) {
      forceAllOff(); 
      digitalWrite(activePin, LOW); 
      isSpraying = true;
      prevMotorMillis = currentMillis;
      Serial.printf(C_GREEN "      ┌── [Action] 💨 재분사 시작! (%d초)\r\n" C_RESET, sprayDuration/1000);
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
    // [Update] 데모 모드에서도 소리 나게 설정
    if (demoStep == 1) { target = PIN_SUNNY; name = "☀️ 맑음"; playSound(1); }
    else if (demoStep == 2) { target = PIN_CLOUDY; name = "☁️ 흐림"; playSound(2); }
    else if (demoStep == 3) { target = PIN_RAIN; name = "☔ 비"; playSound(3); }
    else if (demoStep == 4) { target = PIN_SNOW; name = "❄️ 눈"; playSound(4); }

    Serial.printf(C_MAGENTA "[Auto Demo] %s 모드 작동\r\n" C_RESET, name.c_str());
    digitalWrite(target, LOW); 
  }
}

void checkSafety() {
  if (millis() - startTimeMillis > MAX_RUN_TIME) {
    Serial.printf(C_RED "\r\n🚨 [Emergency] 안전 타이머 작동! 강제 종료.\r\n" C_RESET);
    stopSystem();
    currentMode = 0;
    printMainMenu();
  }
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    char c = Serial.peek(); 
    
    // [Mode 4: 정밀 세팅 모드]
    if (currentMode == 4) {
       char inputChar = Serial.read(); 
       if (inputChar == '\n' || inputChar == '\r') return; 
       
       if (inputChar == '+') { calibration_factor += 10; if(!isSimulation) scale.set_scale(calibration_factor); printCalibrationInfo(); }
       else if (inputChar == '-') { calibration_factor -= 10; if(!isSimulation) scale.set_scale(calibration_factor); printCalibrationInfo(); }
       else if (inputChar == 't') { 
           if(!isSimulation) scale.tare(); 
           Serial.printf(C_GREEN "⚖️ 영점 조절 완료 (Tare)\r\n" C_RESET); 
           printCalibrationInfo(); // 영점 잡고나서 0.0g 확인
       }
       else if (inputChar == 'w') { // [추가된 기능] w 누르면 무게만 확인
           printCalibrationInfo(); 
       }
       else if (inputChar == 's') { prefs.putFloat("cal_factor", calibration_factor); Serial.printf(C_BLUE "💾 [Save] 저장 완료!\r\n" C_RESET); }
       else if (inputChar == '0') { currentMode = 0; printMainMenu(); }
       return; 
    }

    // [나머지 모드]
    delay(50);
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      if (input == "0") { stopSystem(); currentMode = 0; printMainMenu(); } 
      else { handleInput(input); }
    }
  }
}

void printCalibrationInfo() {
    float w = isSimulation ? 500.0 : scale.get_units(5);
    Serial.printf("📡 보정값: %.1f | 현재 무게: %.2f g\r\n", calibration_factor, w);
}

void connectWiFi() {
  Serial.printf(C_YELLOW "[System] WiFi Connecting" C_RESET);
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 15) { delay(200); Serial.print("."); retry++; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\r\n" C_GREEN "[System] WiFi Connected! (%d dBm)\r\n" C_RESET, WiFi.RSSI());
    Serial.printf(C_CYAN "🌐 웹 컨트롤 주소: http://%s/\r\n" C_RESET, WiFi.localIP().toString().c_str());
  } else { Serial.printf(C_RED "\r\n[System] WiFi Failed. (Offline Mode)\r\n" C_RESET); }
}

void manageWiFi() {
  unsigned long currentMillis = millis();
  if (currentMillis - prevWifiMillis >= 30000) {
    prevWifiMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(); WiFi.reconnect(); }
  }
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
    Serial.printf(" ├─ WiFi RSSI  : %d dBm\r\n", WiFi.RSSI());
    Serial.printf(" ├─ Web Server : http://%s\r\n", WiFi.localIP().toString().c_str());
    Serial.printf(" ├─ Mode       : %s\r\n", isSimulation ? "SIMULATION" : "REAL SENSOR");
    Serial.printf(" ├─ Cal.Factor : %.1f (Saved)\r\n", calibration_factor);
    float w = isSimulation ? 500.0 : scale.get_units(10); 
    Serial.printf(" └─ Weight     : %.2f g\r\n", w);
    Serial.printf("----------------------------\r\n");
    printMainMenu();
}

void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") { currentMode = 1; Serial.printf(C_BLUE "\r\n--- [ Mode 1: 수동 제어 ] ---\r\n" C_RESET); }
    else if (input == "2") { currentMode = 2; Serial.printf(C_BLUE "\r\n--- [ Mode 2: 감성 모드 ] ---\r\n" C_RESET); }
    else if (input == "3") { 
      currentMode = 3; 
      Serial.printf(C_BLUE "\r\n--- [ Mode 3: 날씨 모드 ] ---\r\n" C_RESET); 
      // ✅ ADD: 날씨 모드 진입 시 타이머 초기화(바로 한 번 호출하고 싶으면 0으로 두면 됨)
      // 여기서는 "진입 즉시 호출"은 원본 요구에 없어서, 마지막 호출 시점을 현재로 세팅하지 않음.
      // lastWeatherMillis = 0; // (필요하면 주석 해제)
    }
    
    // [수정] 메뉴 설명에 'w:확인' 추가
    else if (input == "4") { 
        currentMode = 4; 
        Serial.printf(C_YELLOW "\r\n--- [ 🛠️ 정밀 세팅 ] ---\r\n" C_RESET); 
        Serial.println("👉 +/-:조절, w:무게확인, t:영점, s:저장, 0:종료"); 
    }
    
    else if (input == "5") { currentMode = 5; demoStep=0; Serial.printf(C_MAGENTA "\r\n--- [ ✨ 오토 데모 ] ---\r\n" C_RESET); }
    else if (input == "9") { printDashboard(); } 
    else if (input == "m" || input == "M") { isSimulation = !isSimulation; Serial.printf("\r\n🔄 모드변경: %s\r\n", isSimulation ? "SIMULATION" : "REAL"); printMainMenu(); }
    else { Serial.printf(C_RED "❌ 잘못된 입력\r\n" C_RESET); printMainMenu(); }
  }
  else if (currentMode == 1) runManualMode(input);

  // ✅ MOD: 감성 모드 payload에 device 추가 (unknown 방지)
  else if (currentMode == 2) { 
    Serial.printf(C_YELLOW "[Emotion] 분석 요청...\r\n" C_RESET); 
    sendServerRequest(
      String("{\"device\": \"") + DEVICE_ID + 
      "\", \"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}"
    ); 
  }

  // ✅ MOD: 날씨 모드(수동 지역 입력 호출) payload에 device 추가 (unknown 방지)
  else if (currentMode == 3) { 
      float w = isSimulation ? 500.0 : scale.get_units(10);
      Serial.printf(C_YELLOW "[Weather] 날씨 조회 (%.1fg)\r\n" C_RESET, w);
      sendServerRequest(
        String("{\"device\": \"") + DEVICE_ID + 
        "\", \"mode\": \"weather\", \"region\": \"" + input + "\", \"weight\": " + String(w) + "}"
      );

      // ✅ ADD: 사용자가 수동으로 호출했으면, 자동 호출 타이머도 여기서 갱신해 1시간 쿨다운 시작
      lastWeatherMillis = millis();
  }
}

void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { 
    forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; 
    myDFPlayer.stop();
    Serial.printf(C_RED "\r\n⛔ [System] 정지.\r\n" C_RESET); 
}

void runManualMode(String input) {
  int pin = -1;
  int track = 0;
  if (input == "1") { pin = PIN_SUNNY; track = 1; }
  else if (input == "2") { pin = PIN_CLOUDY; track = 2; }
  else if (input == "3") { pin = PIN_RAIN; track = 3; }
  else if (input == "4") { pin = PIN_SNOW; track = 4; }
  else { Serial.printf(C_RED "⚠️ 1~4 입력\r\n" C_RESET); return; }
  
  forceAllOff(); 
  activePin = pin; 
  isRunning = true; 
  isSpraying = true; 
  sprayDuration = 3000; 
  prevMotorMillis = millis(); 
  startTimeMillis = millis(); 
  
  digitalWrite(activePin, LOW); 
  playSound(track); // [New] 음악 재생
  Serial.printf(C_GREEN "[Loop] 분사 시작 (BGM %d번)\r\n" C_RESET, track);
}

void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) { Serial.printf(C_RED "🚨 WiFi 연결 안됨!\r\n" C_RESET); return; }
  HTTPClient http; http.setTimeout(5000); http.begin(serverName); http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  if(code > 0){
    String res = http.getString(); JsonDocument doc; deserializeJson(doc, res);
    int cmd = doc["spray"]; int dur = doc["duration"]; String txt = doc["result_text"];
    Serial.printf(C_GREEN "✅ %s\r\n" C_RESET, txt.c_str());
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY; else if (cmd == 2) target = PIN_CLOUDY; else if (cmd == 3) target = PIN_RAIN; else if (cmd == 4) target = PIN_SNOW;
    if (target != -1) { 
        forceAllOff(); activePin = target; isRunning = true; isSpraying = true; sprayDuration = dur * 1000; prevMotorMillis = millis(); startTimeMillis = millis(); 
        digitalWrite(activePin, LOW); 
        playSound(cmd); // [New] 서버 명령에 맞는 BGM 재생
        Serial.printf(C_GREEN "[Loop] 분사 시작\r\n" C_RESET); 
    }
    else { Serial.printf(C_RED "⚠️ 명령 없음\r\n" C_RESET); stopSystem(); }
  } else { Serial.printf(C_RED "🚨 통신 에러: %d\r\n" C_RESET, code); }
  http.end();
}

void printMainMenu() {
  Serial.printf(C_CYAN "\r\n=== 🕹️ MAIN MENU (V8.0 Audio) 🕹️ ===\r\n" C_RESET);
  Serial.printf(" [1] 수동   [2] 감성   [3] 날씨\r\n [4] 🛠️ 설정   [5] ✨ 데모   [9] 📊 대시보드\r\n [M] 🔄 시뮬레이션 전환\r\n" C_YELLOW "👉 명령 입력 >>" C_RESET);
}
