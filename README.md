# 🌬️ Smart AI Aroma Diffuser (스마트 AI 디퓨저)

![Version](https://img.shields.io/badge/version-v12.7.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-lightgrey.svg)
![Framework](https://img.shields.io/badge/framework-Arduino-00979C.svg)
![RTOS](https://img.shields.io/badge/OS-FreeRTOS-F18A20.svg)

주변 환경(날씨, 소음)과 사용자의 음성 명령을 분석하여 최적의 향기와 음악을 제공하는 **IoT 기반 스마트 디퓨저**입니다. ESP32와 FreeRTOS를 기반으로 설계되었으며, AWS Lambda 백엔드와 연동하여 동작합니다.

## 📌 주요 기능 (Key Features)

* **Ambient Mode (주변 환경 감응형 모드)**
  * I2S 마이크를 통해 백그라운드 소음(dB)을 실시간으로 측정 및 분석합니다.
  * 버블 정렬(Bubble Sort) 필터링을 통해 노이즈 스파이크를 제거하고, 안정된 데시벨 데이터를 AWS 서버로 전송하여 상황에 맞는 향기를 추천받습니다.
* **Voice Command (음성 인식 제어)**
  * 사용자의 음성을 16-bit WAV 포맷으로 녹음하여 서버로 전송하고, 반환된 JSON 데이터를 파싱하여 기기를 제어합니다.
* **Weather Sync (날씨 연동 분사)**
  * 설정된 지역의 실시간 날씨 데이터를 서버로부터 받아와, 날씨(맑음, 흐림, 비, 눈)에 어울리는 향기를 자동 분사합니다.
* **OTA (Over-The-Air) 무선 업데이트**
  * 케이블 연결 없이 Wi-Fi를 통해 펌웨어를 업데이트하며, Nextion 디스플레이에 진행률(%)을 시각화하여 UX를 개선했습니다.
* **Smart Dashboard & UI**
  * Nextion HMI 디스플레이를 통해 현재 모드, 향수 잔량(Progress Bar), 기기 상태를 직관적으로 제공합니다.

## 🛠 기술 스택 (Tech Stack)

* **MCU:** ESP32 (Dual-core)
* **OS / Task Management:** FreeRTOS (Task, Queue, EventGroup)
* **Hardware Interface:** I2S (Microphone), UART (Nextion, DFPlayer), GPIO (Motor, LED), 인터럽트 (ISR)
* **Libraries:** `ArduinoJson`, `WiFiManager`, `HX711`, `DFRobotDFPlayerMini`, `Adafruit_NeoPixel`
* **Network & Backend:** Wi-Fi, HTTP/HTTPS POST, AWS Lambda

## ⚙️ 핵심 펌웨어 최적화 (Firmware Architecture)

본 프로젝트는 시스템의 안정성과 응답 속도를 극대화하기 위해 다음과 같은 펌웨어 최적화를 거쳤습니다.

1. **Non-blocking 하드웨어 제어 (ISR 도입)**
   * 4개의 HX711 로드셀 센서 데이터를 읽을 때 발생하는 딜레이를 없애기 위해 **인터럽트(ISR) 플래그 방식**을 도입했습니다. 센서 데이터가 준비되었을 때만 즉시 읽어오도록 설계하여 메인 루프의 병목을 제거했습니다.
2. **FreeRTOS 멀티태스킹 및 큐(Queue) 통신**
   * 네트워크 통신(AWS HTTP POST)과 센서 측정(마이크, 저울)을 각각 독립된 Task로 분리했습니다.
   * `networkQueue`를 도입하여 메인 스레드의 블로킹 없이 네트워크 요청을 대기열에 안전하게 적재하고 비동기적으로 처리합니다.
3. **메모리 및 네트워크 최적화**
   * 동적 할당에 의한 힙(Heap) 메모리 단편화를 방지하기 위해 `String::reserve()`와 `JsonDocument`를 활용하여 메모리를 사전 할당했습니다.
   * 기기 고유의 식별을 위해 **Wi-Fi MAC 주소 기반의 동적 Device ID 생성기**를 구현하였으며, 사용자가 직접 ID를 덮어쓸 수 있는 커스텀 로직을 적용했습니다.
4. **C++ Template을 활용한 모듈화**
   * 자료형에 구애받지 않는 만능 정렬 함수(Template)를 구현하고, 하드웨어 초기화 및 LED 제어 로직의 중복 코드를 제거(DRY 원칙)하여 유지보수성을 상용 제품 수준으로 높였습니다.

## 🚀 시작하기 (Getting Started)

1. 기기 전원을 켜면 자동으로 `SmartDiffuser_Setup` AP 모드가 활성화됩니다.
2. 스마트폰이나 PC로 해당 Wi-Fi에 연결하여 초기 네트워크 설정을 진행합니다.
3. 연결이 완료되면 디스플레이의 가이드에 따라 `[설정]` 모드에 진입하여 로드셀 영점(Tare)을 맞춥니다.
4. 디스플레이 또는 시리얼 모니터를 통해 원하는 모드(수동, 날씨, 앰비언트 등)를 실행합니다.

---
*본 프로젝트는 정보통신공학과 종합설계의 일환으로 개발되었습니다.*
