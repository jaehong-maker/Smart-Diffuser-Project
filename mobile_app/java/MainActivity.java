package com.example.diffuser3;

import android.Manifest;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.location.Address;
import android.location.Geocoder;
import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ProgressBar;
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

public class MainActivity extends AppCompatActivity {

    ImageView imgWeatherIcon;
    TextView txtTempHumidity;
    ProgressBar progressScent1, progressScent2, progressScent3, progressScent4;
    TextView txtScent1Percent, txtScent2Percent, txtScent3Percent, txtScent4Percent;

    Button btnGoToModeSelection;
    Button btnLogout; // ★ 로그아웃 버튼 변수 추가

    private FusedLocationProviderClient fusedLocationClient;
    private String currentRegion = "서울";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main_dashboard);

        // UI 연결
        imgWeatherIcon = findViewById(R.id.imgWeatherIcon);
        txtTempHumidity = findViewById(R.id.txtTempHumidity);
        progressScent1 = findViewById(R.id.progressScent1);
        progressScent2 = findViewById(R.id.progressScent2);
        progressScent3 = findViewById(R.id.progressScent3);
        progressScent4 = findViewById(R.id.progressScent4);
        txtScent1Percent = findViewById(R.id.txtScent1Percent);
        txtScent2Percent = findViewById(R.id.txtScent2Percent);
        txtScent3Percent = findViewById(R.id.txtScent3Percent);
        txtScent4Percent = findViewById(R.id.txtScent4Percent);

        btnGoToModeSelection = findViewById(R.id.btnGoToModeSelection);
        btnLogout = findViewById(R.id.btnLogout); // ★ 로그아웃 버튼 연결

        // 작동 모드 선택 화면으로 이동
        btnGoToModeSelection.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, ModeSelectionActivity.class);
            startActivity(intent);
        });

        // ★ 로그아웃 버튼 동작 추가
        btnLogout.setOnClickListener(v -> {
            // 1. 스마트폰에 저장된 내 로그인 정보(SharedPreferences) 모두 지우기
            SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
            prefs.edit().clear().apply();

            Toast.makeText(MainActivity.this, "로그아웃 되었습니다.", Toast.LENGTH_SHORT).show();

            // 2. 로그인 화면으로 다시 이동
            Intent intent = new Intent(MainActivity.this, LoginActivity.class);
            startActivity(intent);

            // 3. 현재 메인 화면은 종료
            finish();
        });

        fusedLocationClient = LocationServices.getFusedLocationProviderClient(this);
        requestPermission();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // 폰에 저장된(로그인한) 내 지역 정보를 불러옵니다.
        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        currentRegion = prefs.getString("region", "서울");

        fetchWeatherFromLambda();
    }

    private void fetchWeatherFromLambda() {
        txtTempHumidity.setText(currentRegion + " 날씨 읽는 중... 📡");
        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        String userEmail = prefs.getString("email", ""); // 내 이메일 꺼내기

        // 첫 번째 인자에 userEmail 추가
        NetworkHelper.sendData(userEmail, "AI_WEATHER", 0, currentRegion, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int sprayCode, String message) {
                txtTempHumidity.setText(currentRegion + ": " + message);
                updateWeatherIcon(sprayCode);
            }
            @Override
            public void onFailure(String errorMsg) {
                txtTempHumidity.setText("날씨 로딩 실패 🚨");
            }
        });
    }
    private void updateWeatherIcon(int sprayCode) {
        // 서버에서 온 향기 번호에 맞춰 그림 변경
        switch (sprayCode) {
            case 1: imgWeatherIcon.setImageResource(R.drawable.ic_weather_sunny); break;
            case 2: imgWeatherIcon.setImageResource(R.drawable.ic_weather_cloud); break;
            case 3: imgWeatherIcon.setImageResource(R.drawable.ic_weather_rain); break;
            case 4: imgWeatherIcon.setImageResource(R.drawable.ic_weather_snow); break;
            default: imgWeatherIcon.setImageResource(android.R.drawable.ic_menu_gallery); break;
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

        // 람다 서버(REGION_COORDS)에 등록된 모든 지역 리스트
        String[] regions = {
                "서울", "수원", "성남", "안양", "고양", "용인", "부천", "안산",
                "남양주", "화성", "평택", "의정부", "파주", "김포", "광명", "광주",
                "이천", "양주", "구리", "포천", "양평", "가평", "시흥", "인천",
                "강원", "충북", "충남", "대전", "세종", "전북", "전남", "대구",
                "경북", "울릉도", "경남", "부산", "울산", "제주"
        };

        // 내 현재 주소(fullAddress)에 위 지역 이름 중 하나라도 포함되어 있다면 그 이름을 반환
        for (String region : regions) {
            if (fullAddress.contains(region)) {
                return region;
            }
        }

        // 매칭되는 지역이 아무것도 없다면 기본값 서울 반환
        return "서울";
    }

    private void requestPermission() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, 100);
        }
    }
}