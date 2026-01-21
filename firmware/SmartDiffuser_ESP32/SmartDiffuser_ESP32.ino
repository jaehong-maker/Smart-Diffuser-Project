/*
 * [프로젝트] 스마트 디퓨저 (Smart Diffuser)
 * [버  전] 9.8 Full GUI Version (App Control + HTTPS + Web Design)
 * [작성자] 21학번 류재홍
 * [기  능] 
 * 1. 하드웨어 통합: 로드셀, 마이크(INMP441), 스피커(DFPlayer), LED
 * 2. 네트워크: AWS Lambda HTTPS 통신 (보안 패치 완료)
 * 3. 앱 연동: 폴링(Polling) 방식으로 앱 명령 즉시 수신
 * 4. 웹 서버: 화려한 UI (버튼 색상, CSS 적용) 복구
 */

#include <WiFi.h>
#include <WiFiClientSecure.h> // [필수] HTTPS 통신
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "HX711.h"
#include <Preferences.h>
#include "DFRobotDFPlayerMini.h" 
#include <esp_task_wdt.h> 
#include <driver/i2s.h>   

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

#define WDT_TIMEOUT 20 // 왓치독 20초

// ★ 와이파이 정보
const char* ssid     = "Jaehong_WiFi";        
const char* password = "12345678";          
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 1. 모터 & LED 핀
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

// 2. 로드셀 핀
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

// 3. DFPlayer 핀
const int DFPLAYER_RX_PIN = 32; 
const int DFPLAYER_TX_PIN = 33; 

// 4. 마이크(I2S) 핀
#define I2S_WS  19  
#define I2S_SD  21  
#define I2S_SCK 18  
#define I2S_PORT I2S_NUM_0

// 전역 객체
HardwareSerial mySoftwareSerial(2); 
DFRobotDFPlayerMini myDFPlayer;
HX711 scale;
Preferences prefs;
WiFiServer webServer(80); 

// 시스템 변수
unsigned long sprayDuration = 3000; 
const long REST_TIME      = 5000;    
const long MAX_RUN_TIME   = 270000; 
float calibration_factor = 430.0; 
int currentVolume = 20;    
int currentMode = 0;        
bool isRunning = false;    
int activePin = -1;        
bool isSpraying = false;    
String lastWebMessage = "시스템 준비 완료 (Ready)";
String inputBuffer = "";  

// ★ 폴링(앱 명령) 변수
unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL = 2000; 

// 기타 변수
int demoStep = 0;
unsigned long prevDemoMillis = 0;
unsigned long prevMotorMillis = 0; 
unsigned long lastCheckTime = 0; 
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; 
int ledBrightness = 0;
int ledFadeAmount = 5;
unsigned long lastWeatherCallMillis = 0;
const unsigned long WEATHER_INTERVAL = 3600000; 
String lastWeatherRegion = "서울";              

// 함수 선언
void initMicrophone(); 
int32_t readMicrophone(); 
void pollServer(); 
void sendServerRequest(String payload);
void connectWiFi();
void manageWiFi();
void forceAllOff();
void stopSystem();
void playSound(int trackNum);
void changeVolume(int vol);
void printMainMenu();
void handleWebClient();
void checkSerialInput();
void handleInput(String input);
void runSprayLogic();
void checkSafety();
void runAutoDemoLoop();
void systemHeartbeat();
void bootAnimation();
void printDashboard();
void printCalibrationInfo();
void redrawInputLine(String &buffer);
void runManualMode(String input);
void autoWeatherScheduler();

// ============================================================
// [1] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5000); 
  
  // 왓치독 설정
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT * 1000,
      .idle_core_mask = (1 << 0) | (1 << 1),
      .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  pinMode(PIN_SUNNY, OUTPUT);
  pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT);
  pinMode(PIN_SNOW, OUTPUT);
  pinMode(PIN_LED, OUTPUT); 
  forceAllOff(); 

  Serial.print("\r\n\r\n");
  Serial.printf(C_BOLD " 🚀 SMART DIFFUSER V9.8 (Full GUI Version) \r\n" C_RESET);

  // 스피커
  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!myDFPlayer.begin(mySoftwareSerial)) Serial.println(C_RED "Audio Fail" C_RESET);
  else Serial.println(C_GREEN "Audio OK" C_RESET);

  // 마이크
  initMicrophone();
  Serial.println(C_GREEN "Mic OK" C_RESET);

  // 설정 로드
  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;
  currentVolume = prefs.getInt("volume", 20); 
  myDFPlayer.volume(currentVolume);

  // 통신
  connectWiFi();
  webServer.begin(); 
  
  // 로드셀
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  
  bootAnimation();
  printMainMenu(); 
}

// ============================================================
// [2] 메인 루프
// ============================================================
void loop() {
  esp_task_wdt_reset(); 

  manageWiFi();       
  systemHeartbeat(); 
  handleWebClient(); 
  autoWeatherScheduler();
  pollServer(); // 앱 명령 확인
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) {
    runSprayLogic();    
    checkSafety();      
  }
  checkSerialInput(); 
}

// ============================================================
// [3] 통신 핵심 함수 (폴링 & HTTPS)
// ============================================================

// 앱 명령 확인 (Poll)
void pollServer() {
    if (isRunning || currentMode == 5) return; 
    unsigned long now = millis();
    if (now - lastPollTime >= POLL_INTERVAL) {
        lastPollTime = now;
        if(WiFi.status() != WL_CONNECTED) return;

        WiFiClientSecure client;
        client.setInsecure(); // SSL 무시
        
        HTTPClient http;
        http.setTimeout(3000);
        
        if (http.begin(client, serverName)) {
            http.addHeader("Content-Type", "application/json");
            int code = http.POST("{\"action\": \"POLL\", \"deviceId\": \"App_User\"}");
            
            if (code > 0) {
                String res = http.getString();
                JsonDocument doc; deserializeJson(doc, res);
                int cmd = doc["spray"];
                
                if (cmd > 0) {
                     Serial.printf("\r\n" C_GREEN "📲 [APP] 명령 수신: %d번 분사\r\n" C_RESET, cmd);
                     int t = -1;
                     if (cmd == 1) t = PIN_SUNNY; else if (cmd == 2) t = PIN_CLOUDY; 
                     else if (cmd == 3) t = PIN_RAIN; else if (cmd == 4) t = PIN_SNOW;
                     
                     if (t != -1) {
                         forceAllOff(); activePin = t; isRunning = true; isSpraying = true; 
                         sprayDuration = 3000; prevMotorMillis = millis(); startTimeMillis = millis(); 
                         digitalWrite(activePin, LOW); playSound(cmd); 
                         lastWebMessage = "앱 제어 실행 중...";
                         Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer);
                     }
                }
            }
            http.end();
        }
    }
}

// 일반 서버 요청 (날씨 등)
void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) { 
      Serial.print("\r\033[K"); Serial.println(C_RED "🚨 WiFi 연결 끊김" C_RESET); 
      Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); return; 
  }
  
  WiFiClientSecure client;
  client.setInsecure(); // ★ HTTPS 패치
  
  HTTPClient http; 
  http.setTimeout(10000); 
  
  if (!http.begin(client, serverName)) {
      Serial.println(C_RED "🚨 HTTPS 초기화 실패" C_RESET); return;
  }

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  
  if(code > 0){
    String res = http.getString(); 
    JsonDocument doc; deserializeJson(doc, res);
    int cmd = doc["spray"]; int dur = doc["duration"]; String txt = doc["result_text"];
    
    Serial.print("\r\033[K");
    Serial.printf(C_GREEN "✅ 서버 응답: %s\r\n" C_RESET, txt.c_str());
    
    int target = -1;
    if (cmd == 1) target = PIN_SUNNY; else if (cmd == 2) target = PIN_CLOUDY; 
    else if (cmd == 3) target = PIN_RAIN; else if (cmd == 4) target = PIN_SNOW;
    
    if (target != -1) { 
        forceAllOff(); activePin = target; isRunning = true; isSpraying = true; 
        sprayDuration = dur * 1000; prevMotorMillis = millis(); startTimeMillis = millis(); 
        digitalWrite(activePin, LOW); playSound(cmd); 
        lastWebMessage = "성공: " + txt;
    } else { 
        lastWebMessage = "대기: " + txt;
        stopSystem(); 
    }
    Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer);
  } else { 
      Serial.printf(C_RED "\r\n🚨 통신 에러: %d\r\n" C_RESET, code); 
      lastWebMessage = "통신 에러 (" + String(code) + ")";
      Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer);
  }
  http.end();
}

// ============================================================
// [4] 웹 서버 (HTML/CSS 복구)
// ============================================================
void handleWebClient() {
  WiFiClient client = webServer.available();
  if (!client) return;
  unsigned long startTime = millis();
  while (!client.available() && millis() - startTime < 1000) { delay(1); }

  String request = "";
  while (client.connected() && client.available()) {
      char c = client.read();
      request += c;
  }

  // 간단 명령 처리
  if (request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0 || request.indexOf("GET /VOL_") >= 0) {
      if (request.indexOf("GET /RUN_SUNNY") >= 0) { runManualMode("1"); lastWebMessage = "수동: 맑음"; }
      if (request.indexOf("GET /RUN_CLOUDY") >= 0) { runManualMode("2"); lastWebMessage = "수동: 흐림"; }
      if (request.indexOf("GET /RUN_RAIN") >= 0) { runManualMode("3"); lastWebMessage = "수동: 비"; }
      if (request.indexOf("GET /RUN_SNOW") >= 0) { runManualMode("4"); lastWebMessage = "수동: 눈"; }
      if (request.indexOf("GET /STOP") >= 0) { stopSystem(); lastWebMessage = "정지됨"; }
      if (request.indexOf("GET /VOL_UP") >= 0) { changeVolume(currentVolume + 2); }
      if (request.indexOf("GET /VOL_DOWN") >= 0) { changeVolume(currentVolume - 2); }
      client.println("HTTP/1.1 204 No Content\r\nConnection: close\r\n"); // 화면 새로고침 방지
  } 
  else {
      // 메인 페이지 출력
      client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");
      client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
      client.println("<style>");
      client.println("body{font-family:sans-serif;text-align:center;background:#1a1a1a;color:white;padding:10px;}");
      client.println(".btn{display:block;width:100%;max-width:400px;margin:10px auto;padding:15px;font-size:18px;border-radius:10px;border:none;color:white;cursor:pointer;}");
      client.println(".vol{display:inline-block;width:48%;margin:5px 1%;padding:15px;}");
      client.println(".status{background:#333;color:#0f0;padding:15px;border-radius:10px;border:1px solid #555;max-width:400px;margin:10px auto;}");
      client.println(".sunny{background:#f1c40f;color:#000}.cloudy{background:#95a5a6}.rain{background:#3498db}.snow{background:#ecf0f1;color:#000}");
      client.println(".red{background:#e74c3c}.blue{background:#3498db}.grey{background:#7f8c8d}");
      client.println("</style>");
      client.println("<script>function send(u){fetch(u);setTimeout(()=>{location.reload()},500)}</script>");
      client.println("</head><body>");
      
      client.printf("<h1>Smart Diffuser V9.8</h1>");
      client.printf("<div class='status'>📢 상태: %s</div>", lastWebMessage.c_str());
      client.printf("<div style='background:#222;padding:10px;border-radius:10px;margin:10px auto;max-width:400px;'>");
      client.printf("<p>🎤 소리(Noise): %d</p>", readMicrophone());
      client.printf("<p>⚖️ 무게(CH4): %.2fg</p>", scale.get_units(5));
      client.printf("<p>📡 WiFi: %ddBm</p></div>", WiFi.RSSI());

      client.println("<button class='btn sunny' onclick=\"send('/RUN_SUNNY')\">☀️ 맑음</button>");
      client.println("<button class='btn cloudy' onclick=\"send('/RUN_CLOUDY')\">☁️ 흐림</button>");
      client.println("<button class='btn rain' onclick=\"send('/RUN_RAIN')\">☔ 비</button>");
      client.println("<button class='btn snow' onclick=\"send('/RUN_SNOW')\">❄️ 눈</button>");
      
      client.printf("<div style='max-width:400px;margin:0 auto;'><button class='btn vol grey' onclick=\"send('/VOL_DOWN')\">🔉 Vol -</button>");
      client.printf("<button class='btn vol blue' onclick=\"send('/VOL_UP')\">🔊 Vol + (%d)</button></div>", currentVolume);
      
      client.println("<button class='btn red' onclick=\"send('/STOP')\">⛔ STOP</button>");
      client.println("</body></html>");
  }
  delay(10); client.stop();
}

// ============================================================
// [5] 기타 기능 함수들
// ============================================================

void initMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100, .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 4, .dma_buf_len = 1024,
    .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
  };
  const i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

int32_t readMicrophone() {
  int32_t sample = 0; size_t bytes_read = 0;
  i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0); 
  if (bytes_read > 0) {
      int32_t rawValue = abs(sample) / 10000;
      if (rawValue < 300) return 0;
      return rawValue; 
  }
  return 0;
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

void manageWiFi() {
  static bool wasConnected = true; 
  if (millis() - lastCheckTime >= 1000) {
    lastCheckTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) { 
        wasConnected = false;
        WiFi.disconnect(); WiFi.reconnect();
      }
    } else if (!wasConnected) wasConnected = true;
  }
}

void autoWeatherScheduler() {
  if (currentMode != 3 || isRunning) return;
  if (millis() - lastWeatherCallMillis >= WEATHER_INTERVAL) {
    lastWeatherCallMillis = millis();
    float w = scale.get_units(10); 
    Serial.printf(C_YELLOW "\r\n[AUTO] 날씨 갱신 (%s)\r\n" C_RESET, lastWeatherRegion.c_str());
    sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}");
  }
}

void checkSerialInput() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        Serial.println();
        if (inputBuffer == "0") { stopSystem(); currentMode = 0; printMainMenu(); }
        else { handleInput(inputBuffer); }
        inputBuffer = "";
      }
    } else if (c == '\b' || c == 0x7F) {
      if (inputBuffer.length() > 0) { inputBuffer.remove(inputBuffer.length() - 1); redrawInputLine(inputBuffer); }
    } else {
      inputBuffer += c; redrawInputLine(inputBuffer);
    }
  }
}

void redrawInputLine(String &buffer) {
  Serial.print("\r\033[K"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(buffer);
}

void handleInput(String input) {
    if(currentMode==0){
        if(input=="1"){currentMode=1;Serial.println("\r\n[Mode 1] 수동 제어");}
        else if(input=="2"){currentMode=2;Serial.println("\r\n[Mode 2] 감성 모드");}
        else if(input=="3"){currentMode=3;Serial.println("\r\n[Mode 3] 날씨 모드");}
        else if(input=="4"){currentMode=4;Serial.println("\r\n[Mode 4] 설정");}
        else if(input=="5"){currentMode=5;Serial.println("\r\n[Mode 5] 데모"); demoStep=0;}
        else if(input=="9"){printDashboard();}
        printMainMenu();
    } else if(currentMode==1) {
        if(input=="+") changeVolume(currentVolume+2);
        else if(input=="-") changeVolume(currentVolume-2);
        else runManualMode(input);
    } else if(currentMode==2) {
        Serial.println("[Emotion] 분석 요청...");
        sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}");
    } else if(currentMode==3) {
        lastWeatherRegion = input;
        float w = scale.get_units(10);
        Serial.printf("[Weather] 조회: %s\r\n", input.c_str());
        sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + input + "\", \"w4\": " + String(w) + "}");
    } else if(currentMode==4) {
        if(input=="+") { calibration_factor+=10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
        else if(input=="-") { calibration_factor-=10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
        else if(input=="t") { scale.tare(); Serial.println("Tare OK"); }
        else if(input=="s") { prefs.putFloat("cal_factor", calibration_factor); Serial.println("Saved"); }
        else if(input=="0") { currentMode=0; printMainMenu(); }
    }
}

void runManualMode(String input) {
    int t = -1;
    if(input=="1") t=PIN_SUNNY; else if(input=="2") t=PIN_CLOUDY; 
    else if(input=="3") t=PIN_RAIN; else if(input=="4") t=PIN_SNOW;
    
    if(t != -1) {
        forceAllOff(); activePin=t; isRunning=true; isSpraying=true; sprayDuration=3000;
        prevMotorMillis=millis(); startTimeMillis=millis();
        digitalWrite(activePin, LOW); playSound(input.toInt());
        Serial.printf("\r\n[Manual] %s번 실행\r\n", input.c_str());
        Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);
    } else if(input=="0") {
        currentMode=0; stopSystem(); printMainMenu();
    }
}

void runSprayLogic() {
  if (activePin == -1) return;
  if (isSpraying) {
    if (millis() - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH); isSpraying = false; prevMotorMillis = millis();
      
      if(currentMode == 1) { stopSystem(); Serial.println("\r\n[완료] 동작 끝"); printMainMenu(); }
      else { Serial.println("\r\n[휴식] 대기 중..."); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); }
    }
  } else {
    if (millis() - prevMotorMillis >= REST_TIME) {
      forceAllOff(); digitalWrite(activePin, LOW); isSpraying = true; prevMotorMillis = millis();
      Serial.println("\r\n[재분사] 시작!"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer);
    }
  }
}

void runAutoDemoLoop() {
  if (millis() - prevDemoMillis >= 4000) {
    prevDemoMillis = millis(); forceAllOff(); 
    demoStep++; if (demoStep > 4) demoStep = 1; 
    int t = -1;
    if (demoStep == 1) t = PIN_SUNNY; else if (demoStep == 2) t = PIN_CLOUDY;
    else if (demoStep == 3) t = PIN_RAIN; else if (demoStep == 4) t = PIN_SNOW;
    digitalWrite(t, LOW); playSound(demoStep);
    Serial.printf("\r\n[Demo] Step %d\r\n", demoStep);
  }
}

void checkSafety() { if (millis() - startTimeMillis > MAX_RUN_TIME) { stopSystem(); Serial.println("🚨 안전 차단"); } }
void printCalibrationInfo() { Serial.printf("Factor: %.1f, Weight: %.2f\r\n", calibration_factor, scale.get_units(5)); }
void systemHeartbeat() {
    if(!isRunning && millis()-prevLedMillis>=30) {
        prevLedMillis=millis(); ledBrightness+=ledFadeAmount;
        if(ledBrightness<=0 || ledBrightness>=255) ledFadeAmount=-ledFadeAmount;
        analogWrite(PIN_LED, ledBrightness);
    }
}
void bootAnimation() { for(int i=0;i<3;i++){digitalWrite(PIN_LED,HIGH);delay(100);digitalWrite(PIN_LED,LOW);delay(100);} }
void printDashboard() {
    Serial.println("\r\n--- Dashboard ---");
    Serial.printf("WiFi: %d dBm\r\n", WiFi.RSSI());
    Serial.printf("Weight: %.2f g\r\n", scale.get_units(10));
    Serial.printf("Mic: %d\r\n", readMicrophone());
    Serial.printf("Vol: %d\r\n", currentVolume);
    printMainMenu();
}
void playSound(int trackNum) { myDFPlayer.play(trackNum); }
void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; myDFPlayer.stop(); }
void changeVolume(int vol) { currentVolume=constrain(vol,0,30); myDFPlayer.volume(currentVolume); prefs.putInt("volume", currentVolume); Serial.printf("Vol: %d\r\n", currentVolume); }
void printMainMenu() { Serial.println("\r\n[1]수동 [2]감성 [3]날씨 [4]설정 [5]데모 [9]상태"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); }
