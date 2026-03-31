#DynamoDB 읽기/쓰기 및 기기 상태 관리를 전담하는 파일

import boto3
import random
import logging
from decimal import Decimal
import config

logger = logging.getLogger()

# DynamoDB
dynamodb = boto3.resource('dynamodb')
log_table = dynamodb.Table('DiffuserLog')
state_table = dynamodb.Table('DiffuserState')
pref_table = dynamodb.Table('DiffuserPref') 
user_table = dynamodb.Table('DiffuserUsers')
auth_table = dynamodb.Table('DiffuserAuth')

def get_music_track(device_id, spray_code):
    try:
        res = state_table.get_item(Key={'deviceId': device_id})
        if 'Item' in res and 'music_tracks' in res['Item']:
            # DB에 "1_6_11_16" 형태로 저장된 문자열을 자름
            track_list = str(res['Item']['music_tracks']).split('_')
            if 1 <= spray_code <= 4 and len(track_list) == 4:
                return int(track_list[spray_code - 1])
    except Exception as e:
        logger.error(f"Music Load Error: {e}")
        
    # 데이터가 없거나 에러나면 기본값 반환
    defaults = {1: 1, 2: 6, 3: 11, 4: 16}
    return defaults.get(int(spray_code), int(spray_code))

def save_feedback(device_id, context, scent_code, is_like):
    try:
        resp = pref_table.get_item(Key={'deviceId': device_id, 'context': context})
        item = resp.get('Item', {})
        
        blocked = set(map(int, item.get('blocked_scents', []))) 
        best = int(item.get('best_scent', 0))

        if is_like:
            best = int(scent_code)
            if best in blocked: blocked.remove(best)
        else:
            blocked.add(int(scent_code))
            if best == int(scent_code): best = 0

        pref_table.put_item(Item={
            'deviceId': device_id,
            'context': context,
            'best_scent': best,
            'blocked_scents': list(blocked)
        })
        return True
    except Exception as e:
        logger.error(f"Feedback Save Error: {e}")
        return False

def get_personalized_scent(device_id, context, default_scent):
    try:
        resp = pref_table.get_item(Key={'deviceId': device_id, 'context': context})
        item = resp.get('Item')
        
        if not item: return default_scent, "기본 추천"

        best = int(item.get('best_scent', 0))
        blocked = set(map(int, item.get('blocked_scents', [])))

        if best > 0: return best, "사용자 선호(고정)"

        if int(default_scent) in blocked:
            candidates = [s for s in [1, 2, 3, 4] if s not in blocked]
            if candidates:
                new_scent = random.choice(candidates)
                return new_scent, "싫은 향 회피(AI 추천)"
            else:
                return default_scent, "모든 향을 싫어함(기본값)"

        return default_scent, "기본 추천"

    except Exception as e:
        logger.error(f"Personalization Error: {e}")
        return default_scent, "에러(기본값)"

def manage_mailbox(device_id, save_cmd=None, save_region=None, save_duration=None):
    try:
        if save_cmd is not None:
            update_exp = "set pending_cmd = :c"
            exp_vals = {':c': int(save_cmd)}
            if save_region:
                update_exp += ", pending_region = :r"
                exp_vals[':r'] = str(save_region)
            # 분사 시간 저장 (없으면 무시)
            if save_duration is not None:
                update_exp += ", pending_duration = :d"
                exp_vals[':d'] = int(save_duration)

            state_table.update_item(
                Key={'deviceId': device_id},
                UpdateExpression=update_exp,
                ExpressionAttributeValues=exp_vals
            )
            return int(save_cmd)
        else:
            res = state_table.get_item(Key={'deviceId': device_id})
            if 'Item' in res:
                cmd = int(res['Item'].get('pending_cmd', 0))
                saved_region = res['Item'].get('pending_region', "") 
                # 저장된 분사 시간 꺼내기 (기본값 3초)
                saved_duration = int(res['Item'].get('pending_duration', 3))

                if cmd > 0:
                    state_table.update_item(
                        Key={'deviceId': device_id},
                        # 꺼낸 후에는 다시 기본값으로 초기화
                        UpdateExpression="set pending_cmd = :z, pending_region = :e, pending_duration = :d",
                        ExpressionAttributeValues={':z': 0, ':e': "", ':d': 3}
                    )
                    return cmd, saved_region, saved_duration 
            return 0, "", 3 
    except Exception as e:
        logger.error(f"Mailbox Error: {e}")
        return 0, "", 3

def get_device_state(device_id):
    try:
        res = state_table.get_item(Key={'deviceId': device_id})
        if 'Item' in res: return res['Item']
        return {
            'deviceId': device_id,
            'last_spray_time': "2000-01-01T00:00:00",
            'current_capacity': Decimal(str(config.MAX_CAPACITY)),
            'last_spray_code': 0
        }
    except: return None

def update_device_state(device_id, last_time, new_capacity, spray_code, weight_g):
    try:
        state_table.update_item(
            Key={'deviceId': device_id},
            UpdateExpression="set last_spray_time = :t, current_capacity = :c, last_spray_code = :s, last_weight_g = :w",
            ExpressionAttributeValues={
                ':t': last_time,
                ':c': Decimal(str(new_capacity)),
                ':s': int(spray_code),
                ':w': Decimal(str(weight_g))
            }
        )
    except Exception as e:
        logger.error(f"DB Write Error: {e}")
