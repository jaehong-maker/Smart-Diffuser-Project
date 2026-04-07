# 스마트 디퓨저 (Smart Diffuser) - Firmware

[cite_start]본 리포지토리는 'I2S 음향 분석 및 날씨 데이터 연동형 AI 스마트 디퓨저'의 ESP32 펌웨어 소스 코드입니다. [cite: 1]
모듈화된 C++ 구조를 채택하여 유지보수성을 높였으며, AWS Lambda 서버와 실시간 통신하며 동작합니다.

## 파일 구조 (File Structure)

코드는 기능별로 분리되어 관리됩니다.

* [cite_start]`SmartDiffuser.ino` : 시스템 초기화 및 메인 루프 실행 [cite: 1]
* [cite_start]`Globals.h` / `Globals.cpp` : 전역 변수, 핀(Pin) 맵, 객체 및 함수 선언 [cite: 1]
* [cite_start]`Hardware.cpp` : 부품 제어 (DFPlayer, I2S 마이크, HX711 로드셀 4개, Nextion 디스플레이) [cite: 1]
* [cite_start]`SystemLogic.cpp` : 사용자 입력 처리, 상태 머신(State Machine), 시스템 구동 로직 [cite: 1]
* [cite_start]`NetworkUI.cpp` : Wi-Fi 관리, AWS 웹훅 통신, 앱 제어용 웹 서버 로직 [cite: 1]

## 하드웨어 핀 맵 (Pin Mapping)

모듈명,기능 및 역할,ESP32 할당 핀
Relay / Motor,4채널 독립 유로 제어 (맑음/흐림/비/눈),"GPIO 4, 13, 14, 27"
HX711 (1~4),4채널 로드셀 데이터 수집 및 클럭 동기화,"DT: GPIO 34, 36, 39, 32SCK: GPIO 33 (공통 결선)"
I2S Microphone,I2S 인터페이스 기반 오디오 샘플링,WS: GPIO 19SD: GPIO 35SCK: GPIO 22
DFPlayer Mini,SoftwareSerial 기반 오디오 재생 제어,RX: GPIO 25TX: GPIO 26
Nextion Display,HardwareSerial 기반 터치 디스플레이 통신,TX: GPIO 17RX: GPIO 16
MFRC522 RFID,커스텀 SPI 통신 기반 사용자 및 카트리지 인증,SCK: GPIO 18MISO: GPIO 12MOSI: GPIO 23CS: GPIO 15RST: GPIO 5
Status LED,시스템 상태 표시 및 동작 시각화,Main LED: GPIO 2Extra LED: GPIO 21

## 주요 작동 모드 (Operation Modes)

[cite_start]시리얼 모니터 또는 디스플레이를 통해 총 8가지 모드를 제어할 수 있습니다. [cite: 1]

1. [cite_start]**[수동 모드]** 사용자가 원하는 날씨의 디퓨저를 직접 선택하여 분사합니다. [cite: 1]
2. [cite_start]**[감성 모드]** 사용자 감성 데이터를 서버로 보내 AI가 어울리는 향을 추천합니다. [cite: 1]
3. [cite_start]**[날씨 모드]** 특정 지역의 실시간 날씨 정보를 API로 받아와 상황에 맞는 향을 자동 분사합니다. [cite: 1]
4. [cite_start]**[설정 모드]** 로드셀의 영점(Tare)을 잡고 기준 무게(Calibration)를 보정합니다. [cite: 1]
5. [cite_start]**[데모 모드]** 4가지 디퓨저가 순차적으로 자동 실행되어 기기를 시연합니다. [cite: 1]
6. [cite_start]**[음성 인식 모드]** I2S 마이크로 2초간 녹음된 음성(.wav)을 AWS 서버로 전송하여 명령을 수행합니다. [cite: 1]
7. [cite_start]**[파형 시각화]** 주변 소리의 크기(Volume)를 실시간으로 측정하여 막대그래프로 보여줍니다. [cite: 1]
8. [cite_start]**[소리 반응 모드]** 설정된 임계치(dB) 이상의 큰 소리가 나면 랜덤으로 디퓨저가 자동 분사됩니다. [cite: 1]

## 통신 프로토콜 (Network)

* [cite_start]기기가 구동되면 2초(`POLL_INTERVAL`) 간격으로 4개의 로드셀 무게 데이터(`weights` 배열)를 AWS Lambda 서버로 POST 전송합니다. [cite: 1]
* [cite_start]JSON 형식의 응답을 파싱하여 원격으로 디퓨저 분사 명령, 음악 선곡, 볼륨 조절(80~82번) 등을 수행합니다. [cite: 1]

---
*Developed by Jae-hong Ryu (21학번) / 한이음 멘토링 프로젝트*


