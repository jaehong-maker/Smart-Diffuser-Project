package com.example.diffuser3;

import android.Manifest;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.TimePickerDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.Address;
import android.location.Geocoder;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.location.Priority;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.IOException;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

public class ModeSelectionActivity extends AppCompatActivity {

    LinearLayout layoutMenu, layoutManual, layoutWeather, layoutAlarm, layoutTime;
    LinearLayout layoutSettingMenu, layoutSettingLoadCell, layoutSettingSound, layoutSettingMusic, layoutFeedback, layoutSettingIntensity;

    TextView txtStatus, txtRegion, txtWeatherInfo, txtCurrentScent, txtCurrentIntensity;

    // 알람 모드 (점진적 볼륨업) UI
    TimePicker timePickerAlarm;
    Spinner spinAlarmScent;
    CheckBox chkAlarmMon, chkAlarmTue, chkAlarmWed, chkAlarmThu, chkAlarmFri, chkAlarmSat, chkAlarmSun;
    LinearLayout containerAlarms;

    // 시간 모드 (범위/요일) UI
    Button btnStartTime, btnEndTime;
    int sHour = 8, sMin = 0;
    int eHour = 9, eMin = 0;
    Spinner spinTimeScent;
    CheckBox chkMon, chkTue, chkWed, chkThu, chkFri, chkSat, chkSun;
    LinearLayout containerTimes;

    // 음악 설정 UI (향기별)
    private Spinner spinScent1Music, spinScent2Music, spinScent3Music, spinScent4Music;

    private FusedLocationProviderClient fusedLocationClient;
    private String currentRegion = "서울";
    private String lastContext = "Manual";
    private int currentSprayCode = 0;
    private long lastVolumeClickTime = 0;
    private String userEmail = "";

    // 이스터에그 변수
    private ImageView imgEasterEgg;
    private ImageView imgEasterEgg2;
    private String secretSequence = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        userEmail = prefs.getString("email", "");

        layoutMenu = findViewById(R.id.layoutMenu);
        layoutManual = findViewById(R.id.layoutManual);
        layoutWeather = findViewById(R.id.layoutWeather);
        layoutAlarm = findViewById(R.id.layoutAlarm);
        layoutTime = findViewById(R.id.layoutTime);
        layoutFeedback = findViewById(R.id.layoutFeedback);

        layoutSettingMenu = findViewById(R.id.layoutSettingMenu);
        layoutSettingLoadCell = findViewById(R.id.layoutSettingLoadCell);
        layoutSettingSound = findViewById(R.id.layoutSettingSound);
        layoutSettingMusic = findViewById(R.id.layoutSettingMusic);
        layoutSettingIntensity = findViewById(R.id.layoutSettingIntensity);

        txtStatus = findViewById(R.id.txtStatus);
        txtRegion = findViewById(R.id.txtRegion);
        txtWeatherInfo = findViewById(R.id.txtWeatherInfo);
        txtCurrentScent = findViewById(R.id.txtCurrentScent);
        txtCurrentIntensity = findViewById(R.id.txtCurrentIntensity);

        // 이스터에그 이미지 연결
        imgEasterEgg = findViewById(R.id.imgEasterEgg);
        if (imgEasterEgg != null) imgEasterEgg.setOnClickListener(v -> imgEasterEgg.setVisibility(View.GONE));

        imgEasterEgg2 = findViewById(R.id.imgEasterEgg2);
        if (imgEasterEgg2 != null) imgEasterEgg2.setOnClickListener(v -> imgEasterEgg2.setVisibility(View.GONE));

        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        String[] scents = {"1번: 시트러스", "2번: 라벤더", "3번: 아쿠아", "4번: 민트"};

        // ==========================================
        // 기본 메뉴 네비게이션
        // ==========================================
        View.OnClickListener backToMenuListener = v -> {
            sendCommand("MENU_STOP", 90, currentRegion);
            showLayout("MENU");
            txtStatus.setText("상태: 메뉴 대기 중");
            layoutFeedback.setVisibility(View.GONE);
        };

        findViewById(R.id.btnModeManual).setOnClickListener(v -> { lastContext = "Manual"; showLayout("MANUAL"); });
        findViewById(R.id.btnModeWeather).setOnClickListener(v -> { showLayout("WEATHER"); getCurrentLocation(); });
        findViewById(R.id.btnModeVoice).setOnClickListener(v -> startVoiceRecording());

        findViewById(R.id.btnModeAlarm).setOnClickListener(v -> {
            showLayout("ALARM");
            refreshAlarmList(prefs);
        });

        findViewById(R.id.btnModeTime).setOnClickListener(v -> {
            showLayout("TIME");
            refreshTimeList(prefs);
        });

        findViewById(R.id.btnModeSetting).setOnClickListener(v -> { lastContext = "Setting"; showLayout("SETTING_MENU"); });

        findViewById(R.id.btnStopAll).setOnClickListener(v -> {
            sendCommand("MENU_STOP", 90, currentRegion);
            txtStatus.setText("상태: 🛑 모든 작동 중지");
            txtCurrentScent.setText("대기 중");
            layoutFeedback.setVisibility(View.GONE);
            Toast.makeText(this, "디퓨저 작동을 중지합니다.", Toast.LENGTH_SHORT).show();
        });

        // ==========================================
        // 수동 모드 설정 (이스터에그 포함)
        // ==========================================
        findViewById(R.id.btnScent1).setOnClickListener(v -> { checkEasterEgg("1"); sendCommand(Constants.ACTION_MANUAL, 1, currentRegion); });
        findViewById(R.id.btnScent2).setOnClickListener(v -> { checkEasterEgg("2"); sendCommand(Constants.ACTION_MANUAL, 2, currentRegion); });
        findViewById(R.id.btnScent3).setOnClickListener(v -> { checkEasterEgg("3"); sendCommand(Constants.ACTION_MANUAL, 3, currentRegion); });
        findViewById(R.id.btnScent4).setOnClickListener(v -> { checkEasterEgg("4"); sendCommand(Constants.ACTION_MANUAL, 4, currentRegion); });

        findViewById(R.id.btnBackFromManual).setOnClickListener(backToMenuListener);
        findViewById(R.id.btnBackFromWeather).setOnClickListener(backToMenuListener);

        // 피드백 버튼
        findViewById(R.id.btnLike).setOnClickListener(v -> sendFeedback("good"));
        findViewById(R.id.btnDislike).setOnClickListener(v -> sendFeedback("bad"));

        // ==========================================
        // [4] 알람 모드 세팅 (점진적 소리 커짐)
        // ==========================================
        timePickerAlarm = findViewById(R.id.timePickerAlarm);
        timePickerAlarm.setIs24HourView(false);
        spinAlarmScent = findViewById(R.id.spinAlarmScent);
        spinAlarmScent.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, scents));
        containerAlarms = findViewById(R.id.containerAlarms);

        chkAlarmMon = findViewById(R.id.chkAlarmMon); chkAlarmTue = findViewById(R.id.chkAlarmTue);
        chkAlarmWed = findViewById(R.id.chkAlarmWed); chkAlarmThu = findViewById(R.id.chkAlarmThu);
        chkAlarmFri = findViewById(R.id.chkAlarmFri); chkAlarmSat = findViewById(R.id.chkAlarmSat);
        chkAlarmSun = findViewById(R.id.chkAlarmSun);

        findViewById(R.id.btnSetAlarm).setOnClickListener(v -> setAlarmModeTimer(prefs));
        findViewById(R.id.btnBackFromAlarm).setOnClickListener(backToMenuListener);

        // ==========================================
        // [5] 시간(범위) 모드 세팅
        // ==========================================
        btnStartTime = findViewById(R.id.btnStartTime);
        btnEndTime = findViewById(R.id.btnEndTime);
        updateTimeButtonText(btnStartTime, sHour, sMin, true);
        updateTimeButtonText(btnEndTime, eHour, eMin, false);

        btnStartTime.setOnClickListener(v -> showTimePicker(true, btnStartTime));
        btnEndTime.setOnClickListener(v -> showTimePicker(false, btnEndTime));

        spinTimeScent = findViewById(R.id.spinTimeScent);
        spinTimeScent.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, scents));

        chkMon = findViewById(R.id.chkMon); chkTue = findViewById(R.id.chkTue);
        chkWed = findViewById(R.id.chkWed); chkThu = findViewById(R.id.chkThu);
        chkFri = findViewById(R.id.chkFri); chkSat = findViewById(R.id.chkSat);
        chkSun = findViewById(R.id.chkSun);
        containerTimes = findViewById(R.id.containerTimes);

        findViewById(R.id.btnSetTime).setOnClickListener(v -> setTimeModeTimer(prefs));
        findViewById(R.id.btnBackFromTime).setOnClickListener(backToMenuListener);

        // ==========================================
        // 기타 설정 모드 네비게이션
        // ==========================================
        findViewById(R.id.btnGoLoadCell).setOnClickListener(v -> showLayout("SETTING_LOADCELL"));
        findViewById(R.id.btnGoSound).setOnClickListener(v -> showLayout("SETTING_SOUND"));
        findViewById(R.id.btnGoMusic).setOnClickListener(v -> showLayout("SETTING_MUSIC"));
        findViewById(R.id.btnGoIntensity).setOnClickListener(v -> { showLayout("SETTING_INTENSITY"); updateIntensityText(prefs.getInt("intensity", 2)); });

        View.OnClickListener backToSettingMenuListener = v -> showLayout("SETTING_MENU");
        findViewById(R.id.btnBackToSettingMenu1).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu2).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu3).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu4).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackFromSettingMenu).setOnClickListener(backToMenuListener);

        findViewById(R.id.btnVolMinus).setOnClickListener(v -> sendVolumeCommand(81));
        findViewById(R.id.btnVolPlus).setOnClickListener(v -> sendVolumeCommand(82));
        findViewById(R.id.btnVolMute).setOnClickListener(v -> sendVolumeCommand(80));

        findViewById(R.id.btnIntensity1).setOnClickListener(v -> setIntensity(1));
        findViewById(R.id.btnIntensity2).setOnClickListener(v -> setIntensity(2));
        findViewById(R.id.btnIntensity3).setOnClickListener(v -> setIntensity(3));

        // ==========================================
        // 향기별 음악 설정 로직
        // ==========================================
        spinScent1Music = findViewById(R.id.spinScent1Music);
        spinScent2Music = findViewById(R.id.spinScent2Music);
        spinScent3Music = findViewById(R.id.spinScent3Music);
        spinScent4Music = findViewById(R.id.spinScent4Music);

        String[] arrScent1 = {"1. 볼빨간사춘기 - 여행", "2. 악동 뮤지션 - Love Lee", "3. 아이유 - 바이,썸머", "4. 아이묭 - Ai Wo Tsutaetaidatoka", "5. 비발디 사계 - 봄"};
        String[] arrScent2 = {"6. 최유리 - 숲", "7. 데이먼스이어 - salty", "8. 10CM X 이수현 - 서울의 잠 못 이루는 밤", "9. 정승환 - 언제라도 어디에서라도", "10. 비발디 사계 - 가을"};
        String[] arrScent3 = {"11. 폴킴 - 비", "12. 이클립스 - 소나기", "13. 아이유 - 잠 못 드는 밤 비는 내리고", "14. 에픽하이 - 비 오는날 듣기좋은 노래", "15. 비발디 사계 - 여름"};
        String[] arrScent4 = {"16. EXO - 첫 눈", "17. 아이유 - 겨울잠", "18. 에일리 - 첫 눈처럼 너에게 가겠다", "19. 정승환 - 눈사람", "20. 비발디 사계 - 겨울"};

        spinScent1Music.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrScent1));
        spinScent2Music.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrScent2));
        spinScent3Music.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrScent3));
        spinScent4Music.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrScent4));

        String savedMusic = prefs.getString("music_tracks", "1_6_11_16");
        String[] tracks = savedMusic.split("_");
        if (tracks.length == 4) {
            try {
                spinScent1Music.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[0]) - 1)));
                spinScent2Music.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[1]) - 6)));
                spinScent3Music.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[2]) - 11)));
                spinScent4Music.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[3]) - 16)));
            } catch (Exception e) {
                e.printStackTrace();
            }
        }

        findViewById(R.id.btnSaveMusic).setOnClickListener(v -> {
            int track1 = spinScent1Music.getSelectedItemPosition() + 1;
            int track2 = spinScent2Music.getSelectedItemPosition() + 6;
            int track3 = spinScent3Music.getSelectedItemPosition() + 11;
            int track4 = spinScent4Music.getSelectedItemPosition() + 16;

            String musicData = track1 + "_" + track2 + "_" + track3 + "_" + track4;
            prefs.edit().putString("music_tracks", musicData).apply();

            txtStatus.setText("상태: 📡 음악 설정 저장 중...");
            NetworkHelper.sendData(userEmail, "SAVE_MUSIC", 0, musicData, new NetworkHelper.NetworkCallback() {
                @Override
                public void onSuccess(int sprayCode, String message) {
                    runOnUiThread(() -> {
                        txtStatus.setText("상태: ✅ 음악 설정 완료");
                        Toast.makeText(ModeSelectionActivity.this, "향기별 음악 설정이 저장되었습니다!", Toast.LENGTH_SHORT).show();
                    });
                }
                @Override
                public void onFailure(String errorMsg) {
                    runOnUiThread(() -> {
                        txtStatus.setText("상태: 🚨 통신 실패");
                        Toast.makeText(ModeSelectionActivity.this, "저장 실패: " + errorMsg, Toast.LENGTH_SHORT).show();
                    });
                }
            });
        });

        requestPermissions();
    }

    // ==============================================
    // [4] 알람 모드 로직 (점진적 볼륨업)
    // ==============================================
    private void setAlarmModeTimer(SharedPreferences prefs) {
        int hour = timePickerAlarm.getHour();
        int minute = timePickerAlarm.getMinute();
        int scentCode = spinAlarmScent.getSelectedItemPosition() + 1;
        int alarmId = (int) System.currentTimeMillis();

        boolean[] selectedDays = {false, chkAlarmSun.isChecked(), chkAlarmMon.isChecked(), chkAlarmTue.isChecked(), chkAlarmWed.isChecked(), chkAlarmThu.isChecked(), chkAlarmFri.isChecked(), chkAlarmSat.isChecked()};
        long triggerTime = calculateNextAlarmTime(hour, minute, selectedDays);

        Intent intent = new Intent(this, DiffuserAlarmReceiver.class);
        intent.putExtra("email", userEmail);
        intent.putExtra("scent", scentCode);
        intent.putExtra("region", currentRegion);
        intent.putExtra("alarmId", alarmId);
        intent.putExtra("isStopAction", false);
        intent.putExtra("modeType", "ALARM");

        PendingIntent pi = PendingIntent.getBroadcast(this, alarmId, intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        AlarmManager am = (AlarmManager) getSystemService(Context.ALARM_SERVICE);

        try {
            AlarmManager.AlarmClockInfo info = new AlarmManager.AlarmClockInfo(triggerTime, pi);
            am.setAlarmClock(info, pi);

            String amPm = hour >= 12 ? "오후" : "오전";
            int dispHour = hour > 12 ? hour - 12 : (hour == 0 ? 12 : hour);

            StringBuilder daysStr = new StringBuilder();
            if(chkAlarmMon.isChecked()) daysStr.append("월,"); if(chkAlarmTue.isChecked()) daysStr.append("화,");
            if(chkAlarmWed.isChecked()) daysStr.append("수,"); if(chkAlarmThu.isChecked()) daysStr.append("목,");
            if(chkAlarmFri.isChecked()) daysStr.append("금,"); if(chkAlarmSat.isChecked()) daysStr.append("토,");
            if(chkAlarmSun.isChecked()) daysStr.append("일,");
            String repeatText = daysStr.length() > 0 ? "(" + daysStr.substring(0, daysStr.length()-1) + ") " : "(1회용) ";

            String timeText = String.format("%s %02d:%02d | %s%d번 (점진 알람)", amPm, dispHour, minute, repeatText, scentCode);

            saveToJson(prefs, "alarms_json", alarmId, timeText);
            Toast.makeText(this, "점진적 알람이 등록되었습니다!", Toast.LENGTH_SHORT).show();

            chkAlarmMon.setChecked(false); chkAlarmTue.setChecked(false); chkAlarmWed.setChecked(false);
            chkAlarmThu.setChecked(false); chkAlarmFri.setChecked(false); chkAlarmSat.setChecked(false); chkAlarmSun.setChecked(false);

            refreshAlarmList(prefs);

        } catch (SecurityException e) {
            checkAlarmPermission();
        }
    }

    // ==============================================
    // [5] 시간 모드 로직 (범위/요일 초정밀)
    // ==============================================
    private void showTimePicker(boolean isStart, Button btn) {
        TimePickerDialog dialog = new TimePickerDialog(this, (view, hourOfDay, minute) -> {
            if (isStart) { sHour = hourOfDay; sMin = minute; }
            else { eHour = hourOfDay; eMin = minute; }
            updateTimeButtonText(btn, hourOfDay, minute, isStart);
        }, isStart ? sHour : eHour, isStart ? sMin : eMin, false);
        dialog.show();
    }

    private void updateTimeButtonText(Button btn, int h, int m, boolean isStart) {
        String amPm = h >= 12 ? "오후" : "오전";
        int dispHour = h > 12 ? h - 12 : (h == 0 ? 12 : h);
        String prefix = isStart ? "시작: " : "종료: ";
        btn.setText(String.format("%s%s %02d:%02d", prefix, amPm, dispHour, m));
    }

    private void setTimeModeTimer(SharedPreferences prefs) {
        int scentCode = spinTimeScent.getSelectedItemPosition() + 1;
        int alarmId = (int) System.currentTimeMillis();

        boolean[] selectedDays = {false, chkSun.isChecked(), chkMon.isChecked(), chkTue.isChecked(), chkWed.isChecked(), chkThu.isChecked(), chkFri.isChecked(), chkSat.isChecked()};
        long triggerTime = calculateNextAlarmTime(sHour, sMin, selectedDays);

        Intent intent = new Intent(this, DiffuserAlarmReceiver.class);
        intent.putExtra("email", userEmail);
        intent.putExtra("scent", scentCode);
        intent.putExtra("region", currentRegion);
        intent.putExtra("alarmId", alarmId);
        intent.putExtra("isStopAction", false);
        intent.putExtra("modeType", "TIME");
        intent.putExtra("endHour", eHour);
        intent.putExtra("endMinute", eMin);

        PendingIntent pi = PendingIntent.getBroadcast(this, alarmId, intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        AlarmManager am = (AlarmManager) getSystemService(Context.ALARM_SERVICE);

        try {
            AlarmManager.AlarmClockInfo info = new AlarmManager.AlarmClockInfo(triggerTime, pi);
            am.setAlarmClock(info, pi);

            String sAmPm = sHour >= 12 ? "오후" : "오전"; int sDisp = sHour > 12 ? sHour - 12 : (sHour == 0 ? 12 : sHour);
            String eAmPm = eHour >= 12 ? "오후" : "오전"; int eDisp = eHour > 12 ? eHour - 12 : (eHour == 0 ? 12 : eHour);

            StringBuilder daysStr = new StringBuilder();
            if(chkMon.isChecked()) daysStr.append("월,"); if(chkTue.isChecked()) daysStr.append("화,");
            if(chkWed.isChecked()) daysStr.append("수,"); if(chkThu.isChecked()) daysStr.append("목,");
            if(chkFri.isChecked()) daysStr.append("금,"); if(chkSat.isChecked()) daysStr.append("토,");
            if(chkSun.isChecked()) daysStr.append("일,");
            String repeatText = daysStr.length() > 0 ? "(" + daysStr.substring(0, daysStr.length()-1) + ") " : "(1회용) ";

            String timeText = String.format("%s %02d:%02d ~ %s %02d:%02d | %s%d번", sAmPm, sDisp, sMin, eAmPm, eDisp, eMin, repeatText, scentCode);

            saveToJson(prefs, "times_json", alarmId, timeText);
            Toast.makeText(this, "시간(범위) 예약이 설정되었습니다!", Toast.LENGTH_SHORT).show();

            chkMon.setChecked(false); chkTue.setChecked(false); chkWed.setChecked(false);
            chkThu.setChecked(false); chkFri.setChecked(false); chkSat.setChecked(false); chkSun.setChecked(false);
            refreshTimeList(prefs);

        } catch (SecurityException e) {
            checkAlarmPermission();
        }
    }

    private long calculateNextAlarmTime(int hour, int minute, boolean[] daysOfWeek) {
        Calendar target = Calendar.getInstance();
        target.set(Calendar.HOUR_OF_DAY, hour); target.set(Calendar.MINUTE, minute); target.set(Calendar.SECOND, 0);

        boolean hasDays = false;
        for (boolean d : daysOfWeek) if (d) hasDays = true;

        if (!hasDays) {
            if (target.before(Calendar.getInstance())) target.add(Calendar.DATE, 1);
            return target.getTimeInMillis();
        }

        while (target.before(Calendar.getInstance()) || !daysOfWeek[target.get(Calendar.DAY_OF_WEEK)]) {
            target.add(Calendar.DATE, 1);
        }
        return target.getTimeInMillis();
    }

    // ==============================================
    // 리스트 관리 (JSON)
    // ==============================================
    private void saveToJson(SharedPreferences prefs, String key, int id, String text) {
        try {
            JSONArray array = new JSONArray(prefs.getString(key, "[]"));
            JSONObject obj = new JSONObject();
            obj.put("id", id);
            obj.put("text", text);
            array.put(obj);
            prefs.edit().putString(key, array.toString()).apply();
        } catch (Exception e) { e.printStackTrace(); }
    }

    private void refreshAlarmList(SharedPreferences prefs) {
        drawList(prefs, "alarms_json", containerAlarms, true);
    }
    private void refreshTimeList(SharedPreferences prefs) {
        drawList(prefs, "times_json", containerTimes, false);
    }

    private void drawList(SharedPreferences prefs, String key, LinearLayout container, boolean isAlarmMode) {
        container.removeAllViews();
        try {
            JSONArray array = new JSONArray(prefs.getString(key, "[]"));
            if (array.length() == 0) {
                TextView emptyText = new TextView(this);
                emptyText.setText("저장된 예약이 없습니다.");
                emptyText.setPadding(10, 10, 10, 10);
                container.addView(emptyText);
                return;
            }
            for (int i = 0; i < array.length(); i++) {
                JSONObject obj = array.getJSONObject(i);
                int id = obj.getInt("id");
                String text = obj.getString("text");

                LinearLayout row = new LinearLayout(this);
                row.setOrientation(LinearLayout.HORIZONTAL);
                row.setPadding(10, 15, 10, 15);

                TextView tvInfo = new TextView(this);
                tvInfo.setText(text);
                tvInfo.setTextColor(0xFF333333);
                tvInfo.setTextSize(14f);
                tvInfo.setLayoutParams(new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));

                Button btnDel = new Button(this);
                btnDel.setText("삭제");
                btnDel.setBackgroundColor(0xFFE74C3C);
                btnDel.setTextColor(0xFFFFFFFF);
                btnDel.setOnClickListener(v -> deleteData(id, prefs, key, isAlarmMode));

                row.addView(tvInfo); row.addView(btnDel);
                container.addView(row);
            }
        } catch (Exception e) { e.printStackTrace(); }
    }

    private void deleteData(int alarmId, SharedPreferences prefs, String key, boolean isAlarmMode) {
        AlarmManager am = (AlarmManager) getSystemService(Context.ALARM_SERVICE);

        // 메인 알람 취소
        Intent intent = new Intent(this, DiffuserAlarmReceiver.class);
        PendingIntent pi = PendingIntent.getBroadcast(this, alarmId, intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        am.cancel(pi);

        // 시간 예약 모드일 경우 종료 알람도 취소
        Intent stopIntent = new Intent(this, DiffuserAlarmReceiver.class);
        PendingIntent stopPi = PendingIntent.getBroadcast(this, alarmId + 1, stopIntent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        am.cancel(stopPi);

        try {
            JSONArray array = new JSONArray(prefs.getString(key, "[]"));
            JSONArray newArray = new JSONArray();
            for (int i = 0; i < array.length(); i++) {
                if (array.getJSONObject(i).getInt("id") != alarmId) newArray.put(array.getJSONObject(i));
            }
            prefs.edit().putString(key, newArray.toString()).apply();

            if (isAlarmMode) refreshAlarmList(prefs);
            else refreshTimeList(prefs);

            Toast.makeText(this, "예약이 삭제되었습니다.", Toast.LENGTH_SHORT).show();
        } catch (Exception e) { e.printStackTrace(); }
    }

    private void checkAlarmPermission() {
        Toast.makeText(this, "🚨 앱 설정에서 '정확한 알람 허용' 권한을 켜주세요!", Toast.LENGTH_LONG).show();
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            startActivity(new Intent(android.provider.Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM));
        }
    }

    // ==============================================
    // 기타 기능 및 유틸리티 로직
    // ==============================================
    private void startVoiceRecording() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, 200); return;
        }
        int sampleRate = 16000; int targetSize = sampleRate * 2 * 2;
        AudioRecord audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, Math.max(AudioRecord.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT), targetSize));

        // ★ 추가된 부분: 마이크 초기화 실패 방어
        if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
            Toast.makeText(this, "마이크 초기화 실패! 다른 앱에서 사용 중인지 확인해주세요.", Toast.LENGTH_SHORT).show();
            return;
        }

        byte[] audioData = new byte[targetSize];
        audioRecord.startRecording();
        txtStatus.setText("상태: 🎤 2초간 말씀하세요...");
        Toast.makeText(this, "녹음을 시작합니다 (2초)", Toast.LENGTH_SHORT).show();

        new Thread(() -> {
            int read = 0;
            while (read < targetSize) {
                int result = audioRecord.read(audioData, read, targetSize - read);
                // ★ 수정된 부분: 마이크 읽기 에러 시 무한 루프 탈출
                if (result < 0) {
                    runOnUiThread(() -> txtStatus.setText("상태: 🚨 마이크 오류 발생"));
                    break;
                }
                read += result;
            }
            audioRecord.stop();
            audioRecord.release();

            if (read < targetSize) return; // 녹음이 정상적으로 안 끝났으면 전송 중단

            byte[] wavData = createWavFile(audioData, sampleRate, 1, 16);
            runOnUiThread(() -> txtStatus.setText("상태: 📡 서버로 음성 전송 중..."));

            // ★ 수정된 부분: 이메일과 지역 정보를 파라미터로 함께 전송!
            NetworkHelper.sendAudioData(userEmail, currentRegion, wavData, new NetworkHelper.NetworkCallback() {
                @Override public void onSuccess(int sprayCode, String message) {
                    runOnUiThread(() -> {
                        txtStatus.setText("상태: ✅ " + message); Toast.makeText(ModeSelectionActivity.this, message, Toast.LENGTH_LONG).show();
                        if (sprayCode >= 1 && sprayCode <= 4) { currentSprayCode = sprayCode; txtCurrentScent.setText("💨 분사 중: " + getScentName(sprayCode)); layoutFeedback.setVisibility(View.VISIBLE); }
                    });
                }
                @Override public void onFailure(String errorMsg) { runOnUiThread(() -> { txtStatus.setText("상태: 🚨 음성 처리 실패 (" + errorMsg + ")"); Toast.makeText(ModeSelectionActivity.this, errorMsg, Toast.LENGTH_SHORT).show(); }); }
            });
        }).start();
    }
    private byte[] createWavFile(byte[] pcmData, int sampleRate, int channels, int bitsPerSample) {
        int totalDataLen = pcmData.length + 36; int byteRate = sampleRate * channels * bitsPerSample / 8; byte[] header = new byte[44];
        header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F'; header[4] = (byte) (totalDataLen & 0xff); header[5] = (byte) ((totalDataLen >> 8) & 0xff); header[6] = (byte) ((totalDataLen >> 16) & 0xff); header[7] = (byte) ((totalDataLen >> 24) & 0xff); header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E'; header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' '; header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; header[20] = 1; header[21] = 0; header[22] = (byte) channels; header[23] = 0; header[24] = (byte) (sampleRate & 0xff); header[25] = (byte) ((sampleRate >> 8) & 0xff); header[26] = (byte) ((sampleRate >> 16) & 0xff); header[27] = (byte) ((sampleRate >> 24) & 0xff); header[28] = (byte) (byteRate & 0xff); header[29] = (byte) ((byteRate >> 8) & 0xff); header[30] = (byte) ((byteRate >> 16) & 0xff); header[31] = (byte) ((byteRate >> 24) & 0xff); header[32] = (byte) (channels * bitsPerSample / 8); header[33] = 0; header[34] = (byte) bitsPerSample; header[35] = 0; header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a'; header[40] = (byte) (pcmData.length & 0xff); header[41] = (byte) ((pcmData.length >> 8) & 0xff); header[42] = (byte) ((pcmData.length >> 16) & 0xff); header[43] = (byte) ((pcmData.length >> 24) & 0xff);
        byte[] wavFile = new byte[44 + pcmData.length]; System.arraycopy(header, 0, wavFile, 0, 44); System.arraycopy(pcmData, 0, wavFile, 44, pcmData.length); return wavFile;
    }

    private void sendFeedback(String rating) {
        int ratingVal = rating.equals("good") ? 1 : 0; txtStatus.setText("상태: 📡 피드백 전송 중...");
        NetworkHelper.sendData(userEmail, Constants.ACTION_FEEDBACK, ratingVal, currentSprayCode + "_" + lastContext, new NetworkHelper.NetworkCallback() {
            @Override public void onSuccess(int newSprayCode, String message) {
                if (rating.equals("bad")) {
                    Toast.makeText(ModeSelectionActivity.this, "다른 향기로 변경합니다 🔄", Toast.LENGTH_SHORT).show();
                    if (newSprayCode >= 1 && newSprayCode <= 4) { currentSprayCode = newSprayCode; txtCurrentScent.setText("💨 분사 중: " + getScentName(newSprayCode)); txtStatus.setText("상태: ✅ " + message); sendCommand(Constants.ACTION_MANUAL, newSprayCode, currentRegion); }
                } else { Toast.makeText(ModeSelectionActivity.this, "반영되었습니다 😊", Toast.LENGTH_SHORT).show(); txtStatus.setText("상태: ✅ " + message); }
            }
            @Override public void onFailure(String errorMsg) { txtStatus.setText("상태: 🚨 통신 실패"); }
        });
        layoutFeedback.setVisibility(View.GONE);
    }

    private void getCurrentLocation() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) { requestPermissions(); return; }
        txtStatus.setText("상태: 📍 위치 분석 중..."); txtRegion.setText("현재 위치 확인 중..."); txtWeatherInfo.setText("날씨 정보를 가져오는 중...");
        fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, null).addOnSuccessListener(this, location -> {
            if (location != null) { currentRegion = getRegionName(location.getLatitude(), location.getLongitude()); txtRegion.setText("📍 현재 지역: " + currentRegion); lastContext = "Weather_" + currentRegion; sendCommand("AI_WEATHER", 0, currentRegion);
            } else { txtRegion.setText("위치 실패 (GPS 확인)"); txtStatus.setText("상태: 🚨 GPS 신호 없음"); }
        });
    }

    private void sendCommand(String action, int value, String data) {
        txtStatus.setText("상태: 📡 서버 요청 중...");
        NetworkHelper.sendData(userEmail, action, value, data, new NetworkHelper.NetworkCallback() {
            @Override public void onSuccess(int realSpray, String message) {
                txtStatus.setText("상태: ✅ 분석 완료"); if (action.equals("AI_WEATHER")) txtWeatherInfo.setText("🌡️ 날씨: " + message);
                if (realSpray >= 1 && realSpray <= 4) { currentSprayCode = realSpray; txtCurrentScent.setText("💨 분사 중: " + getScentName(realSpray)); if (!lastContext.equals("Manual")) layoutFeedback.setVisibility(View.VISIBLE); }
            }
            @Override public void onFailure(String errorMsg) { txtStatus.setText("상태: 🚨 에러 발생"); Toast.makeText(ModeSelectionActivity.this, errorMsg, Toast.LENGTH_SHORT).show(); }
        });
    }

    private void sendVolumeCommand(int volCode) { NetworkHelper.sendData(userEmail, Constants.ACTION_MANUAL, volCode, currentRegion, new NetworkHelper.NetworkCallback() { @Override public void onSuccess(int r, String m) {} @Override public void onFailure(String e) {} }); }

    private void showLayout(String type) {
        layoutMenu.setVisibility(View.GONE); layoutManual.setVisibility(View.GONE); layoutWeather.setVisibility(View.GONE);
        layoutAlarm.setVisibility(View.GONE); layoutTime.setVisibility(View.GONE); layoutSettingMenu.setVisibility(View.GONE);
        layoutSettingLoadCell.setVisibility(View.GONE); layoutSettingSound.setVisibility(View.GONE);
        layoutSettingMusic.setVisibility(View.GONE); layoutFeedback.setVisibility(View.GONE); layoutSettingIntensity.setVisibility(View.GONE);

        if (type.equals("MENU")) layoutMenu.setVisibility(View.VISIBLE); else if (type.equals("MANUAL")) layoutManual.setVisibility(View.VISIBLE); else if (type.equals("WEATHER")) layoutWeather.setVisibility(View.VISIBLE); else if (type.equals("ALARM")) layoutAlarm.setVisibility(View.VISIBLE); else if (type.equals("TIME")) layoutTime.setVisibility(View.VISIBLE); else if (type.equals("SETTING_MENU")) layoutSettingMenu.setVisibility(View.VISIBLE); else if (type.equals("SETTING_LOADCELL")) layoutSettingLoadCell.setVisibility(View.VISIBLE); else if (type.equals("SETTING_SOUND")) layoutSettingSound.setVisibility(View.VISIBLE); else if (type.equals("SETTING_MUSIC")) layoutSettingMusic.setVisibility(View.VISIBLE); else if (type.equals("SETTING_INTENSITY")) layoutSettingIntensity.setVisibility(View.VISIBLE);
    }

    private String getScentName(int code) { switch (code) { case 1: return "시트러스 (맑음)"; case 2: return "라벤더 (흐림)"; case 3: return "아쿠아 (비)"; case 4: return "민트 (눈)"; default: return "대기 중"; } }

    private String getRegionName(double lat, double lng) {
        Geocoder geocoder = new Geocoder(this, Locale.KOREA);
        try { List<Address> addresses = geocoder.getFromLocation(lat, lng, 1); if (addresses != null && !addresses.isEmpty()) return mapToLambdaRegion(addresses.get(0).getAddressLine(0));
        } catch (IOException e) { e.printStackTrace(); } return "서울";
    }

    private String mapToLambdaRegion(String fullAddress) {
        if (fullAddress == null) return "서울";
        String[] regions = {"서울", "수원", "성남", "안양", "고양", "용인", "부천", "안산", "남양주", "화성", "평택", "의정부", "파주", "김포", "광명", "광주", "이천", "양주", "구리", "포천", "양평", "가평", "시흥", "인천", "강원", "충북", "충남", "대전", "세종", "전북", "전남", "대구", "경북", "울릉도", "경남", "부산", "울산", "제주"};
        for (String region : regions) if (fullAddress.contains(region)) return region; return "서울";
    }

    private void requestPermissions() {
        String[] permissions = {Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.RECORD_AUDIO};
        boolean needRequest = false; for (String perm : permissions) if (ActivityCompat.checkSelfPermission(this, perm) != PackageManager.PERMISSION_GRANTED) { needRequest = true; break; }
        if (needRequest) ActivityCompat.requestPermissions(this, permissions, 100);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (System.currentTimeMillis() - lastVolumeClickTime < 300) return true;
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) { lastVolumeClickTime = System.currentTimeMillis(); sendVolumeCommand(81); return true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) { lastVolumeClickTime = System.currentTimeMillis(); sendVolumeCommand(82); return true; }
        return super.onKeyDown(keyCode, event);
    }

    private void setIntensity(int level) {
        getSharedPreferences("UserPrefs", MODE_PRIVATE).edit().putInt("intensity", level).apply(); updateIntensityText(level);
        txtStatus.setText("상태: 📡 강도 설정 전송 중...");
        NetworkHelper.sendData(userEmail, "SET_INTENSITY", level, currentRegion, new NetworkHelper.NetworkCallback() {
            @Override public void onSuccess(int sprayCode, String message) { txtStatus.setText("상태: ✅ 설정 완료"); Toast.makeText(ModeSelectionActivity.this, level + "단계 강도로 설정되었습니다.", Toast.LENGTH_SHORT).show(); }
            @Override public void onFailure(String errorMsg) { txtStatus.setText("상태: 🚨 통신 실패"); Toast.makeText(ModeSelectionActivity.this, "강도 설정 실패: " + errorMsg, Toast.LENGTH_SHORT).show(); }
        });
    }

    private void updateIntensityText(int level) {
        String text = "현재 강도: "; if (level == 1) text += "약 (1단계)"; else if (level == 2) text += "중 (2단계)"; else if (level == 3) text += "강 (3단계)"; txtCurrentIntensity.setText(text);
    }

    // 이스터에그 체크
    private void checkEasterEgg(String number) {
        secretSequence += number;

        if (secretSequence.length() > 10) {
            secretSequence = secretSequence.substring(secretSequence.length() - 4);
        }

        if (secretSequence.endsWith("3313")) {
            Toast.makeText(this, "천 상 강 림", Toast.LENGTH_LONG).show();
            if (imgEasterEgg != null) {
                imgEasterEgg.setVisibility(View.VISIBLE);
                imgEasterEgg.bringToFront();
            }
            secretSequence = "";
        } else if (secretSequence.endsWith("1333")) {
            Toast.makeText(this, "싸패 발견", Toast.LENGTH_LONG).show();
            if (imgEasterEgg2 != null) {
                imgEasterEgg2.setVisibility(View.VISIBLE);
                imgEasterEgg2.bringToFront();
            }
            secretSequence = "";
        }
    }
}