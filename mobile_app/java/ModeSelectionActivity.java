package com.example.diffuser3;

import android.Manifest;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.Address;
import android.location.Geocoder;
import android.media.AudioManager;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import com.google.android.gms.location.FusedLocationProviderClient;
import com.google.android.gms.location.LocationServices;
import com.google.android.gms.location.Priority;

import java.io.IOException;
import java.util.List;
import java.util.Locale;

import android.widget.Spinner;
import android.widget.ArrayAdapter;

public class ModeSelectionActivity extends AppCompatActivity {

    // 설정 화면 관련 레이아웃들
    LinearLayout layoutMenu, layoutManual, layoutEmotion, layoutWeather;
    LinearLayout layoutSettingMenu, layoutSettingLoadCell, layoutSettingSound, layoutSettingMusic, layoutFeedback, layoutSettingIntensity;

    TextView txtStatus, txtRegion, txtWeatherInfo, txtCurrentScent, txtCurrentScentEmo, txtCurrentIntensity;

    private FusedLocationProviderClient fusedLocationClient;
    private String currentRegion = "서울";
    private String lastContext = "Manual";
    private int currentSprayCode = 0;

    private AudioManager audioManager;
    private Spinner spinSunny, spinCloudy, spinRain, spinSnow;
    private long lastVolumeClickTime = 0;

    // ★ 이메일 변수 선언
    private String userEmail = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // ★ 앱에 로그인된 내 이메일 정보를 불러옵니다.
        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        userEmail = prefs.getString("email", "");

        layoutMenu = findViewById(R.id.layoutMenu);
        layoutManual = findViewById(R.id.layoutManual);
        layoutEmotion = findViewById(R.id.layoutEmotion);
        layoutWeather = findViewById(R.id.layoutWeather);
        layoutFeedback = findViewById(R.id.layoutFeedback);

        // 분리된 설정 화면들 연결
        layoutSettingMenu = findViewById(R.id.layoutSettingMenu);
        layoutSettingLoadCell = findViewById(R.id.layoutSettingLoadCell);
        layoutSettingSound = findViewById(R.id.layoutSettingSound);
        layoutSettingMusic = findViewById(R.id.layoutSettingMusic);
        layoutSettingIntensity = findViewById(R.id.layoutSettingIntensity); // ★ 추가: 강도 설정 화면 연결

        txtStatus = findViewById(R.id.txtStatus);
        txtRegion = findViewById(R.id.txtRegion);
        txtWeatherInfo = findViewById(R.id.txtWeatherInfo);
        txtCurrentScent = findViewById(R.id.txtCurrentScent);
        txtCurrentScentEmo = findViewById(R.id.txtCurrentScentEmo);
        txtCurrentIntensity = findViewById(R.id.txtCurrentIntensity); // ★ 추가: 현재 강도 표시 텍스트

        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this);

        audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        setVolumeControlStream(AudioManager.STREAM_MUSIC);

        // [기본 메뉴 네비게이션]
        View.OnClickListener backToMenuListener = v -> {
            sendCommand("MENU_STOP", 90, currentRegion);
            showLayout("MENU");
            txtStatus.setText("상태: 메뉴 대기 중");
            layoutFeedback.setVisibility(View.GONE);
        };

        findViewById(R.id.btnModeManual).setOnClickListener(v -> { lastContext = "Manual"; showLayout("MANUAL"); });
        findViewById(R.id.btnModeWeather).setOnClickListener(v -> { showLayout("WEATHER"); getCurrentLocation(); });
        findViewById(R.id.btnModeEmotion).setOnClickListener(v -> { lastContext = "Emotion"; showLayout("EMOTION"); });

        // 메인 메뉴 -> 설정 모드 메뉴 진입
        findViewById(R.id.btnModeSetting).setOnClickListener(v -> {
            lastContext = "Setting";
            showLayout("SETTING_MENU");
        });

        // ==============================================
        // ★ 설정 모드 내부 네비게이션
        // ==============================================
        findViewById(R.id.btnGoLoadCell).setOnClickListener(v -> showLayout("SETTING_LOADCELL"));
        findViewById(R.id.btnGoSound).setOnClickListener(v -> showLayout("SETTING_SOUND"));
        findViewById(R.id.btnGoMusic).setOnClickListener(v -> showLayout("SETTING_MUSIC"));

        // ★ 추가: 설정 메뉴에서 '분사 강도 설정' 클릭 시 이동
        findViewById(R.id.btnGoIntensity).setOnClickListener(v -> {
            showLayout("SETTING_INTENSITY");
            // 내 폰에 저장된 강도 불러오기 (기본값 2단계)
            int savedIntensity = getSharedPreferences("UserPrefs", MODE_PRIVATE).getInt("intensity", 2);
            updateIntensityText(savedIntensity);
        });

        // 상세 설정에서 다시 설정 메뉴로 나오는 버튼들
        View.OnClickListener backToSettingMenuListener = v -> showLayout("SETTING_MENU");
        findViewById(R.id.btnBackToSettingMenu1).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu2).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu3).setOnClickListener(backToSettingMenuListener);
        findViewById(R.id.btnBackToSettingMenu4).setOnClickListener(backToSettingMenuListener); // ★ 강도 설정 화면에서 돌아가는 버튼 연결

        // 설정 메뉴에서 아예 최상단 메인 메뉴로 나가는 버튼
        findViewById(R.id.btnBackFromSettingMenu).setOnClickListener(backToMenuListener);

        // ==============================================
        // ★ 소리 설정 동작
        // ==============================================
        findViewById(R.id.btnVolMinus).setOnClickListener(v -> sendVolumeCommand(81));
        findViewById(R.id.btnVolPlus).setOnClickListener(v -> sendVolumeCommand(82));
        findViewById(R.id.btnVolMute).setOnClickListener(v -> sendVolumeCommand(80));

        // ==============================================
        // ★ 음악 설정 동작
        // ==============================================
        spinSunny = findViewById(R.id.spinSunny);
        spinCloudy = findViewById(R.id.spinCloudy);
        spinRain = findViewById(R.id.spinRain);
        spinSnow = findViewById(R.id.spinSnow);

        String[] arrSunny = {
                "1. 볼빨간사춘기 - 여행",
                "2. 악동 뮤지션 - Love Lee",
                "3. 아이유 - 바이,썸머",
                "4. 아이묭 - Ai Wo Tsutaetaidatoka",
                "5. 비발디 사계 - 봄"
        };
        String[] arrCloudy = {
                "6. 최유리 - 숲",
                "7. 데이먼스이어 - salty",
                "8. 10CM X 이수현 - 서울의 잠 못 이루는 밤",
                "9. 정승환 - 언제라도 어디에서라도",
                "10. 비발디 사계 - 가을"
        };
        String[] arrRain = {
                "11. 폴킴 - 비",
                "12. 이클립스 - 소나기",
                "13. 아이유 - 잠 못 드는 밤 비는 내리고",
                "14. 에픽하이 - 비 오는날 듣기좋은 노래",
                "15. 비발디 사계 - 여름"
        };
        String[] arrSnow = {
                "16. EXO - 첫 눈",
                "17. 아이유 - 겨울잠",
                "18. 에일리 - 첫 눈처럼 너에게 가겠다",
                "19. 정승환 - 눈사람",
                "20. 비발디 사계 - 겨울"
        };
        spinSunny.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrSunny));
        spinCloudy.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrCloudy));
        spinRain.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrRain));
        spinSnow.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, arrSnow));

        // ==============================================
        // ★ 내 폰에 저장된 전용 음악 설정을 불러와서 스피너(화면)에 세팅하기!
        // ==============================================
        String savedMusic = prefs.getString("music_tracks", "1_6_11_16");
        String[] tracks = savedMusic.split("_");
        if (tracks.length == 4) {
            try {
                // 배열 순서(0~4)에 맞게 수학적 계산 적용
                spinSunny.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[0]) - 1)));
                spinCloudy.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[1]) - 6)));
                spinRain.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[2]) - 11)));
                spinSnow.setSelection(Math.max(0, Math.min(4, Integer.parseInt(tracks[3]) - 16)));
            } catch (Exception e) { e.printStackTrace(); }
        }
        // ==============================================

        findViewById(R.id.btnSaveMusic).setOnClickListener(v -> {
            int sunnyTrack = spinSunny.getSelectedItemPosition() + 1;
            int cloudyTrack = spinCloudy.getSelectedItemPosition() + 6;
            int rainTrack = spinRain.getSelectedItemPosition() + 11;
            int snowTrack = spinSnow.getSelectedItemPosition() + 16;

            String musicData = sunnyTrack + "_" + cloudyTrack + "_" + rainTrack + "_" + snowTrack;

            // ★ 서버에 저장하기 전에 폰 메모리(SharedPreferences)에도 즉시 업데이트!
            prefs.edit().putString("music_tracks", musicData).apply();

            txtStatus.setText("상태: 📡 음악 설정 저장 중...");
            // ★ 통신 시 userEmail 파라미터 추가
            NetworkHelper.sendData(userEmail, "SAVE_MUSIC", 0, musicData, new NetworkHelper.NetworkCallback() {
                @Override
                public void onSuccess(int sprayCode, String message) {
                    txtStatus.setText("상태: ✅ 음악 설정 완료");
                    Toast.makeText(ModeSelectionActivity.this, "음악 설정이 개인 계정에 저장되었습니다!", Toast.LENGTH_SHORT).show();
                }
                @Override
                public void onFailure(String errorMsg) {
                    txtStatus.setText("상태: 🚨 저장 실패");
                    Toast.makeText(ModeSelectionActivity.this, "저장 실패: " + errorMsg, Toast.LENGTH_SHORT).show();
                }
            });
        });

        // ==============================================
        // ★ 강도 설정 동작
        // ==============================================
        findViewById(R.id.btnIntensity1).setOnClickListener(v -> setIntensity(1));
        findViewById(R.id.btnIntensity2).setOnClickListener(v -> setIntensity(2));
        findViewById(R.id.btnIntensity3).setOnClickListener(v -> setIntensity(3));

        // ==============================================
        // ★ 기존 기능들 유지
        // ==============================================
        findViewById(R.id.btnScent1).setOnClickListener(v -> sendCommand(Constants.ACTION_MANUAL, 1, currentRegion));
        findViewById(R.id.btnScent2).setOnClickListener(v -> sendCommand(Constants.ACTION_MANUAL, 2, currentRegion));
        findViewById(R.id.btnScent3).setOnClickListener(v -> sendCommand(Constants.ACTION_MANUAL, 3, currentRegion));
        findViewById(R.id.btnScent4).setOnClickListener(v -> sendCommand(Constants.ACTION_MANUAL, 4, currentRegion));

        findViewById(R.id.btnBackFromManual).setOnClickListener(backToMenuListener);
        findViewById(R.id.btnBackFromWeather).setOnClickListener(backToMenuListener);
        findViewById(R.id.btnBackFromEmotion).setOnClickListener(backToMenuListener);

        findViewById(R.id.btnLike).setOnClickListener(v -> sendFeedback("good"));
        findViewById(R.id.btnDislike).setOnClickListener(v -> sendFeedback("bad"));

        requestPermission();
    }

    private void sendFeedback(String rating) {
        int ratingVal = rating.equals("good") ? 1 : 0;
        txtStatus.setText("상태: 📡 피드백 전송 중...");

        // ★ 통신 시 userEmail 파라미터 추가
        NetworkHelper.sendData(userEmail, Constants.ACTION_FEEDBACK, ratingVal, currentSprayCode + "_" + lastContext, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int newSprayCode, String message) {
                if (rating.equals("bad")) {
                    Toast.makeText(ModeSelectionActivity.this, "다른 향기로 변경합니다 🔄", Toast.LENGTH_SHORT).show();
                    if (newSprayCode >= 1 && newSprayCode <= 4) {
                        currentSprayCode = newSprayCode;
                        String scentName = getScentName(newSprayCode);
                        txtCurrentScent.setText("💨 분사 중: " + scentName);
                        txtCurrentScentEmo.setText("💨 분사 중: " + scentName);
                        txtStatus.setText("상태: ✅ " + message);
                        sendCommand(Constants.ACTION_MANUAL, newSprayCode, currentRegion);
                    }
                } else {
                    Toast.makeText(ModeSelectionActivity.this, "반영되었습니다 😊", Toast.LENGTH_SHORT).show();
                    txtStatus.setText("상태: ✅ " + message);
                }
            }
            @Override
            public void onFailure(String errorMsg) {
                txtStatus.setText("상태: 🚨 통신 실패");
            }
        });
        layoutFeedback.setVisibility(View.GONE);
    }

    private void getCurrentLocation() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestPermission();
            return;
        }

        txtStatus.setText("상태: 📍 위치 분석 중...");
        txtRegion.setText("현재 위치 확인 중...");
        txtWeatherInfo.setText("날씨 정보를 가져오는 중...");

        fusedLocationClient.getCurrentLocation(Priority.PRIORITY_HIGH_ACCURACY, null)
                .addOnSuccessListener(this, location -> {
                    if (location != null) {
                        currentRegion = getRegionName(location.getLatitude(), location.getLongitude());
                        txtRegion.setText("📍 현재 지역: " + currentRegion);
                        lastContext = "Weather_" + currentRegion;
                        sendCommand("AI_WEATHER", 0, currentRegion);
                    } else {
                        txtRegion.setText("위치 실패 (GPS 확인)");
                        txtStatus.setText("상태: 🚨 GPS 신호 없음");
                    }
                });
    }

    private void sendCommand(String action, int value, String data) {
        txtStatus.setText("상태: 📡 서버 요청 중...");
        // ★ 통신 시 userEmail 파라미터 추가
        NetworkHelper.sendData(userEmail, action, value, data, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int realSpray, String message) {
                txtStatus.setText("상태: ✅ 분석 완료");
                if (action.equals("AI_WEATHER")) {
                    txtWeatherInfo.setText("🌡️ 날씨: " + message);
                }
                if (realSpray >= 1 && realSpray <= 4) {
                    currentSprayCode = realSpray;
                    String scentName = getScentName(realSpray);
                    txtCurrentScent.setText("💨 분사 중: " + scentName);
                    txtCurrentScentEmo.setText("💨 분사 중: " + scentName);

                    if (!lastContext.equals("Manual")) {
                        layoutFeedback.setVisibility(View.VISIBLE);
                    }
                }
            }
            @Override
            public void onFailure(String errorMsg) {
                txtStatus.setText("상태: 🚨 에러 발생");
                Toast.makeText(ModeSelectionActivity.this, errorMsg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void sendVolumeCommand(int volCode) {
        // ★ 통신 시 userEmail 파라미터 추가
        NetworkHelper.sendData(userEmail, Constants.ACTION_MANUAL, volCode, currentRegion, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int realSpray, String message) {}
            @Override
            public void onFailure(String errorMsg) {}
        });
    }

    // ★ 화면 분리에 맞게 업데이트된 레이아웃 관리 함수
    private void showLayout(String type) {
        layoutMenu.setVisibility(View.GONE);
        layoutManual.setVisibility(View.GONE);
        layoutEmotion.setVisibility(View.GONE);
        layoutWeather.setVisibility(View.GONE);
        layoutSettingMenu.setVisibility(View.GONE);
        layoutSettingLoadCell.setVisibility(View.GONE);
        layoutSettingSound.setVisibility(View.GONE);
        layoutSettingMusic.setVisibility(View.GONE);
        layoutFeedback.setVisibility(View.GONE);
        layoutSettingIntensity.setVisibility(View.GONE); // ★ 강도 설정 화면 숨기기 추가

        if (type.equals("MENU")) layoutMenu.setVisibility(View.VISIBLE);
        else if (type.equals("MANUAL")) layoutManual.setVisibility(View.VISIBLE);
        else if (type.equals("EMOTION")) layoutEmotion.setVisibility(View.VISIBLE);
        else if (type.equals("WEATHER")) layoutWeather.setVisibility(View.VISIBLE);
        else if (type.equals("SETTING_MENU")) layoutSettingMenu.setVisibility(View.VISIBLE);
        else if (type.equals("SETTING_LOADCELL")) layoutSettingLoadCell.setVisibility(View.VISIBLE);
        else if (type.equals("SETTING_SOUND")) layoutSettingSound.setVisibility(View.VISIBLE);
        else if (type.equals("SETTING_MUSIC")) layoutSettingMusic.setVisibility(View.VISIBLE);
        else if (type.equals("SETTING_INTENSITY")) layoutSettingIntensity.setVisibility(View.VISIBLE); // ★ 강도 설정 화면 보이기 추가
    }

    private String getScentName(int code) {
        switch (code) {
            case 1: return "시트러스 (맑음)";
            case 2: return "라벤더 (흐림)";
            case 3: return "아쿠아 (비)";
            case 4: return "민트 (눈)";
            default: return "대기 중";
        }
    }

    private String getRegionName(double lat, double lng) {
        Geocoder geocoder = new Geocoder(this, Locale.KOREA);
        try {
            List<Address> addresses = geocoder.getFromLocation(lat, lng, 1);
            if (addresses != null && !addresses.isEmpty()) {
                return mapToLambdaRegion(addresses.get(0).getAddressLine(0));
            }
        } catch (IOException e) { e.printStackTrace(); }
        return "서울";
    }

    private String mapToLambdaRegion(String fullAddress) {
        if (fullAddress == null) return "서울";
        String[] regions = {
                "서울", "수원", "성남", "안양", "고양", "용인", "부천", "안산",
                "남양주", "화성", "평택", "의정부", "파주", "김포", "광명", "광주",
                "이천", "양주", "구리", "포천", "양평", "가평", "시흥", "인천",
                "강원", "충북", "충남", "대전", "세종", "전북", "전남", "대구",
                "경북", "울릉도", "경남", "부산", "울산", "제주"
        };
        for (String region : regions) {
            if (fullAddress.contains(region)) {
                return region;
            }
        }
        return "서울";
    }

    private void requestPermission() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, 100);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (System.currentTimeMillis() - lastVolumeClickTime < 300) {
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
            lastVolumeClickTime = System.currentTimeMillis();
            sendVolumeCommand(81);
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            lastVolumeClickTime = System.currentTimeMillis();
            sendVolumeCommand(82);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    // ★ 추가: 강도 조절 메서드
    private void setIntensity(int level) {
        // 1. 내 폰(SharedPreferences)에 강도 저장
        getSharedPreferences("UserPrefs", MODE_PRIVATE).edit().putInt("intensity", level).apply();
        updateIntensityText(level);

        // 2. 서버로 강도 설정 명령 전송
        txtStatus.setText("상태: 📡 강도 설정 전송 중...");
        NetworkHelper.sendData(userEmail, "SET_INTENSITY", level, currentRegion, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int sprayCode, String message) {
                txtStatus.setText("상태: ✅ 설정 완료");
                Toast.makeText(ModeSelectionActivity.this, level + "단계 강도로 설정되었습니다.", Toast.LENGTH_SHORT).show();
            }
            @Override
            public void onFailure(String errorMsg) {
                txtStatus.setText("상태: 🚨 통신 실패");
                Toast.makeText(ModeSelectionActivity.this, "강도 설정 실패: " + errorMsg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    // ★ 추가: 현재 설정된 강도를 화면에 텍스트로 표시
    private void updateIntensityText(int level) {
        String text = "현재 강도: ";
        if (level == 1) text += "약 (1단계)";
        else if (level == 2) text += "중 (2단계)";
        else if (level == 3) text += "강 (3단계)";
        txtCurrentIntensity.setText(text);
    }
}