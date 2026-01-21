/*
 * [프로젝트] 스마트 디퓨저 (Smart Diffuser)
 * [버  전] 9.9.1 Voice Control (Color Fix)
 * [작성자] 21학번 류재홍
 * [기  능] 
 * 1. 기존 기능 통합 (GUI, 앱 제어, 로드셀, 스피커)
 * 2. [NEW] 음성 인식 모드: 2초 녹음 -> WAV 변환 -> HTTPS 전송 -> 명령 수행
 * 3. [FIX] 컴파일 에러(C_CYAN) 해결 완료
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
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
// ★ 색상 코드 정의 (수정됨)
#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m" // [복구]
#define C_CYAN    "\033[36m" // [복구]
#define C_BOLD    "\033[1m"

#define WDT_TIMEOUT 30 // 녹음/전송 시간 고려하여 30초로 늘림

// ★ 와이파이 & 서버 정보
const char* ssid     = "Jaehong_WiFi";        
const char* password = "12345678";          
String serverName = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 핀 정의 (기존 동일)
const int PIN_SUNNY  = 26; const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; const int PIN_SNOW   = 13; const int PIN_LED = 2;  
const int LOADCELL_DOUT_PIN = 16; const int LOADCELL_SCK_PIN  = 4;    
const int DFPLAYER_RX_PIN = 32; const int DFPLAYER_TX_PIN = 33; 

// 마이크 핀 (I2S)
#define I2S_WS  19  
#define I2S_SD  21  
#define I2S_SCK 18  
#define I2S_PORT I2S_NUM_0

// ★ 오디오 녹음 설정 (AWS 호환)
#define SAMPLE_RATE 16000    // 16kHz
#define BIT_PER_SAMPLE 16    // 16bit
#define RECORD_TIME 2        // 2초 녹음

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
String lastWebMessage = "Ready";
String inputBuffer = "";  
unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL = 2000; 
int demoStep = 0;
unsigned long prevDemoMillis = 0;
unsigned long prevMotorMillis = 0; 
unsigned long lastCheckTime = 0; 
unsigned long prevLedMillis = 0;
unsigned long startTimeMillis = 0; 
int ledBrightness = 0; int ledFadeAmount = 5;
unsigned long lastWeatherCallMillis = 0;
const unsigned long WEATHER_INTERVAL = 3600000; 
String lastWeatherRegion = "서울";              

// WAV 헤더 구조체
struct WavHeader {
  char riff[4]; uint32_t overall_size; char wave[4];
  char fmt_chunk_marker[4]; uint32_t length_of_fmt; uint16_t format_type; uint16_t channels;
  uint32_t sample_rate; uint32_t byterate; uint16_t block_align; uint16_t bits_per_sample;
  char data_chunk_header[4]; uint32_t data_size;
};

// 함수 선언
void initMicrophone(); 
int32_t readMicrophoneNoise(); // 노이즈 측정용
void recordAndSendVoice();     // ★ 핵심 기능
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
  
  esp_task_wdt_config_t wdt_config = { .timeout_ms = WDT_TIMEOUT * 1000, .idle_core_mask = (1 << 0) | (1 << 1), .trigger_panic = true };
  esp_task_wdt_init(&wdt_config); esp_task_wdt_add(NULL);

  pinMode(PIN_SUNNY, OUTPUT); pinMode(PIN_CLOUDY, OUTPUT);
  pinMode(PIN_RAIN, OUTPUT); pinMode(PIN_SNOW, OUTPUT); pinMode(PIN_LED, OUTPUT); 
  forceAllOff(); 

  Serial.print("\r\n\r\n");
  Serial.printf(C_BOLD " 🚀 SMART DIFFUSER V9.9.1 (Voice Control) \r\n" C_RESET);

  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!myDFPlayer.begin(mySoftwareSerial)) Serial.println(C_RED "Audio Fail" C_RESET);
  else Serial.println(C_GREEN "Audio OK" C_RESET);

  initMicrophone(); // 16kHz, 16bit 설정
  Serial.println(C_GREEN "Mic Init (16kHz)" C_RESET);

  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;
  currentVolume = prefs.getInt("volume", 20); 
  myDFPlayer.volume(currentVolume);

  connectWiFi();
  webServer.begin(); 
  
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
  pollServer(); 
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) { runSprayLogic(); checkSafety(); }
  checkSerialInput(); 
}

// ============================================================
// [3] ★ 음성 녹음 및 AWS 전송 (핵심 기능)
// ============================================================
void recordAndSendVoice() {
    if(WiFi.status() != WL_CONNECTED) { Serial.println(C_RED "WiFi Disconnected" C_RESET); return; }

    // 1. 녹음 준비 (메모리 할당)
    // 16000Hz * 2초 * 2바이트(16bit) = 64,000 바이트
    uint32_t dataSize = SAMPLE_RATE * RECORD_TIME * 2;
    uint32_t totalSize = sizeof(WavHeader) + dataSize;
    
    Serial.printf(C_YELLOW "\r\n[Voice] 녹음 시작! (2초, %d bytes)...\r\n" C_RESET, dataSize);
    
    // 힙 메모리(Heap)에 버퍼 할당 (스택 오버플로우 방지)
    uint8_t* audioBuffer = (uint8_t*)malloc(totalSize);
    if (audioBuffer == NULL) {
        Serial.println(C_RED "🚨 메모리 부족! (Malloc Failed)" C_RESET);
        return;
    }

    // 2. WAV 헤더 생성
    WavHeader header;
    memcpy(header.riff, "RIFF", 4);
    header.overall_size = totalSize - 8;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt_chunk_marker, "fmt ", 4);
    header.length_of_fmt = 16;
    header.format_type = 1; // PCM
    header.channels = 1;    // Mono
    header.sample_rate = SAMPLE_RATE;
    header.byterate = SAMPLE_RATE * 2;
    header.block_align = 2;
    header.bits_per_sample = 16;
    memcpy(header.data_chunk_header, "data", 4);
    header.data_size = dataSize;

    // 헤더를 버퍼 맨 앞에 복사
    memcpy(audioBuffer, &header, sizeof(WavHeader));

    // 3. 마이크 녹음 (I2S Read)
    size_t bytesRead = 0;
    // 버퍼의 헤더 뒤쪽부터 오디오 데이터 채우기
    esp_err_t result = i2s_read(I2S_PORT, (void*)(audioBuffer + sizeof(WavHeader)), dataSize, &bytesRead, portMAX_DELAY);
    
    if (result != ESP_OK) {
        Serial.println(C_RED "I2S Read Error" C_RESET);
        free(audioBuffer);
        return;
    }
    Serial.println(C_GREEN "[Voice] 녹음 완료! 서버 전송 중..." C_RESET);

    // 4. AWS Lambda로 전송 (HTTPS)
    WiFiClientSecure client;
    client.setInsecure(); // SSL 검증 무시
    
    HTTPClient http;
    http.setTimeout(15000); // 전송 시간 고려

    if (http.begin(client, serverName)) {
        http.addHeader("Content-Type", "audio/wav"); // WAV 파일임을 명시
        
        // 바이너리 데이터 POST 전송
        int httpCode = http.POST(audioBuffer, totalSize);
        
        if (httpCode > 0) {
            String res = http.getString();
            Serial.printf(C_GREEN "✅ 응답: %s\r\n" C_RESET, res.c_str());

            // JSON 파싱 및 동작 실행
            JsonDocument doc; deserializeJson(doc, res);
            int cmd = doc["spray"]; 
            int dur = doc["duration"];
            String txt = doc["result_text"];

            if (cmd > 0) {
                forceAllOff(); activePin = (cmd==1)?PIN_SUNNY : (cmd==2)?PIN_CLOUDY : (cmd==3)?PIN_RAIN : PIN_SNOW;
                isRunning = true; isSpraying = true; sprayDuration = dur * 1000;
                prevMotorMillis = millis(); startTimeMillis = millis();
                digitalWrite(activePin, LOW); playSound(cmd);
                lastWebMessage = "음성 명령 성공: " + txt;
                Serial.printf("👉 실행: %s (%d초)\r\n", txt.c_str(), dur);
            } else {
                Serial.printf("⚠️ 인식 실패/대기: %s\r\n", txt.c_str());
                lastWebMessage = "음성 인식 실패: " + txt;
            }
        } else {
            Serial.printf(C_RED "🚨 HTTP Error: %s\r\n" C_RESET, http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println(C_RED "🚨 Connection Failed" C_RESET);
    }

    // 5. 메모리 해제 (필수)
    free(audioBuffer);
    
    // 녹음 끝난 후 버퍼 지우기
    Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);
}

// ============================================================
// [4] 설정 및 기본 함수들
// ============================================================

void initMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,         // 16000Hz (변경됨)
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 16bit (변경됨)
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 64, // 버퍼 조정
    .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
  };
  const i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

// 노이즈 레벨 측정 (16bit 기준)
int32_t readMicrophoneNoise() {
  int16_t sample = 0; size_t bytes_read = 0; // 16bit로 읽기
  i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0); 
  if (bytes_read > 0) {
      int32_t rawValue = abs(sample);
      if (rawValue < 500) return 0; // 필터값 조정
      return rawValue; 
  }
  return 0;
}

// ============================================================
// [5] 나머지 통신 및 제어 함수 (기존 유지)
// ============================================================

void pollServer() {
    if (isRunning || currentMode == 5) return; 
    unsigned long now = millis();
    if (now - lastPollTime >= POLL_INTERVAL) {
        lastPollTime = now;
        if(WiFi.status() != WL_CONNECTED) return;
        WiFiClientSecure client; client.setInsecure();
        HTTPClient http; http.setTimeout(3000);
        if (http.begin(client, serverName)) {
            http.addHeader("Content-Type", "application/json");
            int code = http.POST("{\"action\": \"POLL\", \"deviceId\": \"App_User\"}");
            if (code > 0) {
                String res = http.getString();
                JsonDocument doc; deserializeJson(doc, res);
                int cmd = doc["spray"];
                if (cmd > 0) {
                      Serial.printf("\r\n" C_GREEN "📲 [APP] 명령: %d번\r\n" C_RESET, cmd);
                      forceAllOff(); activePin = (cmd==1)?PIN_SUNNY:(cmd==2)?PIN_CLOUDY:(cmd==3)?PIN_RAIN:PIN_SNOW;
                      isRunning = true; isSpraying = true; sprayDuration = 3000;
                      prevMotorMillis = millis(); startTimeMillis = millis();
                      digitalWrite(activePin, LOW); playSound(cmd); 
                      lastWebMessage = "앱 제어 실행 중...";
                }
            }
            http.end();
        }
    }
}

void sendServerRequest(String payload) {
  if(WiFi.status() != WL_CONNECTED) { Serial.println("WiFi Err"); return; }
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, serverName)) return;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  if(code > 0){
    String res = http.getString(); JsonDocument doc; deserializeJson(doc, res);
    int cmd = doc["spray"]; int dur = doc["duration"]; String txt = doc["result_text"];
    Serial.printf("\r\n✅ 응답: %s\r\n", txt.c_str());
    if (cmd > 0) { 
        forceAllOff(); activePin = (cmd==1)?PIN_SUNNY:(cmd==2)?PIN_CLOUDY:(cmd==3)?PIN_RAIN:PIN_SNOW;
        isRunning = true; isSpraying = true; sprayDuration = dur * 1000; 
        prevMotorMillis = millis(); startTimeMillis = millis(); 
        digitalWrite(activePin, LOW); playSound(cmd); 
        lastWebMessage = txt;
    }
  } 
  http.end();
  Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer);
}

void handleWebClient() {
  WiFiClient client = webServer.available();
  if (!client) return;
  unsigned long startTime = millis();
  while (!client.available() && millis() - startTime < 1000) { delay(1); }
  String request = "";
  while (client.connected() && client.available()) { char c = client.read(); request += c; }

  if (request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0 || request.indexOf("GET /VOL_") >= 0) {
      if (request.indexOf("GET /RUN_SUNNY") >= 0) runManualMode("1");
      if (request.indexOf("GET /RUN_CLOUDY") >= 0) runManualMode("2");
      if (request.indexOf("GET /RUN_RAIN") >= 0) runManualMode("3");
      if (request.indexOf("GET /RUN_SNOW") >= 0) runManualMode("4");
      if (request.indexOf("GET /STOP") >= 0) stopSystem();
      if (request.indexOf("GET /VOL_UP") >= 0) changeVolume(currentVolume + 2);
      if (request.indexOf("GET /VOL_DOWN") >= 0) changeVolume(currentVolume - 2);
      client.println("HTTP/1.1 204 No Content\r\nConnection: close\r\n");
  } else {
      client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");
      client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;text-align:center;background:#1a1a1a;color:white;padding:10px;}.btn{display:block;width:100%;max-width:400px;margin:10px auto;padding:15px;font-size:18px;border-radius:10px;border:none;color:white;cursor:pointer;}.vol{display:inline-block;width:48%;margin:5px 1%;padding:15px;}.sunny{background:#f1c40f;color:#000}.cloudy{background:#95a5a6}.rain{background:#3498db}.snow{background:#ecf0f1;color:#000}.red{background:#e74c3c}.blue{background:#3498db}.grey{background:#7f8c8d}</style><script>function send(u){fetch(u);setTimeout(()=>{location.reload()},500)</script></head><body>");
      client.printf("<h1>Smart Diffuser V9.9</h1><h3>🎤 Voice Control</h3><div style='background:#333;padding:15px;border-radius:10px;margin:10px auto;max-width:400px;'>📢 %s</div>", lastWebMessage.c_str());
      client.printf("<p>Mic Noise: %d | Weight: %.2fg</p>", readMicrophoneNoise(), scale.get_units(5));
      client.println("<button class='btn sunny' onclick=\"send('/RUN_SUNNY')\">☀️ 맑음</button><button class='btn cloudy' onclick=\"send('/RUN_CLOUDY')\">☁️ 흐림</button><button class='btn rain' onclick=\"send('/RUN_RAIN')\">☔ 비</button><button class='btn snow' onclick=\"send('/RUN_SNOW')\">❄️ 눈</button>");
      client.printf("<div><button class='btn vol grey' onclick=\"send('/VOL_DOWN')\">🔉 -</button><button class='btn vol blue' onclick=\"send('/VOL_UP')\">🔊 + (%d)</button></div><button class='btn red' onclick=\"send('/STOP')\">⛔ STOP</button></body></html>", currentVolume);
  }
  delay(10); client.stop();
}

void connectWiFi() {
  Serial.printf(C_YELLOW "[System] WiFi Connecting" C_RESET); WiFi.begin(ssid, password);
  int retry = 0; while(WiFi.status() != WL_CONNECTED && retry < 15) { delay(200); Serial.print("."); retry++; }
  if (WiFi.status() == WL_CONNECTED) { Serial.printf("\r\n" C_GREEN "Connected! (%d dBm)\r\n" C_RESET, WiFi.RSSI()); Serial.printf(C_CYAN "🌐 Web: http://%s/\r\n" C_RESET, WiFi.localIP().toString().c_str()); } 
  else { Serial.printf(C_RED "\r\nWiFi Failed.\r\n" C_RESET); }
}
void manageWiFi() { static bool c=true; if(millis()-lastCheckTime>=1000){lastCheckTime=millis(); if(WiFi.status()!=WL_CONNECTED){if(c){c=false;WiFi.disconnect();WiFi.reconnect();}}else if(!c)c=true;} }
void checkSerialInput() { while(Serial.available()>0){ char c=Serial.read(); if(c=='\n'||c=='\r'){if(inputBuffer.length()>0){Serial.println();if(inputBuffer=="0"){stopSystem();currentMode=0;printMainMenu();}else{handleInput(inputBuffer);}inputBuffer="";}}else if(c=='\b'||c==0x7F){if(inputBuffer.length()>0){inputBuffer.remove(inputBuffer.length()-1);redrawInputLine(inputBuffer);}}else{inputBuffer+=c;redrawInputLine(inputBuffer);}}}
void redrawInputLine(String &buffer) { Serial.print("\r\033[K"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(buffer); }
void handleInput(String input) {
    if(currentMode==0){
        if(input=="1"){currentMode=1;Serial.println("\r\n[Mode 1] 수동");}
        else if(input=="2"){currentMode=2;Serial.println("\r\n[Mode 2] 감성");}
        else if(input=="3"){currentMode=3;Serial.println("\r\n[Mode 3] 날씨");}
        else if(input=="4"){currentMode=4;Serial.println("\r\n[Mode 4] 설정");}
        else if(input=="5"){currentMode=5;Serial.println("\r\n[Mode 5] 데모"); demoStep=0;}
        else if(input=="6"){currentMode=6;Serial.println("\r\n[Mode 6] 🎤 음성 녹음 시작..."); recordAndSendVoice(); currentMode=0; printMainMenu();}
        else if(input=="9"){printDashboard();}
        printMainMenu();
    } else if(currentMode==1) { if(input=="+") changeVolume(currentVolume+2); else if(input=="-") changeVolume(currentVolume-2); else runManualMode(input); }
    else if(currentMode==2) { Serial.println("[Emotion] 요청..."); sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}"); }
    else if(currentMode==3) { lastWeatherRegion=input; float w=scale.get_units(10); Serial.printf("[Weather] %s\r\n",input.c_str()); sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + input + "\", \"w4\": " + String(w) + "}"); }
    else if(currentMode==4) { if(input=="+"){calibration_factor+=10;}else if(input=="-"){calibration_factor-=10;}else if(input=="t"){scale.tare();}else if(input=="s"){prefs.putFloat("cal_factor",calibration_factor);}else if(input=="0"){currentMode=0;printMainMenu();} if(input!="0")printCalibrationInfo(); }
}
void runManualMode(String input) { int t=-1; if(input=="1")t=PIN_SUNNY;else if(input=="2")t=PIN_CLOUDY;else if(input=="3")t=PIN_RAIN;else if(input=="4")t=PIN_SNOW; if(t!=-1){forceAllOff();activePin=t;isRunning=true;isSpraying=true;sprayDuration=3000;prevMotorMillis=millis();startTimeMillis=millis();digitalWrite(activePin,LOW);playSound(input.toInt());Serial.printf("\r\n[Manual] %s번\r\n",input.c_str());Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);}else if(input=="0"){currentMode=0;stopSystem();printMainMenu();} }
void runSprayLogic() { if(activePin==-1)return; if(isSpraying){if(millis()-prevMotorMillis>=sprayDuration){digitalWrite(activePin,HIGH);isSpraying=false;prevMotorMillis=millis();if(currentMode==1){stopSystem();printMainMenu();}else{Serial.println("\r\n[휴식]");Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);Serial.print(inputBuffer);}}}else{if(millis()-prevMotorMillis>=REST_TIME){forceAllOff();digitalWrite(activePin,LOW);isSpraying=true;prevMotorMillis=millis();Serial.println("\r\n[재분사]");Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);Serial.print(inputBuffer);}} }
void runAutoDemoLoop() { if(millis()-prevDemoMillis>=4000){prevDemoMillis=millis();forceAllOff();demoStep++;if(demoStep>4)demoStep=1;int t=-1;if(demoStep==1)t=PIN_SUNNY;else if(demoStep==2)t=PIN_CLOUDY;else if(demoStep==3)t=PIN_RAIN;else if(demoStep==4)t=PIN_SNOW;digitalWrite(t,LOW);playSound(demoStep);Serial.printf("\r\n[Demo] %d\r\n",demoStep);} }
void checkSafety() { if(millis()-startTimeMillis>MAX_RUN_TIME){stopSystem();Serial.println("🚨 안전 차단");} }
void printCalibrationInfo() { Serial.printf("Factor: %.1f, Weight: %.2f\r\n", calibration_factor, scale.get_units(5)); }
void systemHeartbeat() { if(!isRunning && millis()-prevLedMillis>=30){prevLedMillis=millis();ledBrightness+=ledFadeAmount;if(ledBrightness<=0||ledBrightness>=255)ledFadeAmount=-ledFadeAmount;analogWrite(PIN_LED,ledBrightness);} }
void bootAnimation() { for(int i=0;i<3;i++){digitalWrite(PIN_LED,HIGH);delay(100);digitalWrite(PIN_LED,LOW);delay(100);} }
void printDashboard() { Serial.println("\r\n--- Dashboard ---"); Serial.printf("WiFi: %d dBm\r\n", WiFi.RSSI()); Serial.printf("Weight: %.2f g\r\n", scale.get_units(10)); Serial.printf("Mic: %d\r\n", readMicrophoneNoise()); Serial.printf("Vol: %d\r\n", currentVolume); printMainMenu(); }
void playSound(int trackNum) { myDFPlayer.play(trackNum); }
void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; myDFPlayer.stop(); }
void changeVolume(int vol) { currentVolume=constrain(vol,0,30); myDFPlayer.volume(currentVolume); prefs.putInt("volume", currentVolume); Serial.printf("Vol: %d\r\n", currentVolume); }
void autoWeatherScheduler() { if(currentMode!=3||isRunning)return; if(millis()-lastWeatherCallMillis>=WEATHER_INTERVAL){lastWeatherCallMillis=millis();float w=scale.get_units(10);Serial.printf("\r\n[AUTO] 날씨 갱신\r\n");sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}");} }
void printMainMenu() { Serial.println("\r\n[1]수동 [2]감성 [3]날씨 [4]설정 [5]데모 [6]🎤음성"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); }
