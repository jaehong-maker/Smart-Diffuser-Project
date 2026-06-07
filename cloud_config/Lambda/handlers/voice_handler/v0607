import os
import json
import base64
import uuid
import logging
from datetime import datetime, timedelta, timezone

import config
import db_utils
import api_utils

logger = logging.getLogger()

def handle_voice(event, device_id):
    headers = event.get("headers") or {}
    content_type = api_utils._get_header(headers, "content-type")
    x_mime = api_utils._get_header(headers, "x-mime-type")
    
    try:
        bucket = os.environ.get("AUDIO_BUCKET", "")
        if not bucket: return {"statusCode": 500, "body": json.dumps({"message": "Server Error: AUDIO_BUCKET not set"})}

        raw_mime = str(x_mime if x_mime else content_type).lower()
        actual_mime = raw_mime.split(";")[0].strip()

        if actual_mime == "audio/mp4":
            actual_mime = "audio/m4a"
        
        audio_format = "wav"
        if "mp4" in actual_mime: audio_format = "mp4"
        elif "webm" in actual_mime: audio_format = "webm"
        elif "m4a" in actual_mime: audio_format = "m4a"
        elif "mpeg" in actual_mime: audio_format = "mp3"

        raw_body = event.get("body", "") or ""
        is_b64 = bool(event.get("isBase64Encoded", False))
        
        if is_b64:
            first_decoded = base64.b64decode(raw_body)
            try:
                wav_bytes = base64.b64decode(first_decoded)
            except:
                wav_bytes = first_decoded
        else:
            try:
                if isinstance(raw_body, str) and (len(raw_body) > 100):
                    if "," in raw_body:
                        raw_body = raw_body.split(",")[1]
                    wav_bytes = base64.b64decode(raw_body)
                else:
                    wav_bytes = raw_body.encode("latin1")
            except Exception as e:
                logger.warning(f"[DECODE_FALLBACK] {e}")
                wav_bytes = raw_body if isinstance(raw_body, bytes) else raw_body.encode("utf-8")
        
        logger.info(f"[AUDIO_SIZE] {len(wav_bytes)} bytes")
        
        key = f"voice/{device_id}/{datetime.now(timezone(timedelta(hours=9))).strftime('%Y%m%d_%H%M%S')}_{uuid.uuid4().hex[:8]}.{audio_format}"
        api_utils.s3.put_object(Bucket=bucket, Key=key, Body=wav_bytes, ContentType=actual_mime)
        
        audio_b64_str = base64.b64encode(wav_bytes).decode('utf-8')
        kind_code, duration, result_text, led_dict, transcript = api_utils.ask_gemini_audio_analysis(audio_b64_str, actual_mime)

        normalized_transcript = (transcript or "").strip()
        logger.info(f"[TRANSCRIPT] {normalized_transcript}")

        if normalized_transcript in ["SILENCE", "FAIL", "ERROR", "TIMEOUT", "인식된 내용 없음", ""]:
            msg = result_text if result_text else "음성을 인식하지 못했습니다."
            if normalized_transcript == "TIMEOUT": msg = "분석 시간이 초과되었습니다."
            return {
                "statusCode": 200,
                "body": json.dumps({
                    "spray": 0,
                    "message": msg,
                    "transcript": normalized_transcript
                }, ensure_ascii=False)
            }
        
        if led_dict and led_dict.get("led_r", -1) >= 0:
            try:
                db_utils.state_table.update_item(
                    Key={'deviceId': device_id},
                    UpdateExpression="set led_r = :r, led_g = :g, led_b = :b, led_br = :br",
                    ExpressionAttributeValues={
                        ':r': led_dict["led_r"], ':g': led_dict["led_g"], ':b': led_dict["led_b"], ':br': led_dict["led_bright"]
                    }
                )
            except Exception as e:
                logger.error(f"Voice LED DB 업데이트 실패: {e}")

        if kind_code == 0:
            spray_code = 0
        else:
            spray_code = db_utils.get_spray_from_kind(device_id, kind_code)
            if not spray_code:
                spray_code = 1

        state = db_utils.get_device_state(device_id) or {}
        last_time_str = state.get('last_spray_time', "2000-01-01T00:00:00")
        now = datetime.now(timezone(timedelta(hours=9)))
        try:
            last_time = datetime.fromisoformat(last_time_str)
            if (now - last_time).total_seconds() < (config.COOLDOWN_MINUTES * 60):
                return {
                    "statusCode": 200,
                    "body": json.dumps({"spray": 0, "message": "쿨타임 중이라 분사할 수 없습니다.", "mode": "CoolDown"}, ensure_ascii=False)
                }
        except: pass

        if spray_code >= 0:
            target_cmd = spray_code if spray_code > 0 else 90
            db_utils.manage_mailbox(device_id, target_cmd, save_duration=duration)
            
            if spray_code > 0:
                current_cap = float(state.get('current_capacity', config.MAX_CAPACITY))
                new_cap = round(max(0.0, current_cap - (duration * config.CONSUMPTION_PER_SEC)), 2)
                db_utils.update_device_state(device_id, now.isoformat(), new_cap, spray_code, [0.0, 0.0, 0.0, 0.0])

        return {
            "statusCode": 200,
            "headers": {"Content-Type": "application/json"},
            "body": json.dumps({
                "spray": int(spray_code),
                "message": f"음성 인식 성공: {result_text}",
                "transcript": normalized_transcript,
                "duration": duration
            }, ensure_ascii=False)
        }

    except Exception as e:
        logger.error(f"[VOICE_FATAL] {e}")
        return {"statusCode": 200, "body": json.dumps({"spray": 0, "message": f"음성 처리 에러: {str(e)}"}, ensure_ascii=False)}
