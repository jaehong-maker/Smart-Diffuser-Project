/*
 * [프로젝트] 스마트 디퓨저 (Smart Diffuser)
 * [버  전] V10.3 Sound Reactive (dB Trigger)
 * [작성자] 21학번 류재홍
 * [기  능] 
 * 1. 기존 V10.2 기능 100% 유지 (음성, 앱, 웹, 시각화 등)
 * 2. [추가] Mode 8: 특정 데시벨(dB) 이상일 때 자동 분사
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
#include <math.h> // dB 계산용 로그 함수

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

#define WDT_TIMEOUT 60 

const char* ssid      = "Jaehong_WiFi";        
const char* password  = "12345678";          
String serverName     = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 1. 모터/LED
const int PIN_SUNNY  = 26; 
const int PIN_CLOUDY = 27; 
const int PIN_RAIN   = 14; 
const int PIN_SNOW   = 13; 
const int PIN_LED    = 2;  

// 2. 로드셀
const int LOADCELL_DOUT_PIN = 16; 
const int LOADCELL_SCK_PIN  = 4;    

// 3. DFPlayer
const int DFPLAYER_RX_PIN = 32; 
const int DFPLAYER_TX_PIN = 33; 

// 4. Nextion Display
const int NEXTION_RX_PIN = 22; 
const int NEXTION_TX_PIN = 23; 

// 5. INMP441 마이크 (I2S)
#define I2S_WS  19  
#define I2S_SD  21  
#define I2S_SCK 18  
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE 16000    
#define BIT_PER_SAMPLE 16    
#define RECORD_TIME 2        

// 전역 객체
HardwareSerial mySoftwareSerial(2); 
HardwareSerial myNextionSerial(1);  
DFRobotDFPlayerMini myDFPlayer;
HX711 scale;
Preferences prefs;
WiFiServer webServer(80); 

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

unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL = 2000; 

float lastWeight = 0.0;
unsigned long lastWeightCheckTime = 0;
const float WEIGHT_THRESHOLD = 5.0; 

// ★ [NEW] 소리 반응 모드 변수
int soundThreshold = 60;          // 기준 데시벨 (기본 60dB)
unsigned long lastReactionTime = 0; // 연속 실행 방지 쿨타임

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

struct WavHeader {
  char riff[4]; uint32_t overall_size; char wave[4];
  char fmt_chunk_marker[4]; uint32_t length_of_fmt; uint16_t format_type; uint16_t channels;
  uint32_t sample_rate; uint32_t byterate; uint16_t block_align; uint16_t bits_per_sample;
  char data_chunk_header[4]; uint32_t data_size;
};

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
void changeVolume(int vol); 
void initMicrophone(); 
int32_t readMicrophone(); 
void pollServer(); 
void recordAndSendVoice(); 
void monitorWeight(); 
void updateDisplay(int iconID, String text); 
void updateProgressBar(int val); 
void runSoundVisualizer(); // Mode 7
void runSoundReaction();   // Mode 8 (NEW)

// ============================================================
// [1] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5000); 
  
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 60000, 
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
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);
  Serial.printf(C_BOLD    " 🚀 SMART DIFFUSER V10.3 (Reactive) \r\n" C_RESET);
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);

  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!myDFPlayer.begin(mySoftwareSerial)) Serial.println("Audio Fail");
  else Serial.println("Audio OK");

  myNextionSerial.begin(9600, SERIAL_8N1, NEXTION_RX_PIN, NEXTION_TX_PIN);
  updateDisplay(0, "System Ready"); 

  initMicrophone(); 

  prefs.begin("diffuser", false); 
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;
  currentVolume = prefs.getInt("volume", 20); 
  myDFPlayer.volume(currentVolume);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare(); 
  lastWeight = scale.get_units(5); 

  connectWiFi();
  webServer.begin(); 
  bootAnimation();
  printMainMenu(); 
}

// ============================================================
// ★ [NEW] Mode 8: 소리 반응 모드 (dB 트리거)
// ============================================================
void runSoundReaction() {
    int32_t samples[64]; // 샘플 64개씩 읽어서 평균 냄
    size_t bytes_read = 0;
    
    // I2S 데이터 읽기
    i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, 0);
    
    if (bytes_read > 0) {
        double sumSquares = 0;
        int numSamples = bytes_read / 4; 
        
        // RMS(제곱평균제곱근) 계산 -> 소리 에너지 구하기
        for (int i = 0; i < numSamples; i++) {
            int16_t wave = (samples[i] >> 14); 
            sumSquares += (double)wave * wave;
        }
        double rms = sqrt(sumSquares / numSamples);
        if (rms < 1) rms = 1; 
        
        // dB 변환 (상대값)
        // RMS 10000일 때 약 80dB 정도로 매핑
        int db = (int)(20 * log10(rms)); 
        
        // 1. 상태 모니터링 (0.5초마다 출력)
        static unsigned long lastPrint = 0;
        if (millis() - lastPrint > 500) {
            lastPrint = millis();
            Serial.printf("\r\033[K📢 Noise: %d dB (Target: >%d)\r", db, soundThreshold);
            updateDisplay(0, String(db) + " dB / " + String(soundThreshold));
        }

        // 2. 반응 로직 (설정값 초과 시 발사)
        if (db >= soundThreshold) {
             // 5초 쿨타임 (너무 자주 뿌리면 안되니까)
             if (millis() - lastReactionTime > 5000) { 
                 lastReactionTime = millis();
                 
                 Serial.printf("\n\n🎉 [Sound Trigger] %d dB 감지! 자동 분사!\n", db);
                 updateDisplay(0, "LOUD NOISE!");
                 
                 // 랜덤 향기 분사 (1~4번 중 하나)
                 int cmd = random(1, 5); 
                 
                 forceAllOff();
                 if (cmd==1) activePin=PIN_SUNNY; else if (cmd==2) activePin=PIN_CLOUDY;
                 else if (cmd==3) activePin=PIN_RAIN; else if (cmd==4) activePin=PIN_SNOW;
                 
                 isRunning = true; isSpraying = true; sprayDuration = 3000;
                 prevMotorMillis = millis(); startTimeMillis = millis();
                 digitalWrite(activePin, LOW); playSound(cmd);
                 
                 updateDisplay(cmd, "Sound Active");
             }
        }
    }
}

// ============================================================
// Mode 7: 소리 파형 시각화 (테라텀용)
// ============================================================
void runSoundVisualizer() {
    int32_t sample = 0; size_t bytes_read = 0;
    i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0);
    
    if (bytes_read > 0) {
        int16_t wave = (sample >> 14); 
        int vol = abs(wave); 

        static unsigned long lastVisUpdate = 0;
        if (millis() - lastVisUpdate > 100) { 
            lastVisUpdate = millis();
            if (vol < 100) vol = 0;
            int bars = map(vol, 0, 5000, 0, 30); bars = constrain(bars, 0, 30); 

            Serial.print("\r\033[K"); Serial.print(C_CYAN "🎤 Sound: " C_RESET);
            Serial.print(C_GREEN); for(int i=0; i<bars; i++) Serial.print("█");
            for(int i=bars; i<30; i++) Serial.print(" ");
            Serial.print(C_RESET); Serial.printf(" [%d]", vol);

            int gauge = map(vol, 0, 5000, 0, 100);
            updateProgressBar(constrain(gauge, 0, 100));
        }
    }
}

void updateProgressBar(int val) {
    myNextionSerial.print("j0.val="); myNextionSerial.print(val);
    myNextionSerial.write(0xff); myNextionSerial.write(0xff); myNextionSerial.write(0xff);
}

// ============================================================
// 기존 함수들 (일부 수정)
// ============================================================

void loop() {
  esp_task_wdt_reset(); 
  
  // 모드별 루프 분기
  if (currentMode == 7) { runSoundVisualizer(); checkSerialInput(); return; }
  if (currentMode == 8) { runSoundReaction();   checkSerialInput(); return; } // ★ Mode 8 추가

  manageWiFi();       
  systemHeartbeat(); 
  handleWebClient(); 
  autoWeatherScheduler();
  pollServer(); 
  monitorWeight(); 
  
  if (currentMode == 5) runAutoDemoLoop(); 
  else if (isRunning) { runSprayLogic(); checkSafety(); }
  checkSerialInput(); 
}

void checkSerialInput() {
  // Mode 8 (소리 반응)일 때 감도 조절 기능
  if (currentMode == 8) {
      if (Serial.available() > 0) {
          char c = Serial.read();
          if (c == '\n' || c == '\r') return;
          
          if (c == '+') { 
              soundThreshold += 2; 
              Serial.printf("\n🆙 기준값 증가: %d dB\n", soundThreshold); 
          }
          else if (c == '-') { 
              soundThreshold -= 2; 
              Serial.printf("\n⬇️ 기준값 감소: %d dB\n", soundThreshold); 
          }
          else if (c == '0') { 
              currentMode = 0; stopSystem(); printMainMenu(); 
          }
      }
      return;
  }

  // ... (기존 checkSerialInput 내용 유지) ...
  if (currentMode == 4) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') return; 
      if (c == '+') { calibration_factor += 10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
      else if (c == '-') { calibration_factor -= 10; scale.set_scale(calibration_factor); printCalibrationInfo(); }
      else if (c == 't') { scale.tare(); Serial.printf(C_GREEN "\r\n⚖️ 영점 조절 완료\r\n" C_RESET); printCalibrationInfo(); }
      else if (c == 's') { prefs.putFloat("cal_factor", calibration_factor); Serial.printf(C_BLUE "\r\n💾 저장 완료!\r\n" C_RESET); }
      else if (c == '0') { currentMode = 0; printMainMenu(); }
    }
    return;
  }
  
  while (Serial.available() > 0) {
    char c = Serial.read(); 
    if (c == '\n' || c == '\r') { 
      if (inputBuffer.length() > 0) {
        Serial.println(); 
        if (inputBuffer == "0") { stopSystem(); currentMode = 0; printMainMenu(); } 
        else { handleInput(inputBuffer); }
        inputBuffer = ""; 
      }
    }
    else if (c == '\b' || c == 0x7F) { 
      if (inputBuffer.length() > 0) { inputBuffer.remove(inputBuffer.length() - 1); redrawInputLine(inputBuffer); }
    }
    else { inputBuffer += c; redrawInputLine(inputBuffer); }
  }
}

void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") { currentMode = 1; Serial.printf(C_BLUE "\r\n--- [ Mode 1: 수동 ] ---\r\n" C_RESET); updateDisplay(0, "Manual Mode"); }
    else if (input == "2") { currentMode = 2; Serial.printf(C_BLUE "\r\n--- [ Mode 2: 감성 ] ---\r\n" C_RESET); updateDisplay(0, "Emotion Mode"); }
    else if (input == "3") { currentMode = 3; Serial.printf(C_BLUE "\r\n--- [ Mode 3: 날씨 ] ---\r\n" C_RESET); updateDisplay(0, "Weather Mode"); }
    else if (input == "4") { currentMode = 4; Serial.printf(C_YELLOW "\r\n--- [ Setting ] ---\r\n" C_RESET); }
    else if (input == "5") { currentMode = 5; demoStep=0; Serial.printf(C_MAGENTA "\r\n--- [ Demo ] ---\r\n" C_RESET); }
    else if (input == "6") { currentMode = 6; recordAndSendVoice(); currentMode=0; printMainMenu(); } 
    else if (input == "7") { currentMode = 7; Serial.printf(C_MAGENTA "\r\n--- [ 🔊 Sound Visualizer ] ---\r\n" C_RESET); updateDisplay(0, "Visualizer"); } 
    else if (input == "8") { currentMode = 8; Serial.printf(C_MAGENTA "\r\n--- [ 📢 Sound Reactive (Trigger: %ddB) ] ---\r\n" C_RESET, soundThreshold); Serial.println("👉 +/- 키로 감도 조절 가능"); updateDisplay(0, "Reactive Mode"); } 
    else if (input == "9") { printDashboard(); } 
    else { Serial.printf(C_RED "❌ Error\r\n" C_RESET); printMainMenu(); }
    Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); 
  }
  else if (currentMode == 1) { if (input == "+") changeVolume(currentVolume + 2); else if (input == "-") changeVolume(currentVolume - 2); else runManualMode(input); }
  else if (currentMode == 2) { Serial.printf(C_YELLOW "[Emotion] 요청...\r\n" C_RESET); updateDisplay(0, "Analyzing..."); sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}"); }
  else if (currentMode == 3) { lastWeatherRegion = input; float w = scale.get_units(10); Serial.printf(C_YELLOW "[Weather] %s\r\n" C_RESET, input.c_str()); updateDisplay(0, "Checking " + input); sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + input + "\", \"w4\": " + String(w) + "}"); }
}

void printMainMenu() { Serial.println("\r\n[1]수동 [2]감성 [3]날씨 [4]설정 [5]데모 [6]음성 [7]파형 [8]반응"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); }

// (이하 나머지 함수들은 V10.2와 동일하므로 생략하지 않고 모두 포함시켰습니다)
void updateDisplay(int iconID, String text) { myNextionSerial.print("p0.pic="); myNextionSerial.print(iconID); myNextionSerial.write(0xff); myNextionSerial.write(0xff); myNextionSerial.write(0xff); myNextionSerial.print("t0.txt=\""); myNextionSerial.print(text); myNextionSerial.print("\""); myNextionSerial.write(0xff); myNextionSerial.write(0xff); myNextionSerial.write(0xff); }
void bootAnimation() { for(int i=0; i<3; i++) { digitalWrite(PIN_LED, HIGH); delay(100); digitalWrite(PIN_LED, LOW); delay(100); } }
void initMicrophone() { const i2s_config_t i2s_config = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), .sample_rate = SAMPLE_RATE, .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB), .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0 }; const i2s_pin_config_t pin_config = { .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = I2S_SD }; i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL); i2s_set_pin(I2S_PORT, &pin_config); }
int32_t readMicrophone() { int16_t sample = 0; size_t bytes_read = 0; i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0); if (bytes_read > 0) return abs(sample); return 0; }
void redrawInputLine(String &buffer) { Serial.print("\r\033[K"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(buffer); }
void recordAndSendVoice() { if(WiFi.status() != WL_CONNECTED) { Serial.println("WiFi Disconnected"); return; } uint32_t dataSize = SAMPLE_RATE * RECORD_TIME * 2; uint32_t totalSize = sizeof(WavHeader) + dataSize; Serial.printf(C_YELLOW "\r\n[Voice] 🎤 녹음 (2초)...\r\n" C_RESET); uint8_t* audioBuffer = (uint8_t*)malloc(totalSize); if (audioBuffer == NULL) return; WavHeader header; memcpy(header.riff, "RIFF", 4); header.overall_size = totalSize - 8; memcpy(header.wave, "WAVE", 4); memcpy(header.fmt_chunk_marker, "fmt ", 4); header.length_of_fmt = 16; header.format_type = 1; header.channels = 1; header.sample_rate = SAMPLE_RATE; header.byterate = SAMPLE_RATE * 2; header.block_align = 2; header.bits_per_sample = 16; memcpy(header.data_chunk_header, "data", 4); header.data_size = dataSize; memcpy(audioBuffer, &header, sizeof(WavHeader)); size_t bytesRead = 0; i2s_read(I2S_PORT, (void*)(audioBuffer + sizeof(WavHeader)), dataSize, &bytesRead, portMAX_DELAY); long sumVolume = 0; int16_t* samples = (int16_t*)(audioBuffer + sizeof(WavHeader)); for (int i=0; i<dataSize/2; i++) sumVolume += abs(samples[i]); if (sumVolume/(dataSize/2) < 500) { Serial.println(C_RED "→ 🤫 침묵 (취소)" C_RESET); updateDisplay(0, "Too Quiet"); free(audioBuffer); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); return; } Serial.println(C_GREEN "→ 🗣️ 전송 중..." C_RESET); updateDisplay(0, "Thinking..."); esp_task_wdt_delete(NULL); WiFiClientSecure client; client.setInsecure(); HTTPClient http; http.setTimeout(20000); if (http.begin(client, serverName)) { http.addHeader("Content-Type", "audio/wav"); int httpCode = http.POST(audioBuffer, totalSize); if (httpCode > 0) { String res = http.getString(); Serial.printf(C_GREEN "✅ 응답: %s\r\n" C_RESET, res.c_str()); JsonDocument doc; deserializeJson(doc, res); int cmd = doc["spray"]; int dur = doc["duration"]; String txt = doc["result_text"]; if (cmd > 0) { forceAllOff(); if (cmd==1) activePin=PIN_SUNNY; else if (cmd==2) activePin=PIN_CLOUDY; else if (cmd==3) activePin=PIN_RAIN; else if (cmd==4) activePin=PIN_SNOW; isRunning=true; isSpraying=true; sprayDuration=dur*1000; prevMotorMillis=millis(); startTimeMillis=millis(); digitalWrite(activePin, LOW); playSound(cmd); lastWebMessage = "성공: " + txt; updateDisplay(cmd, txt); } else { lastWebMessage = txt; updateDisplay(0, txt); } } else { Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str()); updateDisplay(0, "Network Error"); } http.end(); } else { Serial.println("Conn Fail"); updateDisplay(0, "Conn Fail"); } esp_task_wdt_add(NULL); free(audioBuffer); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); }
void pollServer() { if (isRunning || currentMode == 5) return; unsigned long now = millis(); if (now - lastPollTime >= POLL_INTERVAL) { lastPollTime = now; if(WiFi.status() != WL_CONNECTED) return; WiFiClientSecure client; client.setInsecure(); HTTPClient http; http.setTimeout(3000); if (http.begin(client, serverName)) { http.addHeader("Content-Type", "application/json"); int code = http.POST("{\"action\": \"POLL\", \"deviceId\": \"App_User\"}"); if (code > 0) { String res = http.getString(); JsonDocument doc; deserializeJson(doc, res); int cmd = doc["spray"]; if (cmd > 0) { Serial.printf("\r\n" C_GREEN "📲 [APP] 명령: %d\r\n" C_RESET, cmd); if (cmd == 90) { stopSystem(); currentMode=0; updateDisplay(0, "Reset"); return; } if (cmd == 30) { currentMode=3; if(doc["target_region"]) lastWeatherRegion = String((const char*)doc["target_region"]); lastWeatherCallMillis = millis() - WEATHER_INTERVAL; updateDisplay(0, "Weather Mode"); return; } forceAllOff(); if (cmd == 1) activePin = PIN_SUNNY; else if (cmd == 2) activePin = PIN_CLOUDY; else if (cmd == 3) activePin = PIN_RAIN; else if (cmd == 4) activePin = PIN_SNOW; isRunning = true; isSpraying = true; sprayDuration = 3000; prevMotorMillis = millis(); startTimeMillis = millis(); digitalWrite(activePin, LOW); playSound(cmd); updateDisplay(cmd, "App Control"); lastWebMessage = "앱 제어 중..."; Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); } } http.end(); } } }
void manageWiFi() { static bool wasConnected = true; if (millis() - lastCheckTime >= 1000) { lastCheckTime = millis(); if (WiFi.status() != WL_CONNECTED) { if (wasConnected) { wasConnected = false; Serial.print("\r\033[K"); Serial.printf(C_RED "🚨 WiFi 끊김!\r\n" C_RESET); updateDisplay(0, "WiFi Lost"); WiFi.disconnect(); WiFi.reconnect(); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); } } else { if (!wasConnected) { wasConnected = true; Serial.print("\r\033[K"); Serial.printf(C_GREEN "✅ WiFi 복구!\r\n" C_RESET); updateDisplay(0, "WiFi OK"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); } } } }
void autoWeatherScheduler() { if (currentMode != 3 || isRunning) return; if (millis() - lastWeatherCallMillis >= WEATHER_INTERVAL) { lastWeatherCallMillis = millis(); float w = scale.get_units(10); Serial.print("\r\033[K"); Serial.printf(C_YELLOW "\r\n[AUTO] 날씨 갱신 (%s)\r\n" C_RESET, lastWeatherRegion.c_str()); updateDisplay(0, "Weather Check..."); sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}"); } }
void changeVolume(int vol) { currentVolume = constrain(vol, 0, 30); myDFPlayer.volume(currentVolume); prefs.putInt("volume", currentVolume); Serial.print("\r\033[K"); Serial.printf(C_GREEN "🔊 볼륨: %d\r\n" C_RESET, currentVolume); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); }
void handleWebClient() { WiFiClient client = webServer.available(); if (!client) return; unsigned long startTime = millis(); while (!client.available() && millis() - startTime < 1000) { delay(1); } String request = ""; while (client.connected() && client.available()) { char c = client.read(); request += c; } if (request.length() == 0) { client.stop(); return; } if (request.indexOf("favicon.ico") >= 0) { client.println("HTTP/1.1 404 Not Found\r\nConnection: close\r\n"); client.stop(); return; } if (request.indexOf("GET /RUN_") >= 0 || request.indexOf("GET /STOP") >= 0 || request.indexOf("GET /VOL_") >= 0) { if (request.indexOf("GET /RUN_SUNNY") >= 0) { runManualMode("1"); lastWebMessage = "수동: 맑음"; } if (request.indexOf("GET /RUN_CLOUDY") >= 0) { runManualMode("2"); lastWebMessage = "수동: 흐림"; } if (request.indexOf("GET /RUN_RAIN") >= 0) { runManualMode("3"); lastWebMessage = "수동: 비"; } if (request.indexOf("GET /RUN_SNOW") >= 0) { runManualMode("4"); lastWebMessage = "수동: 눈"; } if (request.indexOf("GET /STOP") >= 0) { stopSystem(); currentMode=0; printMainMenu(); lastWebMessage = "정지"; } client.println("HTTP/1.1 204 No Content\r\nConnection: close\r\n"); } else { client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n"); client.println("<!DOCTYPE html><html><body><h1>Smart Diffuser</h1></body></html>"); } delay(10); client.stop(); }
void sendServerRequest(String payload) { if(WiFi.status() != WL_CONNECTED) { Serial.println("WiFi Err"); return; } WiFiClientSecure client; client.setInsecure(); HTTPClient http; http.setTimeout(10000); if (!http.begin(client, serverName)) return; http.addHeader("Content-Type", "application/json"); int code = http.POST(payload); if(code > 0){ String res = http.getString(); JsonDocument doc; deserializeJson(doc, res); int cmd = doc["spray"]; int dur = doc["duration"]; String txt = doc["result_text"]; Serial.printf("\r\n✅ 응답: %s\r\n", txt.c_str()); if (cmd > 0) { forceAllOff(); if (cmd == 1) activePin = PIN_SUNNY; else if (cmd == 2) activePin = PIN_CLOUDY; else if (cmd == 3) activePin = PIN_RAIN; else if (cmd == 4) activePin = PIN_SNOW; isRunning = true; isSpraying = true; sprayDuration = dur * 1000; prevMotorMillis = millis(); startTimeMillis = millis(); digitalWrite(activePin, LOW); playSound(cmd); lastWebMessage = "성공: " + txt; updateDisplay(cmd, txt); } else { Serial.printf(C_YELLOW "⚠️ 대기: %s\r\n" C_RESET, txt.c_str()); lastWebMessage = txt; stopSystem(); updateDisplay(0, txt); } } http.end(); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); }
void connectWiFi() { Serial.printf(C_YELLOW "[System] WiFi Connecting" C_RESET); WiFi.begin(ssid, password); int retry = 0; while(WiFi.status() != WL_CONNECTED && retry < 15) { delay(200); Serial.print("."); retry++; } if (WiFi.status() == WL_CONNECTED) { Serial.printf("\r\n" C_GREEN "Connected!\r\n" C_RESET); Serial.printf(C_CYAN "🌐 Web: http://%s/\r\n" C_RESET, WiFi.localIP().toString().c_str()); } else { Serial.printf(C_RED "\r\nWiFi Failed.\r\n" C_RESET); } }
void systemHeartbeat() { if(!isRunning && millis()-prevLedMillis>=30){prevLedMillis=millis();ledBrightness+=ledFadeAmount;if(ledBrightness<=0||ledBrightness>=255)ledFadeAmount=-ledFadeAmount;analogWrite(PIN_LED,ledBrightness);} }
void printDashboard() { Serial.println("\r\n--- Dashboard ---"); Serial.printf("WiFi: %d dBm\r\n", WiFi.RSSI()); Serial.printf("Weight: %.2f g\r\n", scale.get_units(10)); Serial.printf("Mic: %d\r\n", readMicrophone()); Serial.printf("Vol: %d\r\n", currentVolume); printMainMenu(); }
void playSound(int trackNum) { myDFPlayer.play(trackNum); }
void forceAllOff() { digitalWrite(PIN_SUNNY, HIGH); digitalWrite(PIN_CLOUDY, HIGH); digitalWrite(PIN_RAIN, HIGH); digitalWrite(PIN_SNOW, HIGH); }
void stopSystem() { forceAllOff(); isRunning = false; activePin = -1; isSpraying = false; myDFPlayer.stop(); updateDisplay(0, "Ready"); }
void runManualMode(String input) { int t=-1; if(input=="1")t=PIN_SUNNY;else if(input=="2")t=PIN_CLOUDY;else if(input=="3")t=PIN_RAIN;else if(input=="4")t=PIN_SNOW; if(t!=-1){forceAllOff();activePin=t;isRunning=true;isSpraying=true;sprayDuration=3000;prevMotorMillis=millis();startTimeMillis=millis();digitalWrite(activePin,LOW);playSound(input.toInt());Serial.printf("\r\n[Manual] %s번\r\n",input.c_str()); updateDisplay(input.toInt(), "Manual Run"); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);}else if(input=="0"){currentMode=0;stopSystem();printMainMenu();} }
void runSprayLogic() { if (activePin == -1) return; if (isSpraying) { if (millis() - prevMotorMillis >= sprayDuration) { digitalWrite(activePin, HIGH); isSpraying = false; prevMotorMillis = millis(); if (currentMode == 1) { stopSystem(); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); return; } Serial.print("\r\033[K"); Serial.printf(C_CYAN "      └── [Idle] 휴식...\r\n" C_RESET); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); updateDisplay(0, "Resting..."); } } else { if (millis() - prevMotorMillis >= REST_TIME) { forceAllOff(); digitalWrite(activePin, LOW); isSpraying = true; prevMotorMillis = millis(); Serial.print("\r\033[K"); Serial.printf(C_GREEN "      ┌── [Action] 재분사!\r\n" C_RESET); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); int icon = 0; if (activePin == PIN_SUNNY) icon=1; else if (activePin == PIN_CLOUDY) icon=2; else if (activePin == PIN_RAIN) icon=3; else if (activePin == PIN_SNOW) icon=4; updateDisplay(icon, "Spraying..."); } } }
void runAutoDemoLoop() { if (millis() - prevDemoMillis >= 4000) { prevDemoMillis = millis(); forceAllOff(); demoStep++; if (demoStep > 4) demoStep = 1; int t = -1; if (demoStep == 1) t = PIN_SUNNY; else if (demoStep == 2) t = PIN_CLOUDY; else if (demoStep == 3) t = PIN_RAIN; else if (demoStep == 4) t = PIN_SNOW; Serial.print("\r\033[K"); Serial.printf(C_MAGENTA "[Auto Demo] %s 모드\r\n" C_RESET, (demoStep==1)?"맑음":(demoStep==2)?"흐림":(demoStep==3)?"비":"눈"); digitalWrite(t, LOW); playSound(demoStep); updateDisplay(demoStep, "Demo Mode"); } }
void checkSafety() { if (millis() - startTimeMillis > MAX_RUN_TIME) { Serial.print("\r\033[K"); Serial.printf(C_RED "\r\n🚨 [Emergency] 안전 타이머 작동!\r\n" C_RESET); stopSystem(); updateDisplay(0, "Error: Timeout"); currentMode = 0; printMainMenu(); Serial.print(inputBuffer); } }
void printCalibrationInfo() { Serial.printf("📡 보정값: %.1f | 현재 무게: %.2f g\r\n", calibration_factor, scale.get_units(5)); }
void monitorWeight() { if (millis() - lastWeightCheckTime > 500) { lastWeightCheckTime = millis(); float currentWeight = scale.get_units(2); if (abs(currentWeight - lastWeight) > WEIGHT_THRESHOLD) { Serial.print("\r\033[K"); Serial.printf(C_MAGENTA "⚖️ [Weight] %.1fg -> %.1fg\r\n" C_RESET, lastWeight, currentWeight); Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET); Serial.print(inputBuffer); lastWeight = currentWeight; } } }
