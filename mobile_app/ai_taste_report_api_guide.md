# 스마트 디퓨저 취향 리포트(Preference Report) API 상세 명세서

모바일 앱에서 사용자의 **취향 리포트 기능**을 연동하기 위한 최종 API 가이드입니다. 
본 기능은 백엔드에 100% 구현되어 있으며, 앱 개발 시 아래 명세에 따라 API를 호출하기만 하면 됩니다.

---

## 1. 데이터베이스(DB) 구조 및 아키텍처
취향 분석 데이터는 사용자 정보나 기기 상태 테이블과 완전히 분리된 **`DiffuserPref` (취향 전용 테이블)**에서 독립적으로 관리됩니다. 

- **파티션 키 (Partition Key):** `email` (사용자 이메일)
- **정렬 키 (Sort Key):** `context` (분사된 상황 키워드, 예: `Weather_Rain_오전`)
- **속성 (Attributes):** `scent_1_score`, `scent_2_score`, ... `scent_7_score`

> **💡 설계 의도**
> 데이터가 무한정 길어지는 것을 방지하고 조회 속도를 극대화하기 위해 독립된 테이블로 분리했습니다. 피드백 발생 시 해당 `context` 행의 향기 점수에 `+1` 또는 `-1`을 더하는(ADD) 형태로 매우 가볍게 동작합니다.

---

## 2. 피드백 전송 (FEEDBACK) API
앱에서 디퓨저가 뿜은 향기에 대해 사용자가 **'좋아요(👍)'** 또는 **'싫어요(👎)'** 버튼을 눌렀을 때 호출합니다. 

### [요청 / Request]
- **Method:** `POST` (AWS Lambda 진입점)
- **Content-Type:** `application/json`
- **Body:**
```json
{
  "action": "FEEDBACK",
  "device": "ESP32_VOICE",    // 기기 고유 ID (필수)
  "email": "user@email.com",  // 사용자 계정 이메일 (권장)
  "value": 1,                 // 1: 좋아요, -1: 싫어요
  "data": "5_Weather_Rain"    // {향기번호}_{상황Context} 형태의 문자열
}
```
*(주의: `data` 파라미터는 앱이 디퓨저 모드 실행 시 전달받은 `context` 값과 향기 번호를 조합하여 전송해야 합니다.)*

### [응답 / Response]
```json
{
  "message": "좋아요! 사용자 선호도 학습 완료", 
  "spray": 5,
  "context": "Weather_Rain"
}
```

---

## 3. 취향 리포트 조회 (REPORT) API
마이페이지나 홈 화면에서 **"분석된 최애 향기 리포트"**를 보여줄 때 호출합니다. `DiffuserPref` DB에 쌓인 점수를 합산하여 가장 높은 점수를 받은 조합을 자연어 문장으로 반환합니다.

### [요청 / Request]
- **Method:** `POST`
- **Content-Type:** `application/json`
- **Body:**
```json
{
  "action": "REPORT",
  "email": "user@email.com"   // 조회할 대상 이메일
}
```

### [응답 / Response] 상세

**① 분석 성공 (피드백 데이터 충분)**
```json
{
  "result": "SUCCESS",
  "summary": "당신은 주로 비오는 날 오전에 라벤더 향기을(를) 가장 좋아하시네요!",
  "total_analyzed": 15, // 누적된 총 긍정 피드백 점수
  "top_scent": "라벤더", // 최고 점수 향기
  "top_context": "비오는 날 오전에" // 최고 점수 상황
}
```

**② 분석 불가 (피드백 데이터 없음/부족)**
```json
{
  "result": "SUCCESS",
  "summary": "아직 충분한 취향 데이터가 모이지 않았어요. 디퓨저를 더 많이 사용하고 피드백을 남겨주세요!",
  "total_analyzed": 0,
  "top_scent": "정보 없음",
  "top_context": "정보 없음"
}
```

---

## 4. 🛠 백엔드 문장 조합 및 번역 매핑 (참고사항)
앱에서 받은 `summary` 문장은 외부 AI가 아닌 **서버 내부의 자동 매핑 로직**에 의해 생성됩니다. DB에 저장된 영문 꼬리표(Context)를 서버가 아래 규칙에 따라 한국어로 번역한 뒤, 문장 템플릿에 맞게 조립하여 반환하므로 속도가 매우 빠르고 안정적입니다.

| 서버 내부 DB 키워드 (Context) | 번역된 한국어 (top_context) | 비고 |
| :--- | :--- | :--- |
| `Weather_Rain` | 비오는 날 | 날씨 연동 모드 |
| `Weather_Sunny` | 맑은 날 | 날씨 연동 모드 |
| `Weather_Snow` | 눈오는 날 | 날씨 연동 모드 |
| `Weather_Cloudy` | 흐린 날 | 날씨 연동 모드 |
| `새벽`, `오전`, `오후`, `밤` | 새벽에, 오전에, 오후에, 밤에 | 날씨 뒤에 조합됨 (예: 비오는 날 오전에) |
| `Emotion_Happy` | 기분이 좋을 때 | 감정 모드 |
| `Emotion_Sad` | 우울할 때 | 감정 모드 |
