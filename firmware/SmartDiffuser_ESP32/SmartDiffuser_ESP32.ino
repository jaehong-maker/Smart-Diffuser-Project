/*
 * [프로젝트명] 날씨 및 감정 기반 스마트 디퓨저 (Smart Diffuser)
 * [버전] 7.4 (Web Menu Navigation)
 * [작성자] 21학번 류재홍
 * [수정사항] 
 * - 웹 접속 시 '메인 메뉴'가 먼저 뜨도록 구조 변경
 * - [수동 제어] 버튼을 눌러야 컨트롤 패널로 진입
 * - [시스템 대시보드] 웹 페이지 추가
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h> 

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

const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;   
const long MAX_RUN_TIME   = 40000; 

// ============================================================
// [2] 전역 변수
// ============================================================
HX711 scale;
Preferences prefs;
WiFiServer webServer(80); 

float calibration_factor = 430.0; 
bool isSimulation = true; 

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

  Serial.print("\r\n\r\n");
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);
  Serial.printf(C_BOLD    "   🏆 SMART DIFFUSER V7.4 (NAV) 🏆      \r\n" C_RESET);
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);

  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;

  connectWiFi();
  webServer.begin(); 
  
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
  handleWebClient(); // 웹 요청 처리
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) {
    runSprayLogic();   
    checkSafety();     
  }
  checkSerialInput(); 
}

// ============================================================
// [5] 핵심 기능 함수들 (웹 서버 로직 강화)
// ============================================================

void handleWebClient() {
  WiFiClient client = webServer.available();
  if (client) {
    String currentLine = "";
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // HTTP 헤더
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            
            // --- [1] 요청 분석 및 동작 실행 ---
            // 모드 변경 명령
            if (request.indexOf("GET /SET_MANUAL") >= 0) { currentMode = 1; }
            if (request.indexOf("GET /SET_EMOTION") >= 0) { currentMode = 2; }
            if (request.indexOf("GET /SET_WEATHER") >= 0) { currentMode = 3; }
            if (request.indexOf("GET /SET_DEMO") >= 0) { currentMode = 5; demoStep = 0; }
            if (request.indexOf("GET /TOGGLE_SIM") >= 0) { isSimulation = !isSimulation; }
            
            // 수동 제어 명령
            if (request.indexOf("GET /RUN_SUNNY") >= 0) handleInput("1");
            if (request.indexOf("GET /RUN_CLOUDY") >= 0) handleInput("2");
            if (request.indexOf("GET /RUN_RAIN") >= 0) handleInput("3");
            if (request.indexOf("GET /RUN_SNOW") >= 0) handleInput("4");
            if (request.indexOf("GET /STOP") >= 0) { stopSystem(); currentMode=0; }

            // --- [2] 페이지 라우팅 (화면 그리기) ---
            
            // 공통 CSS (버튼 스타일)
            client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
            client.println("<style>");
            client.println("body { font-family: sans-serif; text-align: center; background: #1a1a1a; color: white; padding: 20px; }");
            client.println(".btn { display: block; width: 100%; max-width: 400px; margin: 12px auto; padding: 18px; font-size: 18px; border: none; border-radius: 12px; color: white; font-weight: bold; text-decoration: none; transition: 0.2s; }");
            client.println(".btn:active { transform: scale(0.95); opacity: 0.8; }");
            
            // 색상 클래스
            client.println(".blue { background: linear-gradient(to right, #2980b9, #6dd5fa); }"); // 수동
            client.println(".purple { background: linear-gradient(to right, #8e44ad, #c39bd3); }"); // 감성
            client.println(".orange { background: linear-gradient(to right, #d35400, #e67e22); }"); // 날씨
            client.println(".grey { background: linear-gradient(to right, #7f8c8d, #95a5a6); }");   // 세팅
            client.println(".gold { background: linear-gradient(to right, #f1c40f, #f39c12); color: #333; }"); // 데모
            client.println(".teal { background: linear-gradient(to right, #16a085, #1abc9c); }");   // 대시보드
            client.println(".red { background: linear-gradient(to right, #c0392b, #e74c3c); }");    // 시뮬/정지
            
            // 수동 제어용 색상
            client.println(".sunny { background: linear-gradient(to right, #f2994a, #f2c94c); color: #333; }");
            client.println(".cloudy { background: linear-gradient(to right, #bdc3c7, #2c3e50); }");
            client.println(".rain { background: linear-gradient(to right, #2980b9, #6dd5fa); }");
            client.println(".snow { background: linear-gradient(to right, #e0eafc, #cfdef3); color: #333; }");
            
            client.println(".back { background: #333; border: 1px solid #555; margin-bottom: 30px; }");
            client.println("</style></head><body>");

            // --- [화면 A] 수동 제어 페이지 (/PAGE_MANUAL) ---
            if (request.indexOf("GET /PAGE_MANUAL") >= 0 || request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0) {
                client.println("<h1>🎮 수동 제어 (Manual)</h1>");
                client.println("<a href='/'><button class='btn back'>🏠 메인 메뉴로 돌아가기</button></a>");
                
                client.println("<a href='/RUN_SUNNY'><button class='btn sunny'>☀️ 맑음 (SUNNY)</button></a>");
                client.println("<a href='/RUN_CLOUDY'><button class='btn cloudy'>☁️ 흐림 (CLOUDY)</button></a>");
                client.println("<a href='/RUN_RAIN'><button class='btn rain'>☔ 비 (RAIN)</button></a>");
                client.println("<a href='/RUN_SNOW'><button class='btn snow'>❄️ 눈 (SNOW)</button></a>");
                client.println("<br><a href='/STOP'><button class='btn red'>⛔ 긴급 정지 (STOP)</button></a>");
            }
            // --- [화면 B] 대시보드 페이지 (/PAGE_DASHBOARD) ---
            else if (request.indexOf("GET /PAGE_DASHBOARD") >= 0) {
                client.println("<h1>📊 시스템 대시보드</h1>");
                client.println("<a href='/'><button class='btn back'>🏠 메인 메뉴로 돌아가기</button></a>");
                
                client.println("<div style='text-align:left; background:#333; padding:20px; border-radius:10px;'>");
                client.print("<p>📡 WiFi 신호: <b>"); client.print(WiFi.RSSI()); client.println(" dBm</b></p>");
                client.print("<p>⏱️ 가동 시간: <b>"); client.print(millis()/1000); client.println(" 초</b></p>");
                client.print("<p>⚖️ 현재 무게: <b>"); 
                float w = isSimulation ? 500.0 : scale.get_units(5);
                client.print(w); client.println(" g</b></p>");
                client.print("<p>⚙️ 보정값: <b>"); client.print(calibration_factor); client.println("</b></p>");
                client.print("<p>🔄 모드: <b>"); client.print(isSimulation ? "시뮬레이션" : "실제 센서"); client.println("</b></p>");
                client.println("</div>");
                
                // 간단한 영점 조절 버튼 추가
                // client.println("<br><a href='/TARE'><button class='btn grey'>⚖️ 영점 잡기 (Tare)</button></a>");
            }
            // --- [화면 C] 메인 메뉴 (기본 화면) ---
            else {
                client.println("<h1>Smart Diffuser V7.4</h1>");
                client.print("<p style='color:#888; margin-bottom:30px;'>IP: "); client.print(WiFi.localIP()); client.println("</p>");
                
                client.println("<a href='/PAGE_MANUAL'><button class='btn blue'>[1] 🎮 수동 제어 (Manual)</button></a>");
                client.println("<a href='/' onclick=\"alert('터미널에서 2 입력하세요');\"><button class='btn purple'>[2] 💜 감성 모드 (Emotion)</button></a>");
                client.println("<a href='/' onclick=\"alert('터미널에서 3 입력하세요');\"><button class='btn orange'>[3] 🌦️ 날씨 모드 (Weather)</button></a>");
                client.println("<a href='/PAGE_DASHBOARD'><button class='btn teal'>[9] 📊 시스템 대시보드</button></a>");
                client.println("<a href='/SET_DEMO' onclick=\"alert('오토 데모 시작!');\"><button class='btn gold'>[5] ✨ 오토 데모 (Show)</button></a>");
                
                client.println("<br>");
                // 시뮬레이션 토글 버튼 (상태에 따라 텍스트 변경)
                if (isSimulation) {
                   client.println("<a href='/TOGGLE_SIM'><button class='btn red'>🔄 현재: 시뮬레이션 모드 (ON)</button></a>");
                } else {
                   client.println("<a href='/TOGGLE_SIM'><button class='btn grey'>🔄 현재: 실제 센서 모드 (OFF)</button></a>");
                }
            }
            
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
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
          Serial.printf(C_CYAN "      └── [Manual] 동작 완료. 대기 상태로 전환.\r\n" C_RESET);
          stopSystem();
          // printMainMenu(); // 웹 제어 시 터미널 메뉴 계속 띄우면 지저분해서 주석 처리
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
    if (demoStep == 1) { target = PIN_SUNNY; name = "☀️ 맑음"; }
    else if (demoStep == 2) { target = PIN_CLOUDY; name = "☁️ 흐림"; }
    else if (demoStep == 3) { target = PIN_RAIN; name = "☔ 비"; }
    else if (demoStep == 4) { target = PIN_SNOW; name = "❄️ 눈"; }

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
    if (currentMode == 4) {
       char inputChar = Serial.read(); 
       if (inputChar == '\n' || inputChar == '\r') return; 
       
       if (inputChar == '+') { calibration_factor += 10; if(!isSimulation) scale.set_scale(calibration_factor); printCalibrationInfo(); }
       else if (inputChar == '-') { calibration_factor -= 10; if(!isSimulation) scale.set_scale(calibration_factor); printCalibrationInfo(); }
       else if (inputChar == 't') { if(!isSimulation) scale.tare(); Serial.printf(C_GREEN "⚖️ 영점 조절 완료 (Tare)\r\n" C_RESET); }
       else if (inputChar == 's') { prefs.putFloat("cal_factor", calibration_factor); Serial.printf(C_BLUE "💾 [Save] 저장 완료!\r\n" C_RESET); }
       else if (inputChar == '0') { currentMode = 0; printMainMenu(); }
       return; 
    }
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
    else if (input == "3") { currentMode = 3; Serial.printf(C_BLUE "\r\n--- [ Mode 3: 날씨 모드 ] ---\r\n" C_RESET); }
    else if (input == "4") { currentMode = 4; Serial.printf(C_YELLOW "\r\n--- [ 🛠️ 정밀 세팅 ] ---\r\n" C_RESET); Serial.println("👉 +/-:조절, t:영점, s:저장, 0:종료"); }
    else if (input == "5") { currentMode = 5; demoStep=0; Serial.printf(C_MAGENTA "\r\n--- [ ✨ 오토 데모 ] ---\r\n" C_RESET); }
    else if (input == "9") { printDashboard(); } 
    else if (input == "m" || input == "M") { isSimulation = !isSimulation; Serial.printf("\r\n🔄 모드변경: %s\r\n", isSimulation ? "SIMULATION" : "REAL"); printMainMenu(); }
    else { Serial.printf(C_RED "❌ 잘못된 입력\r\n" C_RESET); printMainMenu(); }
  }
  else if (currentMode == 1) runManualMode(input);
  else if (currentMode == 2) { Serial.printf(C_YELLOW "[Emotion] 분석 요청...\r\n" C_RESET); sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}"); }
  else if (currentMode == 3) { 
      float w = isSimulation ? 500.0 : scale.get_units(10);
      Serial.printf(C_YELLOW "[Weather] 날씨 조회 (%.1fg)\r\n" C_RESET, w);
      sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + input + "\", \"weight\": " + String(w) + "}");
  }
}

void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; Serial.printf(C_RED "\r\n⛔ [System] 정지.\r\n" C_RESET); }

void runManualMode(String input) {
  int pin = -1;
  if (input == "1") pin = PIN_SUNNY; else if (input == "2") pin = PIN_CLOUDY; else if (input == "3") pin = PIN_RAIN; else if (input == "4") pin = PIN_SNOW;
  else { Serial.printf(C_RED "⚠️ 1~4 입력\r\n" C_RESET); return; }
  forceAllOff(); activePin = pin; isRunning = true; isSpraying = true; sprayDuration = 3000; prevMotorMillis = millis(); startTimeMillis = millis(); 
  digitalWrite(activePin, LOW); Serial.printf(C_GREEN "[Loop] 분사 시작\r\n" C_RESET);
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
    if (target != -1) { forceAllOff(); activePin = target; isRunning = true; isSpraying = true; sprayDuration = dur * 1000; prevMotorMillis = millis(); startTimeMillis = millis(); digitalWrite(activePin, LOW); Serial.printf(C_GREEN "[Loop] 분사 시작\r\n" C_RESET); }
    else { Serial.printf(C_RED "⚠️ 명령 없음\r\n" C_RESET); stopSystem(); }
  } else { Serial.printf(C_RED "🚨 통신 에러: %d\r\n" C_RESET, code); }
  http.end();
}

void printMainMenu() {
  Serial.printf(C_CYAN "\r\n=== 🕹️ MAIN MENU (V7.4 Nav) 🕹️ ===\r\n" C_RESET);
  Serial.printf(" [1] 수동   [2] 감성   [3] 날씨\r\n [4] 🛠️ 설정   [5] ✨ 데모   [9] 📊 대시보드\r\n [M] 🔄 시뮬레이션 전환\r\n" C_YELLOW "👉 명령 입력 >>" C_RESET);
}
