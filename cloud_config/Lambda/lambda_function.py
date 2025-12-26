import os
import json
import requests
import xmltodict
import boto3
import logging
from datetime import datetime, timedelta, timezone

# 1. 로깅 설정 (CloudWatch에 예쁘게 찍히도록 설정)
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# DynamoDB 설정
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('DiffuserLog')

# 지역 정보
REGION_COORDS = {
    "서울": {"nx": "60", "ny": "127"}, "인천": {"nx": "55", "ny": "124"},
    "춘천": {"nx": "73", "ny": "134"}, "강릉": {"nx": "92", "ny": "131"},
    "수원": {"nx": "60", "ny": "121"}, "청주": {"nx": "69", "ny": "107"},
    "울릉도": {"nx": "102", "ny": "115"}, "전주": {"nx": "63", "ny": "89"},
    "대전": {"nx": "67", "ny": "100"}, "대구": {"nx": "89", "ny": "90"},
    "안동": {"nx": "91", "ny": "106"}, "포항": {"nx": "102", "ny": "94"},
    "목포": {"nx": "50", "ny": "67"}, "광주": {"nx": "58", "ny": "74"},
    "여수": {"nx": "73", "ny": "66"}, "부산": {"nx": "98", "ny": "76"},
    "울산": {"nx": "102", "ny": "84"}, "제주": {"nx": "52", "ny": "38"}
}

# ========================================================
# [로직 1] 날씨 모드
# ========================================================
def logic_weather_mode(weather, humidity):
    spray_code = 1
    # 습도 예외 처리
    try:
        if float(humidity) >= 75.0: weather = "흐림(고습도)"
    except: 
        logger.warning(f"습도 변환 에러: {humidity}")

    if any(x in weather for x in ["비", "강수"]): spray_code = 3
    elif "눈" in weather: spray_code = 4
    elif "흐림" in weather or "구름" in weather: spray_code = 2
    else: spray_code = 1
    
    return spray_code, weather, "Weather_Mode", 3

# ========================================================
# [로직 2] 감정 모드
# ========================================================
def logic_emotion_mode(user_emotion_input):
    now = datetime.now(timezone(timedelta(hours=9)))
    hour = now.hour
    is_daytime = (9 <= hour < 18)
    time_label = "Day" if is_daytime else "Night"

    spray_code = 1
    emotion_name = "Unknown"

    if user_emotion_input in ["1", "신남"]:
        emotion_name = "Happy"
        spray_code = 1 if is_daytime else 2
    elif user_emotion_input in ["2", "편안함"]:
        emotion_name = "Relaxed"
        spray_code = 4 # 낮밤 동일
    elif user_emotion_input in ["3", "화남"]:
        emotion_name = "Angry"
        spray_code = 2 if is_daytime else 3
    elif user_emotion_input in ["4", "슬픔"]:
        emotion_name = "Sad"
        spray_code = 3 if is_daytime else 4

    duration_sec = 3 if is_daytime else 2
    result_text = f"{emotion_name}/{time_label}"
    
    return spray_code, result_text, "Emotion_Mode", duration_sec

# ========================================================
# 공통 유틸리티
# ========================================================
def get_kma_time():
    now = datetime.now(timezone(timedelta(hours=9)))
    if now.minute < 40: now -= timedelta(hours=1)
    return now.strftime("%Y%m%d"), now.strftime("%H00")

def forecast(params):
    url = "http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst"
    try:
        res = requests.get(url, params=params, timeout=5)
        data = xmltodict.parse(res.text)
        
        # 로그에 API 응답 요약 남기기
        logger.info(f"[API 호출] Params: {params} | Status: {res.status_code}")
        
        if "response" not in data or "body" not in data["response"]: 
            logger.error(f"[API 에러] 응답 구조 이상: {res.text[:100]}")
            return "0", "API구조에러", "0"
            
        items = data["response"]["body"]["items"]["item"]
        if not isinstance(items, list): items = [items]
        
        t, p, h = "0", "0", "0"
        for i in items:
            if i["category"]=="T1H": t=i["obsrValue"]
            elif i["category"]=="PTY": p=i["obsrValue"]
            elif i["category"]=="REH": h=i["obsrValue"]
            
        w = "강수" if p!="0" else "맑음"
        return t, w, h
    except Exception as e:
        logger.error(f"[API 에러] 통신 실패: {str(e)}")
        return "0", "통신에러", "0"

def lambda_handler(event, context):
    # 1. [로그 시각화] 요청 들어옴
    logger.info("============== [NEW REQUEST] ==============")
    
    if "SERVICE_KEY" not in os.environ: 
        logger.critical("환경변수 SERVICE_KEY 누락됨")
        return {"statusCode": 500, "body": "Key Error"}
    
    try:
        body = json.loads(event.get("body", "{}"))
        logger.info(f"[입력 데이터] {json.dumps(body, ensure_ascii=False)}")
    except:
        body = {}
        logger.warning("Body가 JSON 형식이 아님")

    mode = body.get("mode", "weather")
    device_id = body.get("device", "unknown")
    region = body.get("region", "")
    
    # 변수 초기화
    spray_code = 0; result_text = ""; logic_name = ""; duration = 0
    temp = "0"; humidity = "0" # 로그용

    # 2. 로직 수행
    if mode == "emotion":
        user_emotion = body.get("user_emotion", "1")
        spray_code, result_text, logic_name, duration = logic_emotion_mode(user_emotion)
        logger.info(f"[감정 모드] 입력: {user_emotion} -> 결정: {result_text}")
        
    else: # Weather Mode
        if not region or region not in REGION_COORDS:
            logger.warning(f"[날씨 모드] 지역 정보 없음 또는 잘못됨: {region}")
            return {"statusCode": 200, "body": json.dumps({"spray":0, "message":"지역필요"})}
            
        nx, ny = REGION_COORDS[region]["nx"], REGION_COORDS[region]["ny"]
        bd, bt = get_kma_time()
        params = {"serviceKey": os.environ["SERVICE_KEY"], "pageNo": "1", "numOfRows": "10", 
                  "dataType": "XML", "base_date": bd, "base_time": bt, "nx": nx, "ny": ny}
        
        temp, weather_res, humidity = forecast(params)
        spray_code, result_text, logic_name, duration = logic_weather_mode(weather_res, humidity)
        logger.info(f"[날씨 모드] 기온:{temp}, 날씨:{weather_res}, 습도:{humidity} -> 결정:{result_text}")

    # 3. [로그 시각화] 결과 데이터 구성
    final_log = {
        "deviceId": device_id,
        "timestamp": datetime.now(timezone(timedelta(hours=9))).isoformat(),
        "mode": logic_name,
        "inputs": {"region": region, "temp": temp, "humidity": humidity} if mode == "weather" else {"emotion": body.get("user_emotion")},
        "output": {"spray_code": spray_code, "duration": duration, "reason": result_text}
    }

    # 4. DB 저장 및 콘솔 출력
    try:
        table.put_item(Item={
            "deviceId": device_id,
            "timestamp": final_log["timestamp"],
            "mode": logic_name,
            "result_text": result_text,
            "spray_code": spray_code,
            "duration": duration,
            "region": region if mode == "weather" else "Emotion_Input"
        })
        logger.info("[DB 저장] DynamoDB 저장 성공")
    except Exception as e:
        logger.error(f"[DB 저장 실패] {str(e)}")

    # 5. [중요] CloudWatch에 최종 요약 로그 찍기 (JSON 형태)
    logger.info(f"FINAL_RESULT: {json.dumps(final_log, ensure_ascii=False)}")
    
    response = {
        "spray": spray_code,
        "result_text": result_text,
        "duration": duration,
        "mode": logic_name
    }

    return {"statusCode": 200, "headers": {"Content-Type": "application/json"}, "body": json.dumps(response, ensure_ascii=False)}
