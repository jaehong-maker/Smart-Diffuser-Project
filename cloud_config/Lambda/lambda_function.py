import os
import json
import requests
import xmltodict
import boto3
import logging
from datetime import datetime, timedelta, timezone
from decimal import Decimal

# 로깅 설정
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# DynamoDB
dynamodb = boto3.resource('dynamodb')
log_table = dynamodb.Table('DiffuserLog')
state_table = dynamodb.Table('DiffuserState')

# 설정값
COOLDOWN_MINUTES = 3       # 쿨타임
CONSUMPTION_PER_SEC = 0.5
MAX_CAPACITY = 100.0

# 지역 정보
REGION_COORDS = {
    "서울": {"nx": "60", "ny": "127"},
    "양주": {"nx": "60", "ny": "121"},
    "양평": {"nx": "60", "ny": "121"},
    "수원": {"nx": "60", "ny": "121"},
    "미추홀구": {"nx": "55", "ny": "124"},
    "춘천": {"nx": "73", "ny": "134"},
    "강릉": {"nx": "92", "ny": "131"},
    "청주": {"nx": "69", "ny": "107"},
    "대전": {"nx": "67", "ny": "100"},
    "세종": {"nx": "67", "ny": "100"},
    "전주": {"nx": "63", "ny": "89"},
    "전라남도": {"nx": "58", "ny": "74"},
    "대구": {"nx": "89", "ny": "90"},
    "안동": {"nx": "91", "ny": "106"},
    "포항": {"nx": "102", "ny": "94"},
    "울릉도": {"nx": "102", "ny": "115"},
    "부산": {"nx": "98", "ny": "76"},
    "울산": {"nx": "102", "ny": "84"},
    "제주": {"nx": "52", "ny": "38"},
    "시흥": {"nx": "56","ny": "122"}
}


# 로직 함수들 (그대로 유지)
def logic_weather_mode(weather, humidity):
    spray_code = 1
    try:
        if float(humidity) >= 50.0: weather = "흐림(고습도)" # 50으로 수정됨
    except: pass
    if any(x in weather for x in ["비", "강수"]): spray_code = 3
    elif "눈" in weather: spray_code = 4
    elif "흐림" in weather or "구름" in weather: spray_code = 2
    else: spray_code = 1
    return spray_code, weather, "Weather_Mode", 3

def logic_emotion_mode(user_emotion_input):
    now = datetime.now(timezone(timedelta(hours=9)))
    is_daytime = (9 <= now.hour < 18)
    time_label = "Day" if is_daytime else "Night"
    spray_code = 1
    emotion_name = "Unknown"
    if user_emotion_input in ["1", "신남"]:
        emotion_name = "Happy"; spray_code = 1 if is_daytime else 2
    elif user_emotion_input in ["2", "편안함"]:
        emotion_name = "Relaxed"; spray_code = 4
    elif user_emotion_input in ["3", "화남"]:
        emotion_name = "Angry"; spray_code = 2 if is_daytime else 3
    elif user_emotion_input in ["4", "슬픔"]:
        emotion_name = "Sad"; spray_code = 3 if is_daytime else 4
    return spray_code, f"{emotion_name}/{time_label}", "Emotion_Mode", (3 if is_daytime else 2)

def get_kma_time():
    now = datetime.now(timezone(timedelta(hours=9)))
    if now.minute < 40: now -= timedelta(hours=1)
    return now.strftime("%Y%m%d"), now.strftime("%H00")

def forecast(params):
    url = "http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst"
    try:
        res = requests.get(url, params=params, timeout=5)
        data = xmltodict.parse(res.text)
        if "response" not in data or "body" not in data["response"]: return "0", "API에러", "0"
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

def get_device_state(device_id):
    try:
        res = state_table.get_item(Key={'deviceId': device_id})
        if 'Item' in res: return res['Item']
        return {'deviceId': device_id, 'last_spray_time': "2000-01-01T00:00:00", 'current_capacity': Decimal(str(MAX_CAPACITY)), 'last_spray_code': 0}
    except: return None

def update_device_state(device_id, last_time, new_capacity, spray_code):
    try:
        state_table.put_item(Item={
            'deviceId': device_id,
            'last_spray_time': last_time,
            'current_capacity': Decimal(str(new_capacity)),
            'last_spray_code': int(spray_code)
        })
    except Exception as e: logger.error(f"DB Write Error: {e}")

# ========================================================
# ★ 메인 핸들러 (수정됨)
# ========================================================
def lambda_handler(event, context):
    logger.info("============== [REQUEST START] ==============")
    try: body = json.loads(event.get("body", "{}"))
    except: body = {}
    
    device_id = body.get("device", "ESP32_Test")
    mode = body.get("mode", "weather")
    region = body.get("region", "")
    
    # ----------------------------------------------------
    # 1. [우선 계산] 쿨타임 체크 전에 '무슨 향'인지 먼저 계산
    # ----------------------------------------------------
    spray_code = 0; result_text = ""; logic_name = ""; duration = 0
    temp = "0"; humidity = "0"

    if mode == "emotion":
        spray_code, result_text, logic_name, duration = logic_emotion_mode(body.get("user_emotion", "1"))
    else:
        if "SERVICE_KEY" not in os.environ: return {"statusCode": 500, "body": "API Key Error"}
        if not region or region not in REGION_COORDS: return {"statusCode": 200, "body": json.dumps({"spray":0, "message":"지역 정보 없음"})}
        nx, ny = REGION_COORDS[region]["nx"], REGION_COORDS[region]["ny"]
        bd, bt = get_kma_time()
        params = {"serviceKey": os.environ["SERVICE_KEY"], "pageNo": "1", "numOfRows": "10", "dataType": "XML", "base_date": bd, "base_time": bt, "nx": nx, "ny": ny}
        temp, weather_res, humidity = forecast(params)
        spray_code, result_text, logic_name, duration = logic_weather_mode(weather_res, humidity)
    
    # ----------------------------------------------------
    # 2. [상태 확인] DB에서 이전 상태 불러오기
    # ----------------------------------------------------
    state = get_device_state(device_id)
    if not state: state = {} # 방어 코드
    
    last_time_str = state.get('last_spray_time', "2000-01-01T00:00:00")
    last_spray_code = int(state.get('last_spray_code', 0)) 
    current_capacity = float(state.get('current_capacity', MAX_CAPACITY))
    
    now = datetime.now(timezone(timedelta(hours=9)))
    
    # ----------------------------------------------------
    # 3. [쿨타임 판별] 계산된 향기와 이전 향기 비교
    # ----------------------------------------------------
    is_blocked = False
    log_msg = ""
    
    # 향기가 달라졌을 때만 쿨타임 체크
    if spray_code != last_spray_code:
        try:
            last_time = datetime.fromisoformat(last_time_str)
            diff_minutes = (now - last_time).total_seconds() / 60
            
            if diff_minutes < COOLDOWN_MINUTES:
                is_blocked = True
                remaining = int(COOLDOWN_MINUTES - diff_minutes)
                # ★ 메시지를 명확하게 만듦
                log_msg = f"[쿨타임] 향기 변경 불가 ({remaining}분 남음)"
                logger.warning(log_msg)
        except: pass
    
    # ----------------------------------------------------
    # 4. [로그 시각화] 차단되었어도 '원래 하려던 것' 로그 남기기
    # ----------------------------------------------------
    final_log = {
        "deviceId": device_id,
        "timestamp": now.isoformat(),
        "mode": logic_name,
        "inputs": {"region": region, "weather": result_text, "temp": temp, "humidity": humidity} if mode == "weather" else {"emotion": body.get("user_emotion")},
        "target_output": {"spray_code": spray_code, "reason": result_text}, # 원래 하려던 것
        "status": "BLOCKED" if is_blocked else "SUCCESS",
        "block_reason": log_msg if is_blocked else "N/A" # 차단 이유
    }
    logger.info(f"FINAL_RESULT: {json.dumps(final_log, ensure_ascii=False)}")

    # ----------------------------------------------------
    # 5. [응답 분기] 차단 여부에 따른 처리
    # ----------------------------------------------------
    
    # [차단됨] -> spray 0 전송 (중요: 로그는 다 찍었음)
    if is_blocked:
        return {
            "statusCode": 200, "headers": {"Content-Type": "application/json"},
            "body": json.dumps({
                "spray": 0,             # 분사 금지
                "result_text": "WAIT",
                "message": log_msg,     # ★ ESP32 화면에 띄울 메시지
                "mode": "CoolDown"
            }, ensure_ascii=False)
        }

    # [통과됨] -> 상태 업데이트 및 분사
    usage = duration * CONSUMPTION_PER_SEC
    new_capacity = current_capacity - usage
    if new_capacity < 0: new_capacity = 0.0
    new_capacity = round(new_capacity, 2)
    
    # DB 업데이트
    if spray_code > 0:
        update_device_state(device_id, now.isoformat(), new_capacity, spray_code)

    # 과거 로그 DB 저장
    try:
        log_table.put_item(Item={
            "deviceId": device_id, "timestamp": now.isoformat(), "mode": logic_name,
            "result_text": result_text, "spray_code": spray_code, "duration": duration,
            "region": region if mode == "weather" else "Emotion"
        })
    except: pass

    # 최종 성공 응답
    return {
        "statusCode": 200, "headers": {"Content-Type": "application/json"},
        "body": json.dumps({
            "spray": spray_code,
            "result_text": result_text,
            "duration": duration,
            "remaining_capacity": new_capacity,
            "message": f"분사 성공! ({result_text})",
            "mode": logic_name
        }, ensure_ascii=False)
    }
