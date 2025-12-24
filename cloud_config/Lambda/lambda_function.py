import os
import json
import requests
import xmltodict
import boto3
from datetime import datetime, timedelta, timezone

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
# [로직 1] 날씨 모드 (기존 유지)
# ========================================================
def logic_weather_mode(weather, humidity):
    spray_code = 1
    if weather == "맑음":
        try:
            if float(humidity) >= 75.0: weather = "흐림"
        except: pass

    if any(x in weather for x in ["비", "강수"]): spray_code = 3
    elif "눈" in weather: spray_code = 4
    elif "흐림" in weather or "구름" in weather: spray_code = 2
    else: spray_code = 1
    
    return spray_code, weather, "Weather_Mode", 3

# ========================================================
# [로직 2] 감정 모드 (★시간대별 향기 변경★)
# ========================================================
def logic_emotion_mode(user_emotion_input):
    # 1. 시간 확인 (KST)
    now = datetime.now(timezone(timedelta(hours=9)))
    hour = now.hour
    
    # 2. 낮/밤 판별 (09시~18시: 낮)
    is_daytime = (9 <= hour < 18)
    time_label = "Day" if is_daytime else "Night"

    # 3. 감정 + 시간 -> 향기(모터) 매핑
    spray_code = 1
    emotion_name = ""

    # (1) 신남 (Happy)
    if user_emotion_input in ["1", "신남"]:
        emotion_name = "신남"
        if is_daytime:
            spray_code = 1  # 낮: 활기찬 1번 향
        else:
            spray_code = 2  # 밤: 은은하게 기분 좋은 2번 향

    # (2) 편안함 (Relaxed)
    elif user_emotion_input in ["2", "편안함"]:
        emotion_name = "편안"
        if is_daytime:
            spray_code = 4  # 낮: 포근한 4번 향
        else:
            spray_code = 4  # 밤: 숙면을 위한 4번 향 (동일하게 유지)

    # (3) 화남 (Angry)
    elif user_emotion_input in ["3", "화남"]:
        emotion_name = "화남"
        if is_daytime:
            spray_code = 2  # 낮: 열을 식혀주는 쿨링 2번 향
        else:
            spray_code = 3  # 밤: 차분하게 가라앉히는 3번 향

    # (4) 슬픔 (Sad)
    elif user_emotion_input in ["4", "슬픔"]:
        emotion_name = "슬픔"
        if is_daytime:
            spray_code = 3  # 낮: 기분 전환 리프레시 3번 향
        else:
            spray_code = 4  # 밤: 따뜻하게 위로하는 4번 향

    # 4. 분사 강도 설정 (밤에는 조금 약하게)
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
        if "response" not in data or "body" not in data["response"]: return "0", "API구조에러", "0"
        items = data["response"]["body"]["items"]["item"]
        if not isinstance(items, list): items = [items]
        t, p, h = "0", "0", "0"
        for i in items:
            if i["category"]=="T1H": t=i["obsrValue"]
            elif i["category"]=="PTY": p=i["obsrValue"]
            elif i["category"]=="REH": h=i["obsrValue"]
        w = "강수" if p!="0" else "맑음"
        return t, w, h
    except: return "0", "통신에러", "0"

def lambda_handler(event, context):
    if "SERVICE_KEY" not in os.environ: return {"statusCode": 500, "body": "Key Error"}
    
    body = json.loads(event.get("body", "{}"))
    mode = body.get("mode", "weather")
    device_id = body.get("device", "unknown")
    
    spray_code = 0
    result_text = ""
    logic_name = ""
    duration = 0
    region = body.get("region", "")

    if mode == "emotion":
        # 감정 모드 실행
        user_emotion = body.get("user_emotion", "1")
        spray_code, result_text, logic_name, duration = logic_emotion_mode(user_emotion)
    else:
        # 날씨 모드 실행
        if not region or region not in REGION_COORDS:
            return {"statusCode": 200, "body": json.dumps({"spray":0, "message":"지역필요"})}
        nx, ny = REGION_COORDS[region]["nx"], REGION_COORDS[region]["ny"]
        bd, bt = get_kma_time()
        params = {"serviceKey": os.environ["SERVICE_KEY"], "pageNo": "1", "numOfRows": "10", 
                  "dataType": "XML", "base_date": bd, "base_time": bt, "nx": nx, "ny": ny}
        temp, weather_res, humidity = forecast(params)
        spray_code, result_text, logic_name, duration = logic_weather_mode(weather_res, humidity)

    # DB 저장
    try:
        table.put_item(Item={
            "deviceId": device_id,
            "timestamp": datetime.now(timezone(timedelta(hours=9))).isoformat(),
            "mode": logic_name,
            "result_text": result_text,
            "spray_code": spray_code,
            "duration": duration,
            "region": region if mode == "weather" else "Emotion_Input"
        })
    except: pass

    # 응답
    response = {
        "spray": spray_code,
        "result_text": result_text,
        "duration": duration,
        "mode": logic_name
    }

    return {"statusCode": 200, "headers": {"Content-Type": "application/json"}, "body": json.dumps(response, ensure_ascii=False)}
