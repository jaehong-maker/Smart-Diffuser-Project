/*
 * [프로젝트] 스마트 디퓨저 (Smart Diffuser)
 * [버   전] V10.9.1 Echo Fix (Final Polish) + Pin Map Update
 * [수   정] 기존 로직 유지 + 로드셀 4개 및 핀 재설정 적용
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
#include <math.h>

// ============================================================
// [0] 핀 정의 (재홍님 요청사항 100% 반영)
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

const char* ssid     = "Jaehong_WiFi";
const char* password = "12345678";
String serverName    = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

// 1. 모터 & LED (릴레이 모듈 IN 1, 2, 3, 4)
const int PIN_SUNNY  = 4;    // IO4
const int PIN_CLOUDY = 13;   // IO13 (재홍님 표: 13)
const int PIN_RAIN   = 14;   // IO14
const int PIN_SNOW   = 27;   // IO27 (재홍님 표: 27)
const int PIN_LED    = 2;

// 2. 로드셀 4개 (DT는 입력 전용 핀 활용)
// 로드셀 1 (DT: 34 / SCK: 33)
const int LC1_DT  = 34;
const int LC1_SCK = 33;
// 로드셀 2 (DT: 36 / SCK: 18)
const int LC2_DT  = 36;
const int LC2_SCK = 18;
// 로드셀 3 (DT: 39 / SCK: 21)
const int LC3_DT  = 39;
const int LC3_SCK = 21;
// 로드셀 4 (DT: 32 / SCK: 23)
const int LC4_DT  = 32;
const int LC4_SCK = 23;

// 3. DFPlayer (스피커: DAC 전용 소리 출력)
const int DFPLAYER_RX_PIN = 25;  // IO25
const int DFPLAYER_TX_PIN = 26;  // IO26

// 4. Nextion (넥션 디스플레이: UART2 전용석)
const int NEXTION_TX_PIN = 16;  // IO16
const int NEXTION_RX_PIN = 17;  // IO17

// 5. INMP441 마이크 (I2S 마이크: SD 35 / WS 19 / SCK 22)
#define I2S_WS   19
#define I2S_SD   35
#define I2S_SCK  22
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 16000
#define RECORD_TIME 2

// 전역 객체
HardwareSerial mySoftwareSerial(2);
HardwareSerial nexSerial(1);
DFRobotDFPlayerMini myDFPlayer;

// ★ 로드셀 4개 객체 선언 (기존 scale 하나에서 4개로 변경)
HX711 scale1, scale2, scale3, scale4;

Preferences prefs;
WiFiServer webServer(80);

// 변수들
unsigned long sprayDuration = 3000;
const long REST_TIME        = 5000;
const long MAX_RUN_TIME     = 270000;

float calibration_factor = 430.0;

int currentVolume = 20;
int currentMode   = 0;
bool isRunning    = false;
int activePin     = -1;
bool isSpraying   = false;

String lastWebMessage = "Ready";
String inputBuffer    = "";

unsigned long lastPollTime = 0;
const unsigned long POLL_INTERVAL = 2000;

// ★ 로드셀 4개 무게 저장용 배열
float weights[4] = {0.0, 0.0, 0.0, 0.0};

unsigned long lastWeightCheckTime = 0;
const float WEIGHT_THRESHOLD = 5.0;

int soundThreshold = 60;
unsigned long lastReactionTime = 0;

int demoStep = 0;
unsigned long prevDemoMillis  = 0;
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
  char riff[4];
  uint32_t overall_size;
  char wave[4];

  char fmt_chunk_marker[4];
  uint32_t length_of_fmt;
  uint16_t format_type;
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t byterate;
  uint16_t block_align;
  uint16_t bits_per_sample;

  char data_chunk_header[4];
  uint32_t data_size;
};

// 함수 원형 (기존 그대로)
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

void runSoundVisualizer();
void runSoundReaction();

void checkNextionInput();
void handleNextionCmd(const String &cmd);
void enterWeatherMode(bool runNow);
void showPrompt();

void nexSend(const String &cmd);
void nexSetText(const char* obj, String text);
void nexSetVal(const char* obj, int v);
void nexSetPic(const char* obj, int picId);

// ============================================================
// [1] 초기화 (Setup)
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5000);

  // Nextion 시작
  nexSerial.begin(9600, SERIAL_8N1, NEXTION_RX_PIN, NEXTION_TX_PIN);

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
  Serial.printf(C_BOLD    " 🚀 SMART DIFFUSER V10.9.1 (Pin Fixed) \r\n" C_RESET);
  Serial.printf(C_MAGENTA "****************************************\r\n" C_RESET);

  mySoftwareSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (!myDFPlayer.begin(mySoftwareSerial)) Serial.println("Audio Fail");
  else Serial.println("Audio OK");

  updateDisplay(0, "System Ready");
  initMicrophone();

  prefs.begin("diffuser", false);
  float savedFactor = prefs.getFloat("cal_factor", 0.0);
  if (savedFactor != 0.0) calibration_factor = savedFactor;

  currentVolume = prefs.getInt("volume", 20);
  myDFPlayer.volume(currentVolume);

  // ★ 로드셀 4개 초기화 (기존 scale.begin -> 4개로 확장)
  scale1.begin(LC1_DT, LC1_SCK);
  scale2.begin(LC2_DT, LC2_SCK);
  scale3.begin(LC3_DT, LC3_SCK);
  scale4.begin(LC4_DT, LC4_SCK);

  scale1.set_scale(calibration_factor); scale1.tare();
  scale2.set_scale(calibration_factor); scale2.tare();
  scale3.set_scale(calibration_factor); scale3.tare();
  scale4.set_scale(calibration_factor); scale4.tare();

  // 초기 무게값 읽기
  weights[0] = scale1.get_units(5);

  connectWiFi();
  webServer.begin();
  bootAnimation();
  printMainMenu();
}

// ============================================================
// ★ [FIX] 메아리(Echo) 무시 & 명령어 수신
// ============================================================
void checkNextionInput() {
  static String buf = "";
  static unsigned long lastRxTime = 0;

  while (nexSerial.available()) {
    char c = (char)nexSerial.read();

    // 노이즈 필터: 알파벳, 숫자, 밑줄, 공백만 허용
    if (isalnum(c) || c == '_' || c == ' ') {
      buf += c;
      if (buf.length() > 64) buf = "";
    }
    lastRxTime = millis();
  }

  // 타임아웃(0.05초) 후 명령어 처리
  if (buf.length() > 0 && (millis() - lastRxTime > 50)) {
    buf.trim();
    if (buf.length() > 0) {
      handleNextionCmd(buf);
    }
    buf = "";
  }
}

// ★ [FIX] 명령어 처리 (메아리 차단 로직 추가)
void handleNextionCmd(const String &cmd) {
  // 1. 메아리(Echo) 차단: p0..., t0..., j0... 로 시작하면 무시
  if (cmd.startsWith("p0") || cmd.startsWith("t0") || cmd.startsWith("j0")) {
    return; // 조용히 무시
  }

  // 2. 유효 명령어인지 먼저 확인 (디버깅용 출력 최소화)
  bool isCommand = false;
  if (cmd == "M1" || cmd == "M2" || cmd == "M3" || cmd.startsWith("S")) isCommand = true;
  if (cmd == "Y1" || cmd == "Y2" || cmd == "Y3") isCommand = true;

  if (isCommand) {
    Serial.print("\r\033[K");
    Serial.printf(C_CYAN "[NX] 터치: %s\r\n" C_RESET, cmd.c_str());
  }

  // 3. 명령어 실행
  if (cmd == "M2" || cmd == "Y3") {
    enterWeatherMode(false);
  }
  else if (cmd == "M1" || cmd == "Y1") {
    currentMode = 1;
    Serial.println(C_BLUE "--- [ Mode 1: 수동 ] ---" C_RESET);
    showPrompt();
  }
  else if (cmd == "M3" || cmd == "Y2") {
    currentMode = 2;
    Serial.println(C_BLUE "--- [ Mode 2: 감성 ] ---" C_RESET);
    showPrompt();
  }
  else if (cmd.startsWith("S")) {
    int scent = cmd.substring(1).toInt();
    if (scent >= 1 && scent <= 4) runManualMode(String(scent));
  }
  // 그 외 잡음은 무시
}

void nexSend(const String &cmd) {
  nexSerial.print(cmd);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
  nexSerial.write(0xFF);
}

void nexSetText(const char* obj, String text) {
  text.replace("\"", "'");
  nexSend(String(obj) + ".txt=\"" + text + "\"");
}

void nexSetVal(const char* obj, int v) {
  nexSend(String(obj) + ".val=" + String(v));
}

void nexSetPic(const char* obj, int picId) {
  nexSend(String(obj) + ".pic=" + String(picId));
}

void updateDisplay(int iconID, String text) {
  nexSetPic("p0", iconID);
  nexSetText("t0", text);
}

void updateProgressBar(int val) {
  nexSetVal("j0", constrain(val, 0, 100));
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  esp_task_wdt_reset();
  checkNextionInput();

  if (currentMode == 7) {
    runSoundVisualizer();
    checkSerialInput();
    return;
  }
  if (currentMode == 8) {
    runSoundReaction();
    checkSerialInput();
    return;
  }

  manageWiFi();
  systemHeartbeat();
  handleWebClient();
  autoWeatherScheduler();
  pollServer();
  monitorWeight();

  if (currentMode == 5) runAutoDemoLoop();
  else if (isRunning) {
    runSprayLogic();
    checkSafety();
  }

  checkSerialInput();
}

// ============================================================
// 기능 함수들
// ============================================================
void checkSerialInput() {
  if (currentMode == 8) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') return;

      if (c == '+') {
        soundThreshold += 2;
        Serial.printf("\r\n🆙 감도: %d dB\r\n", soundThreshold);
      }
      else if (c == '-') {
        soundThreshold -= 2;
        Serial.printf("\r\n⬇️ 감도: %d dB\r\n", soundThreshold);
      }
      else if (c == '0') {
        currentMode = 0;
        stopSystem();
        printMainMenu();
      }
      showPrompt();
    }
    return;
  }

  if (currentMode == 4) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') return;

      if (c == '+') {
        calibration_factor += 10;
        scale1.set_scale(calibration_factor);
        printCalibrationInfo();
      }
      else if (c == '-') {
        calibration_factor -= 10;
        scale1.set_scale(calibration_factor);
        printCalibrationInfo();
      }
      else if (c == 't') {
        scale1.tare();
        Serial.println("영점");
        printCalibrationInfo();
      }
      else if (c == 's') {
        prefs.putFloat("cal_factor", calibration_factor);
        Serial.println("저장");
      }
      else if (c == '0') {
        currentMode = 0;
        printMainMenu();
      }
    }
    return;
  }

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        Serial.println();
        if (inputBuffer == "0") {
          stopSystem();
          currentMode = 0;
          printMainMenu();
        } else {
          handleInput(inputBuffer);
        }
        inputBuffer = "";
      }
    }
    else if (c == '\b' || c == 0x7F) {
      if (inputBuffer.length() > 0) {
        inputBuffer.remove(inputBuffer.length() - 1);
        redrawInputLine(inputBuffer);
      }
    }
    else {
      inputBuffer += c;
      redrawInputLine(inputBuffer);
    }
  }
}

void handleInput(String input) {
  if (currentMode == 0) {
    if (input == "1") {
      currentMode = 1;
      Serial.println(C_BLUE "\r\n--- [ Mode 1: 수동 ] ---" C_RESET);
      updateDisplay(0, "Manual Mode");
      showPrompt();
    }
    else if (input == "2") {
      currentMode = 2;
      Serial.println(C_BLUE "\r\n--- [ Mode 2: 감성 ] ---" C_RESET);
      updateDisplay(0, "Emotion Mode");
      showPrompt();
    }
    else if (input == "3") {
      currentMode = 3;
      Serial.println(C_BLUE "\r\n--- [ Mode 3: 날씨 ] ---" C_RESET);
      updateDisplay(0, "Weather Mode");
      showPrompt();
    }
    else if (input == "4") {
      currentMode = 4;
      Serial.println(C_YELLOW "\r\n--- [ Setting ] ---" C_RESET);
    }
    else if (input == "5") {
      currentMode = 5;
      demoStep = 0;
      Serial.println(C_MAGENTA "\r\n--- [ Demo ] ---" C_RESET);
    }
    else if (input == "6") {
      currentMode = 6;
      recordAndSendVoice();
      currentMode = 0;
      printMainMenu();
    }
    else if (input == "7") {
      currentMode = 7;
      Serial.println(C_MAGENTA "\r\n--- [ Sound Visualizer ] ---" C_RESET);
      updateDisplay(0, "Visualizer");
    }
    else if (input == "8") {
      currentMode = 8;
      Serial.printf(C_MAGENTA "\r\n--- [ Sound Reactive (%ddB) ] ---\r\n" C_RESET, soundThreshold);
      updateDisplay(0, "Reactive");
    }
    else if (input == "9") {
      printDashboard();
    }
    else {
      Serial.println(C_RED "Error" C_RESET);
      printMainMenu();
    }
  }
  else if (currentMode == 1) {
    if (input == "+") changeVolume(currentVolume + 2);
    else if (input == "-") changeVolume(currentVolume - 2);
    else runManualMode(input);
  }
  else if (currentMode == 2) {
    Serial.println("감성 분석 중...");
    updateDisplay(0, "Analyzing...");
    sendServerRequest("{\"mode\": \"emotion\", \"user_emotion\": \"" + input + "\"}");
  }
  else if (currentMode == 3) {
    lastWeatherRegion = input;
    float w = scale1.get_units(10);
    Serial.printf("날씨 조회: %s\r\n", input.c_str());
    updateDisplay(0, "Checking " + input);
    sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + input + "\", \"w4\": " + String(w) + "}");
  }
}

void runManualMode(String input) {
  int t = -1;

  if (input == "1") t = PIN_SUNNY;
  else if (input == "2") t = PIN_CLOUDY;
  else if (input == "3") t = PIN_RAIN;
  else if (input == "4") t = PIN_SNOW;

  if (t != -1) {
    forceAllOff();
    activePin = t;
    isRunning = true;
    isSpraying = true;

    sprayDuration = 3000;
    prevMotorMillis = millis();
    startTimeMillis = millis();

    digitalWrite(activePin, LOW);
    playSound(input.toInt());

    Serial.printf("\r\n[Manual] %s번 실행\r\n", input.c_str());
    updateDisplay(input.toInt(), "Manual Run");
    showPrompt();
  }
  else if (input == "0") {
    currentMode = 0;
    stopSystem();
    printMainMenu();
  }
  else {
    showPrompt();
  }
}

void enterWeatherMode(bool runNow) {
  stopSystem();
  currentMode = 3;
  lastWebMessage = "Nextion: 날씨";

  Serial.print("\r\033[K");
  Serial.println(C_BLUE "\r\n--- [ Mode 3: 날씨 모드 ] ---" C_RESET);

  // lastWeatherRegion 대신 안내 문구 출력
  Serial.println(C_YELLOW "👉 날씨를 확인할 지역 이름을 입력하세요 (예: 부산)" C_RESET);
  showPrompt();

  if (runNow) {
    float w = scale1.get_units(10);
    sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}");
    lastWeatherCallMillis = millis();
  } else {
    lastWeatherCallMillis = 0;
  }
}

void runSoundReaction() {
  int32_t samples[64];
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, &samples, sizeof(samples), &bytes_read, 0);
  if (bytes_read > 0) {
    double sumSquares = 0;
    int numSamples = bytes_read / 4;

    for (int i = 0; i < numSamples; i++) {
      int16_t wave = (samples[i] >> 14);
      sumSquares += (double)wave * wave;
    }

    double rms = sqrt(sumSquares / numSamples);
    if (rms < 1) rms = 1;

    int db = (int)(20 * log10(rms));

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
      lastPrint = millis();
      Serial.printf("\r\033[K📢 Noise: %d dB (Target: >%d)\r", db, soundThreshold);
      updateDisplay(0, String(db) + " dB / " + String(soundThreshold));
    }

    if (db >= soundThreshold) {
      if (millis() - lastReactionTime > 5000) {
        lastReactionTime = millis();
        Serial.printf("\n\n🎉 [Sound Trigger] %d dB! 자동 분사!\n", db);
        updateDisplay(0, "LOUD NOISE!");

        int cmd = random(1, 5);
        forceAllOff();

        if (cmd == 1) activePin = PIN_SUNNY;
        else if (cmd == 2) activePin = PIN_CLOUDY;
        else if (cmd == 3) activePin = PIN_RAIN;
        else if (cmd == 4) activePin = PIN_SNOW;

        isRunning = true;
        isSpraying = true;
        sprayDuration = 3000;
        prevMotorMillis = millis();
        startTimeMillis = millis();

        digitalWrite(activePin, LOW);
        playSound(cmd);

        updateDisplay(cmd, "Sound Active");
      }
    }
  }
}

void runSoundVisualizer() {
  int32_t sample = 0;
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0);
  if (bytes_read > 0) {
    int16_t wave = (sample >> 14);
    int vol = abs(wave);

    static unsigned long lastVisUpdate = 0;
    if (millis() - lastVisUpdate > 100) {
      lastVisUpdate = millis();

      if (vol < 100) vol = 0;

      int bars = map(vol, 0, 5000, 0, 30);
      bars = constrain(bars, 0, 30);

      Serial.print("\r\033[K");
      Serial.print(C_CYAN "🎤 Sound: " C_RESET);
      Serial.print(C_GREEN);

      for (int i = 0; i < bars; i++) Serial.print("█");
      for (int i = bars; i < 30; i++) Serial.print(" ");

      Serial.print(C_RESET);
      Serial.printf(" [%d]", vol);

      updateProgressBar(map(vol, 0, 5000, 0, 100));
    }
  }
}

// 유틸리티
void showPrompt() {
  Serial.print("\r\033[K");
  Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);
}

void printMainMenu() {
  Serial.println("\r\n[1]수동 [2]감성 [3]날씨 [4]설정 [5]데모 [6]음성 [7]파형 [8]반응");
  showPrompt();
}

void bootAnimation() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED, HIGH); delay(100);
    digitalWrite(PIN_LED, LOW);  delay(100);
  }
}

void initMicrophone() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

int32_t readMicrophone() {
  int16_t sample = 0;
  size_t bytes_read = 0;

  i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, 0);
  if (bytes_read > 0) return abs(sample);
  return 0;
}

void redrawInputLine(String &buffer) {
  Serial.print("\r\033[K");
  Serial.print(C_YELLOW "👉 명령 입력 >>" C_RESET);
  Serial.print(buffer);
}

// ------------------------------------------------------------------
// 아래는 네가 준 원문 그대로 기능 유지. (가독성만 줄바꿈/들여쓰기)
// ------------------------------------------------------------------

void recordAndSendVoice() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    return;
  }

  uint32_t dataSize  = SAMPLE_RATE * RECORD_TIME * 2;
  uint32_t totalSize = sizeof(WavHeader) + dataSize;

  Serial.printf(C_YELLOW "\r\n[Voice] 🎤 녹음 (2초)...\r\n" C_RESET);

  uint8_t* audioBuffer = (uint8_t*)malloc(totalSize);
  if (audioBuffer == NULL) return;

  WavHeader header;
  memcpy(header.riff, "RIFF", 4);
  header.overall_size = totalSize - 8;
  memcpy(header.wave, "WAVE", 4);
  memcpy(header.fmt_chunk_marker, "fmt ", 4);
  header.length_of_fmt = 16;
  header.format_type = 1;
  header.channels = 1;
  header.sample_rate = SAMPLE_RATE;
  header.byterate = SAMPLE_RATE * 2;
  header.block_align = 2;
  header.bits_per_sample = 16;
  memcpy(header.data_chunk_header, "data", 4);
  header.data_size = dataSize;

  memcpy(audioBuffer, &header, sizeof(WavHeader));

  size_t bytesRead = 0;
  i2s_read(I2S_PORT,
           (void*)(audioBuffer + sizeof(WavHeader)),
           dataSize,
           &bytesRead,
           portMAX_DELAY);

  long sumVolume = 0;
  int16_t* samples = (int16_t*)(audioBuffer + sizeof(WavHeader));
  for (int i = 0; i < (int)(dataSize / 2); i++) sumVolume += abs(samples[i]);

  if (sumVolume / (dataSize / 2) < 500) {
    Serial.println(C_RED "→ 🤫 침묵 (취소)" C_RESET);
    updateDisplay(0, "Too Quiet");
    free(audioBuffer);
    showPrompt();
    return;
  }

  Serial.println(C_GREEN "→ 🗣️ 전송 중..." C_RESET);
  updateDisplay(0, "Thinking...");

  esp_task_wdt_delete(NULL);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(20000);

  if (http.begin(client, serverName)) {
    http.addHeader("Content-Type", "audio/wav");
    int httpCode = http.POST(audioBuffer, totalSize);

    if (httpCode > 0) {
      String res = http.getString();
      Serial.printf(C_GREEN "✅ 응답: %s\r\n" C_RESET, res.c_str());

      JsonDocument doc;
      deserializeJson(doc, res);

      int cmd = doc["spray"];
      int dur = doc["duration"];
      String txt = doc["result_text"];

      if (cmd > 0) {
        forceAllOff();
        if (cmd == 1) activePin = PIN_SUNNY;
        else if (cmd == 2) activePin = PIN_CLOUDY;
        else if (cmd == 3) activePin = PIN_RAIN;
        else if (cmd == 4) activePin = PIN_SNOW;

        isRunning = true;
        isSpraying = true;
        sprayDuration = dur * 1000;
        prevMotorMillis = millis();
        startTimeMillis = millis();

        digitalWrite(activePin, LOW);
        playSound(cmd);

        lastWebMessage = "성공: " + txt;
        updateDisplay(cmd, txt);
      } else {
        lastWebMessage = txt;
        updateDisplay(0, txt);
      }
    } else {
      Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
      updateDisplay(0, "Network Error");
    }
    http.end();
  } else {
    Serial.println("Conn Fail");
    updateDisplay(0, "Conn Fail");
  }

  esp_task_wdt_add(NULL);
  free(audioBuffer);
  showPrompt();
}

void pollServer() {
  if (isRunning || currentMode == 5) return;

  unsigned long now = millis();
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;

    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(3000);

    if (http.begin(client, serverName)) {
      http.addHeader("Content-Type", "application/json");
      int code = http.POST("{\"action\": \"POLL\", \"deviceId\": \"App_User\"}");

      if (code > 0) {
        String res = http.getString();

        JsonDocument doc;
        deserializeJson(doc, res);

        int cmd = doc["spray"];
        if (cmd > 0) {
          Serial.printf("\r\n" C_GREEN "📲 [APP] 명령: %d\r\n" C_RESET, cmd);

          if (cmd == 90) {
            stopSystem();
            currentMode = 0;
            updateDisplay(0, "Reset");
            return;
          }
          if (cmd == 30) {
            currentMode = 3;
            if (doc["target_region"])
              lastWeatherRegion = String((const char*)doc["target_region"]);
            lastWeatherCallMillis = millis() - WEATHER_INTERVAL;
            updateDisplay(0, "Weather Mode");
            return;
          }

          forceAllOff();

          if (cmd == 1) activePin = PIN_SUNNY;
          else if (cmd == 2) activePin = PIN_CLOUDY;
          else if (cmd == 3) activePin = PIN_RAIN;
          else if (cmd == 4) activePin = PIN_SNOW;

          isRunning = true;
          isSpraying = true;
          sprayDuration = 3000;
          prevMotorMillis = millis();
          startTimeMillis = millis();

          digitalWrite(activePin, LOW);
          playSound(cmd);

          updateDisplay(cmd, "App Control");
          lastWebMessage = "앱 제어 중...";

          showPrompt();
          Serial.print(inputBuffer);
        }
      }
      http.end();
    }
  }
}

void manageWiFi() {
  static bool wasConnected = true;

  if (millis() - lastCheckTime >= 1000) {
    lastCheckTime = millis();

    if (WiFi.status() != WL_CONNECTED) {
      if (wasConnected) {
        wasConnected = false;
        Serial.print("\r\033[K");
        Serial.printf(C_RED "🚨 WiFi 끊김!\r\n" C_RESET);

        updateDisplay(0, "WiFi Lost");
        WiFi.disconnect();
        WiFi.reconnect();

        showPrompt();
        Serial.print(inputBuffer);
      }
    } else {
      if (!wasConnected) {
        wasConnected = true;
        Serial.print("\r\033[K");
        Serial.printf(C_GREEN "✅ WiFi 복구!\r\n" C_RESET);

        updateDisplay(0, "WiFi OK");

        showPrompt();
        Serial.print(inputBuffer);
      }
    }
  }
}

void autoWeatherScheduler() {
  if (currentMode != 3 || isRunning) return;

  if (millis() - lastWeatherCallMillis >= WEATHER_INTERVAL) {
    lastWeatherCallMillis = millis();

    float w = scale1.get_units(10);

    Serial.print("\r\033[K");
    Serial.printf(C_YELLOW "\r\n[AUTO] 날씨 갱신 (%s)\r\n" C_RESET, lastWeatherRegion.c_str());

    updateDisplay(0, "Weather Check...");
    sendServerRequest("{\"mode\": \"weather\", \"region\": \"" + lastWeatherRegion + "\", \"w4\": " + String(w) + "}");
  }
}

void changeVolume(int vol) {
  currentVolume = constrain(vol, 0, 30);
  myDFPlayer.volume(currentVolume);
  prefs.putInt("volume", currentVolume);

  Serial.print("\r\033[K");
  Serial.printf(C_GREEN "🔊 볼륨: %d\r\n" C_RESET, currentVolume);
  showPrompt();
}

void handleWebClient() {
  WiFiClient client = webServer.available();
  if (!client) return;

  unsigned long startTime = millis();
  while (!client.available() && millis() - startTime < 1000) {
    delay(1);
  }

  String request = "";
  while (client.connected() && client.available()) {
    char c = client.read();
    request += c;
  }

  if (request.length() == 0) {
    client.stop();
    return;
  }

  if (request.indexOf("favicon.ico") >= 0) {
    client.println("HTTP/1.1 404 Not Found\r\nConnection: close\r\n");
    client.stop();
    return;
  }

  if (request.indexOf("GET /RUN_") >= 0 ||
      request.indexOf("GET /STOP") >= 0 ||
      request.indexOf("GET /VOL_") >= 0) {

    if (request.indexOf("GET /RUN_SUNNY") >= 0) {
      runManualMode("1");
      lastWebMessage = "수동: 맑음";
    }
    if (request.indexOf("GET /RUN_CLOUDY") >= 0) {
      runManualMode("2");
      lastWebMessage = "수동: 흐림";
    }
    if (request.indexOf("GET /RUN_RAIN") >= 0) {
      runManualMode("3");
      lastWebMessage = "수동: 비";
    }
    if (request.indexOf("GET /RUN_SNOW") >= 0) {
      runManualMode("4");
      lastWebMessage = "수동: 눈";
    }
    if (request.indexOf("GET /STOP") >= 0) {
      stopSystem();
      currentMode = 0;
      printMainMenu();
      lastWebMessage = "정지";
    }

    client.println("HTTP/1.1 204 No Content\r\nConnection: close\r\n");
  } else {
    client.println("HTTP/1.1 200 OK\r\nContent-type:text/html\r\nConnection: close\r\n");
    client.println("<!DOCTYPE html><html><body><h1>Smart Diffuser</h1></body></html>");
  }

  delay(10);
  client.stop();
}

void sendServerRequest(String payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Err");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);

  if (!http.begin(client, serverName)) return;

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);

  if (code > 0) {
    String res = http.getString();

    JsonDocument doc;
    deserializeJson(doc, res);

    int cmd = doc["spray"];
    int dur = doc["duration"];
    String txt = doc["result_text"];

    Serial.printf("\r\n✅ 응답: %s\r\n", txt.c_str());

    if (cmd > 0) {
      forceAllOff();

      if (cmd == 1) activePin = PIN_SUNNY;
      else if (cmd == 2) activePin = PIN_CLOUDY;
      else if (cmd == 3) activePin = PIN_RAIN;
      else if (cmd == 4) activePin = PIN_SNOW;

      isRunning = true;
      isSpraying = true;
      sprayDuration = dur * 1000;
      prevMotorMillis = millis();
      startTimeMillis = millis();

      digitalWrite(activePin, LOW);
      playSound(cmd);

      lastWebMessage = "성공: " + txt;
      updateDisplay(cmd, txt);
    } else {
      Serial.printf(C_YELLOW "⚠️ 대기: %s\r\n" C_RESET, txt.c_str());
      lastWebMessage = txt;
      stopSystem();
      updateDisplay(0, txt);
    }
  }

  http.end();
  showPrompt();
  Serial.print(inputBuffer);
}

void connectWiFi() {
  Serial.printf(C_YELLOW "[System] WiFi Connecting" C_RESET);

  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(200);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\r\n" C_GREEN "Connected!\r\n" C_RESET);
    Serial.printf(C_CYAN "🌐 Web: http://%s/\r\n" C_RESET, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf(C_RED "\r\nWiFi Failed.\r\n" C_RESET);
  }
}

void systemHeartbeat() {
  if (!isRunning && millis() - prevLedMillis >= 30) {
    prevLedMillis = millis();
    ledBrightness += ledFadeAmount;

    if (ledBrightness <= 0 || ledBrightness >= 255)
      ledFadeAmount = -ledFadeAmount;

    analogWrite(PIN_LED, ledBrightness);
  }
}

void printDashboard() {
  Serial.println("\r\n--- Dashboard ---");
  Serial.printf("WiFi: %d dBm\r\n", WiFi.RSSI());
  // ★ 4개 로드셀 무게 출력
  Serial.printf("Weights: W1:%.1f / W2:%.1f / W3:%.1f / W4:%.1f\n",
                weights[0], weights[1], weights[2], weights[3]);
  Serial.printf("Mic: %d\r\n", readMicrophone());
  Serial.printf("Vol: %d\r\n", currentVolume);
  printMainMenu();
}

void playSound(int trackNum) {
  myDFPlayer.play(trackNum);
}

void forceAllOff() {
  digitalWrite(PIN_SUNNY, HIGH);
  digitalWrite(PIN_CLOUDY, HIGH);
  digitalWrite(PIN_RAIN, HIGH);
  digitalWrite(PIN_SNOW, HIGH);
}

void stopSystem() {
  forceAllOff();
  isRunning = false;
  activePin = -1;
  isSpraying = false;
  myDFPlayer.stop();
  updateDisplay(0, "Ready");
}

void runSprayLogic() {
  if (activePin == -1) return;

  if (isSpraying) {
    if (millis() - prevMotorMillis >= sprayDuration) {
      digitalWrite(activePin, HIGH);
      isSpraying = false;
      prevMotorMillis = millis();

      if (currentMode == 1) {
        stopSystem();
        showPrompt();
        return;
      }

      Serial.print("\r\033[K");
      Serial.printf(C_CYAN "      └── [Idle] 휴식...\r\n" C_RESET);
      showPrompt();
      Serial.print(inputBuffer);
      updateDisplay(0, "Resting...");
    }
  } else {
    if (millis() - prevMotorMillis >= REST_TIME) {
      forceAllOff();
      digitalWrite(activePin, LOW);

      isSpraying = true;
      prevMotorMillis = millis();

      Serial.print("\r\033[K");
      Serial.printf(C_GREEN "      ┌── [Action] 재분사!\r\n" C_RESET);

      showPrompt();
      Serial.print(inputBuffer);

      int icon = 0;
      if (activePin == PIN_SUNNY) icon = 1;
      else if (activePin == PIN_CLOUDY) icon = 2;
      else if (activePin == PIN_RAIN) icon = 3;
      else if (activePin == PIN_SNOW) icon = 4;

      updateDisplay(icon, "Spraying...");
    }
  }
}

void runAutoDemoLoop() {
  if (millis() - prevDemoMillis >= 4000) {
    prevDemoMillis = millis();

    forceAllOff();
    demoStep++;
    if (demoStep > 4) demoStep = 1;

    int t = -1;
    if (demoStep == 1) t = PIN_SUNNY;
    else if (demoStep == 2) t = PIN_CLOUDY;
    else if (demoStep == 3) t = PIN_RAIN;
    else if (demoStep == 4) t = PIN_SNOW;

    Serial.print("\r\033[K");
    Serial.printf(C_MAGENTA "[Auto Demo] %s 모드\r\n" C_RESET,
                  (demoStep == 1) ? "맑음" : (demoStep == 2) ? "흐림" : (demoStep == 3) ? "비" : "눈");

    digitalWrite(t, LOW);
    playSound(demoStep);
    updateDisplay(demoStep, "Demo Mode");
  }
}

void checkSafety() {
  if (millis() - startTimeMillis > MAX_RUN_TIME) {
    Serial.print("\r\033[K");
    Serial.printf(C_RED "\r\n🚨 [Emergency] 안전 타이머 작동!\r\n" C_RESET);

    stopSystem();
    updateDisplay(0, "Error: Timeout");

    currentMode = 0;
    printMainMenu();
    Serial.print(inputBuffer);
  }
}

void printCalibrationInfo() {
  Serial.printf("📡 보정값: %.1f | 현재 무게: %.2f g\r\n",
                calibration_factor,
                scale1.get_units(5));
}

// ★ 로드셀 4개 모니터링 함수 수정
void monitorWeight() {
  if (millis() - lastWeightCheckTime > 500) {
    lastWeightCheckTime = millis();

    weights[0] = scale1.get_units(2);
    weights[1] = scale2.get_units(2);
    weights[2] = scale3.get_units(2);
    weights[3] = scale4.get_units(2);
    // 필요 시 여기서 자동 감지 로직 추가 가능
  }
}
