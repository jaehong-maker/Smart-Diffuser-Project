import os
import json
import requests
import xmltodict
import boto3
from datetime import datetime, timedelta, timezone

# ==============================
# DynamoDB 설정
# ==============================
dynamodb = boto3.resource('dynamodb')
table = dynamodb.Table('DiffuserLog')

def save_log(device_id, region, temp, weather, spray_code, humidity):
    """
    디퓨저 동작 로그 저장
    """
    # 로그 가독성을 위해 코드별 향기 이름 매핑
    scent_map = {1: "Sunny_Citrus", 2: "Cloudy_Cotton", 3: "Rainy_Aqua", 4: "Snowy_Musk", 0: "None"}
    scent_name = scent_map.get(spray_code, "Unknown")

    try:
        table.put_item(
            Item={
                "deviceId": device_id,
                "timestamp": datetime.now(
                    timezone(timedelta(hours=9))
                ).isoformat(),      # KST 한국시간
                "region": region,
                "temperature": temp,
                "humidity": humidity,     # 습도 데이터도 저장
                "weather": weather,       # 최종 판단된 날씨 (예: 흐림)
                "spray_code": spray_code, # 아두이노 동작 번호
                "scent": scent_name       # 향기 이름
            }
        )
    except Exception as e:
        print(f"DynamoDB Error: {e}")

# ==============================
# 지역별 기상청 격자 좌표
# ==============================
REGION_COORDS = {
    "서울": {"nx": "60", "ny": "127"},
    "인천": {"nx": "55", "ny": "124"},
    "춘천": {"nx": "73", "ny": "134"},
    "강릉": {"nx": "92", "ny": "131"},
    "수원": {"nx": "60", "ny": "121"},
    "청주": {"nx": "69", "ny": "107"},
    "울릉도": {"nx": "102", "ny": "115"},
    "전주": {"nx": "63", "ny": "89"},
    "대전": {"nx": "67", "ny": "100"},
    "대구": {"nx": "89", "ny": "90"},
    "안동": {"nx": "91", "ny": "106"},
    "포항": {"nx": "102", "ny": "94"},
    "목포": {"nx": "50", "ny": "67"},
    "광주": {"nx": "58", "ny": "74"},
    "여수": {"nx": "73", "ny": "66"},
    "부산": {"nx": "98", "ny": "76"},
    "울산": {"nx": "102", "ny": "84"},
    "제주": {"nx": "52", "ny": "38"}
}

# ==============================
# 기상청 시간 계산 (KST)
# ==============================
def get_kma_time_params():
    kst = timezone(timedelta(hours=9))
    now = datetime.now(kst)

    # 초단기 실황은 40분 이전이면 이전 시각 조회
    if now.minute < 40:
        now -= timedelta(hours=1)

    return now.strftime("%Y%m%d"), now.strftime("%H00")

# ==============================
# 날씨 코드 매핑
# ==============================
pty_to_weather = {
    "0": "없음",
    "1": "비",
    "2": "비/눈",
    "3": "눈",
    "5": "빗방울",
    "6": "빗방울눈날림",
    "7": "눈날림"
}

# ==============================
# 기상청 초단기 실황 조회 (습도 추가)
# ==============================
def forecast(params):
    url = "http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst"

    try:
        res = requests.get(url, params=params, timeout=5)
        # XML 파싱
        data = xmltodict.parse(res.text)

        # 데이터 구조 확인
        if "response" not in data or "body" not in data["response"]:
             return "0", "API구조에러", "0"

        items = data["response"]["body"]["items"]["item"]
        if not isinstance(items, list):
            items = [items]

        temp = "0"      # 기온 (T1H)
        pty = "0"       # 강수형태 (PTY)
        humidity = "0"  # 습도 (REH) - ★추가됨

        for item in items:
            if item["category"] == "T1H":
                temp = item["obsrValue"]
            elif item["category"] == "PTY":
                pty = item["obsrValue"]
            elif item["category"] == "REH":
                humidity = item["obsrValue"]

        # 1차 날씨 판단 (강수 여부만 확인)
        if pty != "0":
            weather = pty_to_weather.get(pty, "강수")
        else:
            weather = "맑음" # 일단 맑음으로 둠 (나중에 습도로 '흐림' 판단)

        return temp, weather, humidity

    except Exception as e:
        print(f"Forecast Error: {e}")
        return "0", "통신에러", "0"

# ==============================
# Lambda 메인 핸들러
# ==============================
def lambda_handler(event, context):

    # 0. 환경변수 확인
    if "SERVICE_KEY" not in os.environ:
        return {
            "statusCode": 500,
            "body": "SERVICE_KEY 환경변수 미설정"
        }

    SERVICE_KEY = os.environ["SERVICE_KEY"]

    # 1. 요청 파싱 (JSON Body)
    body = {}
    if event.get("body"):
        try:
            body = json.loads(event["body"])
        except:
            pass

    device_id = body.get("device", "unknown")
    region = body.get("region", "").strip()

    # 2. 지역 미입력/미지원 처리
    if not region or region not in REGION_COORDS:
        return {
            "statusCode": 200,
            "headers": {"Content-Type": "application/json; charset=utf-8"},
            "body": json.dumps({
                "spray": 0,
                "weather": "지역에러",
                "region": region if region else "None"
            }, ensure_ascii=False)
        }

    # 3. 기상청 조회
    nx = REGION_COORDS[region]["nx"]
    ny = REGION_COORDS[region]["ny"]
    base_date, base_time = get_kma_time_params()

    params = {
        "serviceKey": SERVICE_KEY,
        "pageNo": "1",
        "numOfRows": "10",
        "dataType": "XML",
        "base_date": base_date,
        "base_time": base_time,
        "nx": nx,
        "ny": ny
    }

    # API 호출 (기온, 날씨, 습도 받아옴)
    temp, weather, humidity = forecast(params)

    # ---------------------------------------------------------
    # ★ [핵심 로직] 습도 기반 흐림 판별
    # 비(강수)가 안 오는데 습도가 75% 이상이면 '흐림'으로 간주
    # ---------------------------------------------------------
    if weather == "맑음":
        try:
            hum_val = float(humidity)
            if hum_val >= 70.0:  # 습도 기준값 (조절 가능)
                weather = "흐림"
                print(f"[알림] {region} 습도 {humidity}%로 높음 -> '흐림'으로 변경")
        except:
            pass # 변환 에러 시 맑음 유지

    # 4. 최종 날씨 -> 아두이노 모터 코드(1~4) 매핑
    spray_code = 0
    
    if weather == "통신에러" or weather == "API구조에러":
        spray_code = 0 # 에러 시 작동 안 함
    elif any(x in weather for x in ["비", "강수", "빗방울"]):
        spray_code = 3 # 비 (14번 핀)
    elif "눈" in weather:
        spray_code = 4 # 눈 (13번 핀)
    elif "흐림" in weather or "구름" in weather:
        spray_code = 2 # 흐림 (27번 핀) - 습도가 높으면 여기로 옴
    else:
        spray_code = 1 # 맑음 (26번 핀)

    # 5. DynamoDB 저장 (습도 데이터 포함)
    save_log(device_id, region, temp, weather, spray_code, humidity)

    # 6. 응답 생성
    response_body = {
        "spray": spray_code,   # 아두이노 제어 코드
        "weather": weather,    # "맑음", "흐림", "비", "눈"
        "region": region,
        "temp": temp,          # (참고용) 기온
        "humidity": humidity   # (참고용) 습도
    }

    return {
        "statusCode": 200,
        "headers": {"Content-Type": "application/json; charset=utf-8"},
        "body": json.dumps(response_body, ensure_ascii=False)
    }
