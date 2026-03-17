package com.example.diffuser3;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class LoginActivity extends AppCompatActivity {
    EditText edtEmail, edtPassword;
    Button btnLogin, btnGoSignup;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // 자동 로그인
        SharedPreferences prefs = getSharedPreferences("UserPrefs", MODE_PRIVATE);
        if (prefs.contains("email")) {
            startActivity(new Intent(this, MainActivity.class));
            finish();
            return;
        }

        setContentView(R.layout.activity_login);
        edtEmail = findViewById(R.id.edtEmail);
        edtPassword = findViewById(R.id.edtPassword);
        btnLogin = findViewById(R.id.btnLogin);
        btnGoSignup = findViewById(R.id.btnGoSignup);

        // 로그인 버튼
        btnLogin.setOnClickListener(v -> {
            String email = edtEmail.getText().toString().trim();
            String password = edtPassword.getText().toString().trim();

            if (email.isEmpty() || password.isEmpty()) {
                Toast.makeText(this, "이메일과 비밀번호를 입력하세요.", Toast.LENGTH_SHORT).show();
                return;
            }

            Toast.makeText(this, "로그인 중...", Toast.LENGTH_SHORT).show();
// 이메일, 비밀번호, 지역(""), 인증번호("") 순서입니다.
            NetworkHelper.sendUserInfo("LOGIN", email, password, "", "", new NetworkHelper.NetworkCallback() {                @Override
                public void onSuccess(int sprayCode, String message) {
                    String[] parts = message.split("\\|");
                    String region = parts[0];
                    String musicTracks = parts.length > 1 ? parts[1] : "1_6_11_16";
                    String msg = parts.length > 2 ? parts[2] : "성공";

                    prefs.edit()
                            .putString("email", email)
                            .putString("region", region)
                            .putString("music_tracks", musicTracks)
                            .apply();

                    Toast.makeText(LoginActivity.this, msg, Toast.LENGTH_SHORT).show();
                    startActivity(new Intent(LoginActivity.this, MainActivity.class));
                    finish();
                }
                @Override
                public void onFailure(String errorMsg) {
                    Toast.makeText(LoginActivity.this, errorMsg, Toast.LENGTH_SHORT).show();
                }
            });
        });

        // 새로 회원가입하기 버튼 (SignupActivity로 이동)
        btnGoSignup.setOnClickListener(v -> {
            startActivity(new Intent(LoginActivity.this, SignupActivity.class));
        });
    }
}