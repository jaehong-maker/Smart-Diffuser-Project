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

import java.io.IOException;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {

    ImageView imgWeatherIcon;
    TextView txtTempHumidity;
    ProgressBar progressScent1, progressScent2, progressScent3, progressScent4;
    TextView txtScent1Percent, txtScent2Percent, txtScent3Percent, txtScent4Percent;

    Button btnGoToModeSelection;
    Button btnLogout;

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
        btnLogout = findViewById(R.id.btnLogout);

        // 작동 모드 선택 화면으로 이동
        btnGoToModeSelection.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, ModeSelectionActivity.class);
            startActivity(intent);
        });

        // 로그아웃 버튼 동작
        btnLogout.setOnClickListener(v -> {
            SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
            prefs.edit().clear().apply();

            Toast.makeText(MainActivity.this, "로그아웃 되었습니다.", Toast.LENGTH_SHORT).show();
            Intent intent = new Intent(MainActivity.this, LoginActivity.class);
            startActivity(intent);
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

        // 메인 화면 UI 유지를 위해 날씨 가져오기 실행
        fetchWeatherFromLambda();
    }

    private void fetchWeatherFromLambda() {
        txtTempHumidity.setText(currentRegion + " 날씨 읽는 중... 📡");
        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        String userEmail = prefs.getString("email", "");

        NetworkHelper.sendData(userEmail, "AI_WEATHER", 0, currentRegion, new NetworkHelper.NetworkCallback() {
            @Override
            public void onSuccess(int sprayCode, String message) {
                // 1. 메인 화면에 날씨 텍스트와 아이콘은 정상적으로 업데이트 (UI 유지)
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
}