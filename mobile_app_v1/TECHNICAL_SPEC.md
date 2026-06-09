# 📚 Smart Diffuser Technical Specification

이 문서는 Smart Diffuser (AromaSync) 프로젝트의 시스템 구조와 주요 로직을 설명합니다.

## 1. Backend (AWS Lambda)
- **device_handler.py**: 기기 상태 동기화 및 Mailbox 패턴 관리
- **voice_handler.py**: Gemini AI 기반 음성 분석 및 향기 추천
- **core_mode_handler.py**: 날씨 및 감정 AI 모드 로직

## 2. Frontend (React)
- **DeviceContext.tsx**: 3초 주기 폴링 및 전역 상태 관리
- **ModesScreen.tsx**: 5대 스마트 모드 제어 UI
- **ScentsScreen.tsx**: 카트리지 잔량 실시간 시각화

## 3. Firmware (ESP32)
- **SystemLogic.cpp**: 노즐 분사 및 안전 제어 상태 머신
- **Hardware.cpp**: EMA 필터 적용 무게 측정 및 I2S 소음 분석

## 4. 데이터 흐름
- App(명령) -> Lambda(Mailbox) -> ESP32(실행) -> Lambda(상태업데이트) -> App(UI반영)