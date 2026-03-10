# Hardware Design Data

디퓨저 프로젝트의 하드웨어 설계 데이터 및 회로 정보 폴더입니다.

## 폴더 구조
- `sch.json`: EasyEDA 기반 회로도(Schematic) 원본 파일
- `pcb.json`: PCB 아트웍(Layout) 설계 원본 파일

## 설계 환경
- **Design Tool**: EasyEDA (Standard/Pro)
- **Main MCU**: ESP32Devkit
- **Key Modules**: 
  - HX711 (로드셀 무게 센서)
  - INMP441 (I2S 기반 소음 인식 마이크)

## 다운로드 및 사용 방법
- 해당 파일은 **EasyEDA (Standard 또는 Pro)** 에서 편집 가능합니다.
1. [EasyEDA 공식 사이트](https://easyeda.com/)에 접속하여 로그인합니다.
2. 상단 메뉴에서 **[File] -> [Open] -> [EasyEDA...]**를 클릭합니다.
3. 다운로드한 `sch.json`(회로도) 또는 `pcb.json`(PCB) 파일을 선택하여 업로드합니다.
4. 파일이 정상적으로 열리면 수정 및 부품 확인이 가능합니다.
