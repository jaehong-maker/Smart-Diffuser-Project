import os
import json
import base64
import time
import uuid
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

# [추가] AWS 클라이언트 (S3, Transcribe)
s3 = boto3.client("s3")
transcribe = boto3.client("transcribe")

# 설정값
COOLDOWN_MINUTES = 0.33       # 쿨타임
CONSUMPTION_PER_SEC = 0.5
MAX_CAPACITY = 100.0

# 지역 정보 (기존 목록 유지)
REGION_COORDS = {
    "서울": {"nx": "60", "ny": "127"},
    "양주": {"nx": "60", "ny": "121"},
    "양평": {"nx": "60", "ny": "121"},
    "인천": {"nx": "55", "ny": "124"},
    "강원": {"nx": "73", "ny": "134"},
    "충북": {"nx": "69", "ny": "107"},
    "충남": {"nx": "67", "ny": "100"},
    "대전": {"nx": "67", "ny": "100"},
    "세종": {"nx": "67", "ny": "100"},
    "전북": {"nx": "63", "ny": "89"},
    "광주": {"nx": "58", "ny": "74"},
    "전남": {"nx": "58", "ny": "74"},
    "대구": {"nx": "89", "ny": "90"},
    "경북": {"nx": "102", "ny": "94"},
    "울릉도": {"nx": "102", "ny": "115"},
    "경남": {"nx": "98", "ny": "76"},
    "부산": {"nx": "98", "ny": "76"},
    "울산": {"nx": "102", "ny": "84"},
    "제주": {"nx": "52", "ny": "38"},
    "시흥": {"nx": "56", "ny": "122"}
}

# [추가됨] ★ 앱 연동을 위한 우체통(Mailbox) 함수
# [수정됨] 지역 정보(save_region)까지 함께 저장/조회하는 함수
def manage_mailbox(device_id, save_cmd=None, save_region=None):
    try:
        if save_cmd is not None:
            # [앱 -> 서버] 명령과 지역 저장 (편지 쓰기)
            update_exp = "set pending_cmd = :c"
            exp_vals = {':c': int(save_cmd)}
            
            # 지역 정보가 있으면 같이 저장
            if save_region:
                update_exp += ", pending_region = :r"
                exp_vals[':r'] = str(save_region)

            state_table.update_item(
                Key={'deviceId': device_id},
                UpdateExpression=update_exp,
                ExpressionAttributeValues=exp_vals
            )
            return int(save_cmd)
        else:
            # [ESP32 -> 서버] 명령 조회 (편지 읽기)
            res = state_table.get_item(Key={'deviceId': device_id})
            if 'Item' in res:
                cmd = int(res['Item'].get('pending_cmd', 0))
                saved_region = res['Item'].get('pending_region', "") # 저장된 지역 꺼내기

                if cmd > 0:
                    # 읽었으면 초기화 (0으로)
                    state_table.update_item(
                        Key={'deviceId': device_id},
                        UpdateExpression="set pending_cmd = :z, pending_region = :e",
                        ExpressionAttributeValues={':z': 0, ':e': ""}
                    )
                    return cmd, saved_region # 명령과 지역을 같이 반환
            return 0, "" # 명령 없음
    except Exception as e:
        logger.error(f"Mailbox Error: {e}")
        return 0, ""

# 로직 함수들 (그대로 유지)
def logic_weather_mode(weather, humidity):
    spray_code = 1
    try:
        if float(humidity) >= 50.0:
            weather = "흐림(고습도)"
    except:
        pass
    if any(x in weather for x in ["비", "강수"]):
        spray_code = 3
    elif "눈" in weather:
        spray_code = 4
    elif "흐림" in weather or "구름" in weather:
        spray_code = 2
    else:
        spray_code = 1
    return spray_code, weather, "Weather_Mode", 3

def logic_emotion_mode(user_emotion_input):
    now = datetime.now(timezone(timedelta(hours=9)))
    is_daytime = (9 <= now.hour < 18)
    time_label = "Day" if is_daytime else "Night"
    spray_code = 1
    emotion_name = "Unknown"
    if user_emotion_input in ["1", "신남"]:
        emotion_name = "Happy"
        spray_code = 1 if is_daytime else 2
    elif user_emotion_input in ["2", "편안함"]:
        emotion_name = "Relaxed"
        spray_code = 4
    elif user_emotion_input in ["3", "화남"]:
        emotion_name = "Angry"
        spray_code = 2 if is_daytime else 3
    elif user_emotion_input in ["4", "슬픔"]:
        emotion_name = "Sad"
        spray_code = 3 if is_daytime else 4
    return spray_code, f"{emotion_name}/{time_label}", "Emotion_Mode", (3 if is_daytime else 2)

def get_kma_time():
    now = datetime.now(timezone(timedelta(hours=9)))
    if now.minute < 40:
        now -= timedelta(hours=1)
    return now.strftime("%Y%m%d"), now.strftime("%H00")

def forecast(params):
    url = "http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst"
    try:
        res = requests.get(url, params=params, timeout=5)
        data = xmltodict.parse(res.text)
        if "response" not in data or "body" not in data["response"]:
            return "0", "API에러", "0"
        items = data["response"]["body"]["items"]["item"]
        if not isinstance(items, list):
            items = [items]
        t, p, h = "0", "0", "0"
        for i in items:
            if i["category"] == "T1H":
                t = i["obsrValue"]
            elif i["category"] == "PTY":
                p = i["obsrValue"]
            elif i["category"] == "REH":
                h = i["obsrValue"]
        w = "강수" if p != "0" else "맑음"
        return t, w, h
    except:
        return "0", "통신에러", "0"

def get_device_state(device_id):
    try:
        res = state_table.get_item(Key={'deviceId': device_id})
        if 'Item' in res:
            return res['Item']
        return {
            'deviceId': device_id,
            'last_spray_time': "2000-01-01T00:00:00",
            'current_capacity': Decimal(str(MAX_CAPACITY)),
            'last_spray_code': 0
        }
    except:
        return None

def update_device_state(device_id, last_time, new_capacity, spray_code, weight_g):
    try:
        state_table.put_item(Item={
            'deviceId': device_id,
            'last_spray_time': last_time,
            'current_capacity': Decimal(str(new_capacity)),
            'last_spray_code': int(spray_code),
            'last_weight_g': Decimal(str(weight_g))
        })
    except Exception as e:
        logger.error(f"DB Write Error: {e}")

# ========================================================
# [추가] 음성 처리 유틸
# ========================================================

def _get_header(headers: dict, key: str) -> str:
    if not isinstance(headers, dict):
        return ""
    # API GW/Function URL 환경에 따라 소문자/대문자 섞여서 옴
    for k, v in headers.items():
        if str(k).lower() == key.lower():
            return v
    return ""

def _guess_device_id(event, body_dict):
    # 너 코드 호환: device / deviceId 모두 지원
    # 음성 바이너리 요청은 JSON body가 없을 수 있으니 header/query도 봄
    qs = event.get("queryStringParameters") or {}
    headers = event.get("headers") or {}
    device_id = ""

    if isinstance(body_dict, dict):
        device_id = body_dict.get("device", "") or body_dict.get("deviceId", "")

    if not device_id:
        device_id = qs.get("deviceId") or qs.get("device") or ""

    if not device_id:
        device_id = _get_header(headers, "x-device-id")

    return device_id if device_id else "ESP32_VOICE"

def _transcribe_wav_from_s3(bucket: str, key: str, language_code="ko-KR", sample_rate=16000):
    """
    S3에 올라간 wav를 Transcribe로 텍스트 변환.
    - Transcribe는 job_name이 유니크해야 함
    - 완료까지 폴링(짧게)
    """
    job_name = f"diffuser-{uuid.uuid4().hex}"
    media_uri = f"s3://{bucket}/{key}"

    transcribe.start_transcription_job(
        TranscriptionJobName=job_name,
        Media={"MediaFileUri": media_uri},
        MediaFormat="wav",
        LanguageCode=language_code,
        Settings={
            # 필요 시 옵션 조정 가능
            "ShowSpeakerLabels": False
        }
    )

    # Lambda 타임아웃 고려: 너무 길게 기다리면 안됨
    # 2초 음성은 보통 수 초 내 완료되는 편이라 짧게 폴링
    for _ in range(25):  # 최대 약 25초
        job = transcribe.get_transcription_job(TranscriptionJobName=job_name)
        status = job["TranscriptionJob"]["TranscriptionJobStatus"]
        if status == "COMPLETED":
            uri = job["TranscriptionJob"]["Transcript"]["TranscriptFileUri"]
            txt = requests.get(uri, timeout=5).json()
            # transcripts[0].transcript 에 전체 문장
            transcript = ""
            try:
                transcript = txt["results"]["transcripts"][0]["transcript"]
            except:
                transcript = ""
            return transcript.strip()
        if status == "FAILED":
            reason = job["TranscriptionJob"].get("FailureReason", "Unknown")
            raise RuntimeError(f"Transcribe Failed: {reason}")
        time.sleep(1)

    raise TimeoutError("Transcribe Timeout")

def _voice_to_spray(transcript: str):
    """
    텍스트에서 키워드 매칭 -> (spray_code, duration_sec, result_text)
    필요하면 너가 키워드 계속 추가하면 됨.
    """
    t = (transcript or "").strip().lower()

    # 기본값
    spray_code = 0
    duration = 3
    result_text = f"VOICE: {transcript}"

    # 아주 단순한 키워드 룰(원하면 더 늘리면 됨)
    # 1: 맑음 / 2: 흐림 / 3: 비 / 4: 눈
    if any(k in t for k in ["맑", "상쾌", "시트러스", "레몬", "오렌지"]):
        spray_code = 1
        result_text = f"맑음(키워드): {transcript}"
    elif any(k in t for k in ["흐림", "구름", "차분", "라벤더", "릴렉스", "편안"]):
        spray_code = 2
        result_text = f"흐림(키워드): {transcript}"
    elif any(k in t for k in ["비", "강수", "레인", "우울", "진정", "바다", "아쿠아"]):
        spray_code = 3
        result_text = f"비(키워드): {transcript}"
    elif any(k in t for k in ["눈", "스노우", "겨울", "민트", "쿨", "시원"]):
        spray_code = 4
        result_text = f"눈(키워드): {transcript}"
    elif any(k in t for k in ["정지", "스톱", "멈춰", "꺼", "중지"]):
        spray_code = 0
        duration = 0
        result_text = f"정지(키워드): {transcript}"

    # 길이 키워드(선택)
    # 예: "10초" "5초" 같은 표현
    for sec in [1, 2, 3, 5, 10, 15]:
        if f"{sec}초" in t:
            duration = sec
            break

    return spray_code, duration, result_text

# ========================================================
# ★ 메인 핸들러
# ========================================================
def lambda_handler(event, context):
    logger.info("============== [REQUEST START] ==============")

    # ------------------------------------------------------
    # [추가] 0) Content-Type이 audio/wav면 "음성 모드"로 처리
    # ------------------------------------------------------
    headers = event.get("headers") or {}
    content_type = _get_header(headers, "content-type") or ""

    # 일부 환경에서 "audio/wav; charset=binary" 처럼 올 수 있어 contains 처리
    if "audio/wav" in str(content_type).lower():

        # =========================================================
        # [수정] STT 블록 전체 try/except로 감싸서 500 방지 + 원인 반환
        # =========================================================
        try:
            # 필수 환경변수
            bucket = os.environ.get("AUDIO_BUCKET", "")
            if not bucket:
                return {
                    "statusCode": 500,
                    "headers": {"Content-Type": "application/json"},
                    "body": json.dumps({"spray": 0, "result_text": "VOICE_BUCKET missing"}, ensure_ascii=False)
                }

            # body는 base64로 올 가능성 큼
            raw_body = event.get("body", "") or ""
            is_b64 = bool(event.get("isBase64Encoded", False))

            try:
                wav_bytes = base64.b64decode(raw_body) if is_b64 else raw_body.encode("latin1")
            except Exception as e:
                logger.error(f"Voice decode error: {e}")
                return {
                    "statusCode": 400,
                    "headers": {"Content-Type": "application/json"},
                    "body": json.dumps({"spray": 0, "result_text": "VOICE decode error"}, ensure_ascii=False)
                }

            # device_id 추정
            device_id = _guess_device_id(event, {})

            # S3 업로드
            key = f"voice/{device_id}/{datetime.now(timezone(timedelta(hours=9))).strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex}.wav"
            try:
                s3.put_object(
                    Bucket=bucket,
                    Key=key,
                    Body=wav_bytes,
                    ContentType="audio/wav"
                )
            except Exception as e:
                logger.error(f"S3 put_object error: {e}")
                return {
                    "statusCode": 500,
                    "headers": {"Content-Type": "application/json"},
                    "body": json.dumps({"spray": 0, "result_text": "S3 upload failed"}, ensure_ascii=False)
                }

            # Transcribe 실행
            try:
                transcript = _transcribe_wav_from_s3(bucket, key, language_code=os.environ.get("VOICE_LANG", "ko-KR"))
            except Exception as e:
                logger.error(f"Transcribe error: {e}")
                return {
                    "statusCode": 200,
                    "headers": {"Content-Type": "application/json"},
                    "body": json.dumps({
                        "spray": 0,
                        "duration": 0,
                        "result_text": "VOICE_STT_FAIL",
                        "message": "음성 인식 실패"
                    }, ensure_ascii=False)
                }

            spray_code, duration, result_text = _voice_to_spray(transcript)

            # 음성 로그 저장(선택)
            try:
                now = datetime.now(timezone(timedelta(hours=9)))
                log_table.put_item(Item={
                    "deviceId": device_id,
                    "timestamp": now.isoformat(),
                    "mode": "Voice_Mode",
                    "result_text": result_text,
                    "spray_code": int(spray_code),
                    "duration": int(duration),
                    "region": "Voice",
                    "weight_g": Decimal("0"),
                    "voice_text": transcript,
                    "voice_s3_key": key
                })
            except Exception as e:
                logger.warning(f"Voice log save failed: {e}")

            return {
                "statusCode": 200,
                "headers": {"Content-Type": "application/json"},
                "body": json.dumps({
                    "spray": int(spray_code),
                    "duration": int(duration),
                    "result_text": result_text,
                    "message": result_text,
                    "mode": "Voice_Mode",
                    "voice_text": transcript
                }, ensure_ascii=False)
            }

        except Exception as e:
            # 어떤 예외가 나도 Lambda가 죽지 않게(500 방지)
            logger.error(f"[VOICE_FATAL] {e}")
            return {
                "statusCode": 200,
                "headers": {"Content-Type": "application/json"},
                "body": json.dumps({
                    "spray": 0,
                    "duration": 0,
                    "result_text": "VOICE_ERROR",
                    "message": "VOICE_ERROR",
                    "error": str(e)
                }, ensure_ascii=False)
            }

    # ------------------------------------------------------
    # [이 아래는 너 원본 JSON 처리 로직 그대로]
    # ------------------------------------------------------
    try:
        body = json.loads(event.get("body", "{}"))
    except:
        body = {}

    device_id = body.get("device", "ESP32_Test")
    if not device_id: device_id = body.get("deviceId", "App_User") # deviceId 필드도 확인

    mode = body.get("mode", "weather")
    region = body.get("region", "")
    action = body.get("action", "") # [추가] 액션 확인

    if action == "POLL":
        # 명령(spray_code)과 지역(target_region)을 같이 받아옴
        spray_code, target_region = manage_mailbox(device_id)
        
        if spray_code > 0:
            result_text = f"예약명령 실행({spray_code})"
            if target_region:
                result_text += f"/{target_region}"
            logger.info(f"[POLL] ESP32에게 전달: {spray_code}, {target_region}")
        else:
            result_text = "명령없음"
            target_region = ""

        return {
            "statusCode": 200,
            "headers": {"Content-Type": "application/json"},
            "body": json.dumps({
                "spray": spray_code,
                "target_region": target_region, # ★ ESP32로 지역 정보 전송
                "result_text": result_text,
                "message": result_text
            }, ensure_ascii=False)
        }

    # [수정된 로직 2] 앱이 "수동 분사해!" 명령할 때 (MANUAL)
    elif action == "MANUAL":
        try:
            spray_code = int(body.get("sprayNum", 1))
            region_req = body.get("region", "") # 앱이 보낸 지역 받기
            
            # 우체통에 명령 + 지역 함께 저장
            manage_mailbox(device_id, spray_code, region_req)
            
            result_text = f"수동분사 {spray_code}번 예약됨"
            logger.info(f"[APP] 수동 명령 저장: {spray_code} ({region_req})")
        except:
            spray_code = 0
            result_text = "명령 오류"

        return {
            "statusCode": 200,
            "headers": {"Content-Type": "application/json"},
            "body": json.dumps({
                "spray": spray_code,
                "result_text": result_text,
                "message": result_text
            }, ensure_ascii=False)
        }

    # ======================================================================
    # [기존 로직 유지] 날씨 모드, 감성 모드, 테스트 모드 등
    # ======================================================================

    # ===================== [TEST WEATHER PARSER] =====================
    forced_weather = None
    if isinstance(region, str) and region.startswith("테스트"):
        forced_weather = region.replace("테스트", "").strip()
        logger.info(f"[TEST MODE] Forced weather = {forced_weather}")
    # =================================================================

    # ESP32 CH4 로드셀 g 값 수신
    weight_g = body.get("w4", 0)
    if weight_g is None:
        weight_g = 0

    # ----------------------------------------------------
    # 1. 향 로직 계산
    # ----------------------------------------------------
    spray_code = 0
    result_text = ""
    logic_name = ""
    duration = 0
    temp = "0"
    humidity = "0"

    if mode == "emotion":
        spray_code, result_text, logic_name, duration = logic_emotion_mode(
            body.get("user_emotion", "1")
        )
    else:
        # ===================== [TEST WEATHER OVERRIDE] =====================
        if forced_weather:
            weather_res = forced_weather
            temp = "0"
            humidity = "0"
            spray_code, result_text, logic_name, duration = logic_weather_mode(
                weather_res, humidity
            )
        # ===================================================================
        else:
            if "SERVICE_KEY" not in os.environ:
                return {"statusCode": 500, "body": "API Key Error"}

            # [수정] 앱이 지역을 안 보내면 기본값 '서울'로 처리하여 에러 방지
            if not region:
                region = "서울"

            if region not in REGION_COORDS:
                logger.warning(f"Unknown Region: {region}, Fallback to Seoul")
                nx, ny = REGION_COORDS["서울"]["nx"], REGION_COORDS["서울"]["ny"]
            else:
                nx = REGION_COORDS[region]["nx"]
                ny = REGION_COORDS[region]["ny"]

            bd, bt = get_kma_time()
            params = {
                "serviceKey": os.environ["SERVICE_KEY"],
                "pageNo": "1",
                "numOfRows": "10",
                "dataType": "XML",
                "base_date": bd,
                "base_time": bt,
                "nx": nx,
                "ny": ny
            }
            temp, weather_res, humidity = forecast(params)
            spray_code, result_text, logic_name, duration = logic_weather_mode(
                weather_res, humidity
            )

    if spray_code > 0:
        manage_mailbox(device_id, spray_code)
        logger.info(f"[{mode}] 결정된 명령 {spray_code}번 -> 우체통 저장 완료")
    # ----------------------------------------------------
    # 2. 상태 확인
    # ----------------------------------------------------
    state = get_device_state(device_id)
    if not state:
        state = {}

    last_time_str = state.get('last_spray_time', "2000-01-01T00:00:00")
    last_spray_code = int(state.get('last_spray_code', 0))
    current_capacity = float(state.get('current_capacity', MAX_CAPACITY))

    now = datetime.now(timezone(timedelta(hours=9)))

    # ----------------------------------------------------
    # 3. 쿨타임 판별
    # ----------------------------------------------------
    is_blocked = False
    log_msg = ""

    if spray_code != last_spray_code:
        try:
            last_time = datetime.fromisoformat(last_time_str)
            diff_minutes = (now - last_time).total_seconds() / 60
            if diff_minutes < COOLDOWN_MINUTES:
                is_blocked = True
                remaining = int(COOLDOWN_MINUTES - diff_minutes)
                log_msg = f"[쿨타임] 향기 변경 불가 ({remaining}분 남음)"
                logger.warning(log_msg)
        except:
            pass

    # ----------------------------------------------------
    # 4. 로그 시각화
    # ----------------------------------------------------
    final_log = {
        "deviceId": device_id,
        "timestamp": now.isoformat(),
        "mode": logic_name,
        "inputs": {
            "region": region,
            "weather": result_text,
            "temp": temp,
            "humidity": humidity,
            "weight_g": weight_g
        } if mode == "weather" else {
            "emotion": body.get("user_emotion"),
            "weight_g": weight_g
        },
        "target_output": {
            "spray_code": spray_code,
            "reason": result_text
        },
        "status": "BLOCKED" if is_blocked else "SUCCESS",
        "block_reason": log_msg if is_blocked else "N/A"
    }
    logger.info(f"FINAL_RESULT: {json.dumps(final_log, ensure_ascii=False)}")

    # ----------------------------------------------------
    # 5. 차단 처리
    # ----------------------------------------------------
    if is_blocked:
        return {
            "statusCode": 200,
            "headers": {"Content-Type": "application/json"},
            "body": json.dumps({
                "spray": 0,
                "result_text": "WAIT",
                "message": log_msg,
                "mode": "CoolDown"
            }, ensure_ascii=False)
        }

    # ----------------------------------------------------
    # 6. 상태 업데이트
    # ----------------------------------------------------
    usage = duration * CONSUMPTION_PER_SEC
    new_capacity = current_capacity - usage
    if new_capacity < 0:
        new_capacity = 0.0
    new_capacity = round(new_capacity, 2)

    if spray_code > 0:
        update_device_state(
            device_id,
            now.isoformat(),
            new_capacity,
            spray_code,
            weight_g
        )

    # ----------------------------------------------------
    # 7. DiffuserLog 저장
    # ----------------------------------------------------
    try:
        log_table.put_item(Item={
            "deviceId": device_id,
            "timestamp": now.isoformat(),
            "mode": logic_name,
            "result_text": result_text,
            "spray_code": spray_code,
            "duration": duration,
            "region": region if mode == "weather" else "Emotion",
            "weight_g": Decimal(str(weight_g))
        })
    except:
        pass

    # ----------------------------------------------------
    # 8. 응답
    # ----------------------------------------------------
    return {
        "statusCode": 200,
        "headers": {"Content-Type": "application/json"},
        "body": json.dumps({
            "spray": spray_code,
            "result_text": result_text,
            "duration": duration,
            "remaining_capacity": new_capacity,
            "message": f"분사 성공! ({result_text})",
            "mode": logic_name
        }, ensure_ascii=False)
    }
