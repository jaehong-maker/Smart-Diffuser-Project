# SmartDiffuser Setup Guide

디바이스 첫 부팅 시 Nextion 디스플레이가 보여줄 **QR 코드의 랜딩 페이지**입니다.
완전 정적(HTML/CSS/JS)이라 빌드 단계 없이 어떤 호스팅에든 그대로 올라갑니다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `index.html` | 단일 페이지 가이드 (한국어, 모바일 최적화) |
| `styles.css` | 디자인 — 사이트 컬러는 `--accent` 변수 하나로 제어 |
| `script.js` | 맨 위로 버튼 + 목차 현재 섹션 하이라이트 |

## 로컬 미리보기

```bash
cd Smart-Diffuser-Project/setup_guide
python -m http.server 8000
# 브라우저에서 http://localhost:8000 열기
```

PowerShell이라면:

```powershell
cd Smart-Diffuser-Project\setup_guide
python -m http.server 8000
```

## 배포 — 옵션별

### 1. GitHub Pages (가장 간단)
1. 리포 Settings → Pages → Source를 `main` 브랜치로 설정
2. 폴더는 `/Smart-Diffuser-Project/setup_guide` 지정
3. 배포 URL 예: `https://<유저명>.github.io/<리포명>/Smart-Diffuser-Project/setup_guide/`

### 2. Cloudflare Pages / Netlify / Vercel
- 빌드 명령 비워두고, 출력 디렉터리만 `Smart-Diffuser-Project/setup_guide` 지정.
- 무료 플랜으로 충분.

### 3. AWS S3 + CloudFront (이미 AWS 쓰는 중이라면)
- 본 프로젝트가 이미 Lambda를 쓰므로 동일 계정에서 정적 호스팅이 자연스러움.
- 버킷에 파일 3개 업로드 → 정적 웹사이트 호스팅 활성화.

## QR 코드 만들기

배포 URL이 정해지면 QR을 생성해 펌웨어에 넣습니다.

### CLI 한 줄 (qrencode)
```bash
qrencode -o setup-qr.png -s 10 "https://example.com/setup_guide/"
```

### 스크립트로 (Python)
```python
import qrcode
img = qrcode.make("https://example.com/setup_guide/")
img.save("setup-qr.png")
```

### 펌웨어에 임베드 — Nextion에 띄우는 방법
Nextion HMI 에디터에서 **QRcode 컴포넌트**를 페이지에 배치하고
ESP32에서 다음과 같이 텍스트만 쏘면 디스플레이가 즉석에서 QR을 그립니다.

```cpp
// 부팅 직후 AP 모드 진입 시 호출
nexSend("page setup");
nexSend("qr0.txt=\"https://example.com/setup_guide/\"");
```

> Nextion `QRcode`는 텍스트 컴포넌트와 동일하게 `.txt` 속성을 받으며,
> URL이 길어질수록 자동으로 더 조밀한 QR로 그려집니다. 영문/숫자만 쓰는 게 인식률이 가장 좋습니다.

## 펌웨어 통합 위치

`NetworkUI.cpp`의 `displayWatcherTask` (또는 `connectWiFi()`가 AP 모드로 진입하는 분기)
에서 위 두 줄을 호출하면 됩니다. 사용자가 첫 부팅 시 디스플레이의 QR을 폰으로 찍으면 본 가이드가 열립니다.

## 콘텐츠 수정 가이드

- **단계를 추가/제거**할 때: `index.html`의 `<article class="step">` 블록을 그대로 복사해 번호만 바꾸고, 상단 `.toc ol` 목록에도 한 줄 추가.
- **컬러 톤 변경**: `styles.css` 최상단 `:root` 의 `--accent` 한 줄만 바꾸면 강조색 전체가 일괄 변경됩니다.
- **버전 표시**: 푸터의 `SmartDiffuser Setup Guide v1.0`. 배포할 때마다 갱신 권장.

## 디자인 원칙

- **모바일 우선** — 폰으로 QR을 찍어 바로 보는 시나리오라 `max-width: 720px` 단일 컬럼.
- **외부 의존성 0** — 폰트/아이콘/CDN 없이 시스템 폰트만 사용 → 오프라인/저속 환경에서도 즉시 렌더링.
- **인쇄 친화** — `@media print` 규칙으로 종이 매뉴얼로도 출력 가능.
- **접근성** — 스킵 링크, `aria-current`, `prefers-reduced-motion` 지원.
