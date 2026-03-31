package com.example.diffuser3;

import android.os.Bundle;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class AlarmRingActivity extends AppCompatActivity {
    private String email, region;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 화면 켜기 권한 유지
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON |
                WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD |
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

        setContentView(R.layout.activity_alarm_ring);

        email = getIntent().getStringExtra("email");
        int scent = getIntent().getIntExtra("scent", 1);
        region = getIntent().getStringExtra("region");

        TextView txtAlarmInfo = findViewById(R.id.txtAlarmInfo);
        txtAlarmInfo.setText(scent + "번 향기 알람 작동 중!\n(기본 볼륨으로 음악이 재생됩니다)");

        // ★ 화면이 켜지면 볼륨업 반복 없이 딱 한 번만 시작 명령을 보냅니다!
        NetworkHelper.sendData(email, Constants.ACTION_MANUAL, scent, region, new NetworkHelper.NetworkCallback() {
            @Override public void onSuccess(int c, String m) {}
            @Override public void onFailure(String e) {}
        });

        Button btnDismiss = findViewById(R.id.btnDismissAlarm);
        btnDismiss.setOnClickListener(v -> {
            // 해제 버튼 누르면 정지 명령(90) 쏘고 종료
            NetworkHelper.sendData(email, "MENU_STOP", 90, region, new NetworkHelper.NetworkCallback() {
                @Override
                public void onSuccess(int c, String m) {
                    Toast.makeText(AlarmRingActivity.this, "알람이 해제되었습니다.", Toast.LENGTH_SHORT).show();
                    finish();
                }
                @Override
                public void onFailure(String e) {
                    Toast.makeText(AlarmRingActivity.this, "해제 실패 (다시 눌러주세요)", Toast.LENGTH_SHORT).show();
                }
            });
        });
    }

    @Override
    public void onBackPressed() {
        // 스마트폰 뒤로가기 버튼으로 끄기 방지
    }
}