package com.example.diffuser3;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import java.util.Calendar;

public class DiffuserAlarmReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        String modeType = intent.getStringExtra("modeType");

        if ("ALARM".equals(modeType)) {
            // ★ [알람 모드] 정각이 되면 잠든 화면을 깨우고 빨간 해제 버튼이 있는 AlarmRingActivity를 강제 실행합니다!
            Intent alarmIntent = new Intent(context, AlarmRingActivity.class);
            alarmIntent.putExtras(intent); // 향기 번호, 지역 등 정보 그대로 넘겨주기
            alarmIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            context.startActivity(alarmIntent);
        }
        else {
            // ★ [TIME 모드] (시간 범위 예약 모드 - 기존 로직 그대로 사용)
            boolean isStopAction = intent.getBooleanExtra("isStopAction", false);
            String email = intent.getStringExtra("email");
            String region = intent.getStringExtra("region");
            int scent = intent.getIntExtra("scent", 1);
            int alarmId = intent.getIntExtra("alarmId", 0);

            if (isStopAction) {
                // 종료 시간이 되면 90번 정지 명령 발송!
                NetworkHelper.sendData(email, "MENU_STOP", 90, region, new NetworkHelper.NetworkCallback() {
                    @Override public void onSuccess(int sprayCode, String message) {}
                    @Override public void onFailure(String errorMsg) {}
                });
            } else {
                // 시작 시간이 되면 시작 명령 발송
                NetworkHelper.sendData(email, Constants.ACTION_MANUAL, scent, region, new NetworkHelper.NetworkCallback() {
                    @Override public void onSuccess(int sprayCode, String message) {}
                    @Override public void onFailure(String errorMsg) {}
                });

                // 종료 시간 계산해서 알람 재설정
                int endHour = intent.getIntExtra("endHour", 0);
                int endMinute = intent.getIntExtra("endMinute", 0);
                Calendar stopCal = Calendar.getInstance();
                stopCal.set(Calendar.HOUR_OF_DAY, endHour);
                stopCal.set(Calendar.MINUTE, endMinute);
                stopCal.set(Calendar.SECOND, 0);
                if (stopCal.before(Calendar.getInstance())) stopCal.add(Calendar.DATE, 1);

                Intent stopIntent = new Intent(context, DiffuserAlarmReceiver.class);
                stopIntent.putExtras(intent);
                stopIntent.putExtra("isStopAction", true);
                PendingIntent stopPi = PendingIntent.getBroadcast(context, alarmId + 1, stopIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);

                AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
                try {
                    AlarmManager.AlarmClockInfo info = new AlarmManager.AlarmClockInfo(stopCal.getTimeInMillis(), stopPi);
                    alarmManager.setAlarmClock(info, stopPi);
                } catch (SecurityException e) { e.printStackTrace(); }
            }
        }
    }
}