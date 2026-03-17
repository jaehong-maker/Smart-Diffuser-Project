package com.example.diffuser3;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class SignupActivity extends AppCompatActivity {

    // UI 컴포넌트 변수
    EditText edtSignupEmail, edtSignupPassword, edtSignupPasswordConfirm, edtSignupRegion, edtAuthCode;
    Button btnDoSignup, btnSendAuth;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_signup);

        // 1. UI 요소 연결
        edtSignupEmail = findViewById(R.id.edtSignupEmail);
        edtAuthCode = findViewById(R.id.edtAuthCode); // 추가된 인증번호 입력창
        edtSignupPassword = findViewById(R.id.edtSignupPassword);
        edtSignupPasswordConfirm = findViewById(R.id.edtSignupPasswordConfirm);
        edtSignupRegion = findViewById(R.id.edtSignupRegion);

        btnDoSignup = findViewById(R.id.btnDoSignup);
        btnSendAuth = findViewById(R.id.btnSendAuth); // 추가된 인증번호 받기 버튼

        // 2. [인증번호 받기] 버튼 클릭 리스너
        btnSendAuth.setOnClickListener(v -> {
            String email = edtSignupEmail.getText().toString().trim();

            if (email.isEmpty()) {
                Toast.makeText(this, "인증번호를 받을 이메일을 입력해주세요.", Toast.LENGTH_SHORT).show();
                return;
            }

            Toast.makeText(this, "인증번호를 전송 중입니다...", Toast.LENGTH_SHORT).show();

            // 서버에 인증번호 발송 요청 (액션: SEND_AUTH)
            // 파라미터: 액션, 이메일, 비번(공백), 지역(공백), 인증번호(공백), 콜백
            NetworkHelper.sendUserInfo("SEND_AUTH", email, "", "", "", new NetworkHelper.NetworkCallback() {
                @Override
                public void onSuccess(int sprayCode, String message) {
                    // 서버 응답: "인증번호가 발송되었습니다..."
                    Toast.makeText(SignupActivity.this, message, Toast.LENGTH_LONG).show();
                }

                @Override
                public void onFailure(String errorMsg) {
                    Toast.makeText(SignupActivity.this, "발송 실패: " + errorMsg, Toast.LENGTH_SHORT).show();
                }
            });
        });

        // 3. [가입 완료하기] 버튼 클릭 리스너
        btnDoSignup.setOnClickListener(v -> {
            String email = edtSignupEmail.getText().toString().trim();
            String authCode = edtAuthCode.getText().toString().trim();
            String password = edtSignupPassword.getText().toString().trim();
            String passwordConfirm = edtSignupPasswordConfirm.getText().toString().trim();
            String region = edtSignupRegion.getText().toString().trim();

            // 필수 입력값 확인
            if (email.isEmpty() || authCode.isEmpty() || password.isEmpty() || region.isEmpty()) {
                Toast.makeText(this, "모든 정보를 입력해 주세요.", Toast.LENGTH_SHORT).show();
                return;
            }

            // 비밀번호 일치 확인
            if (!password.equals(passwordConfirm)) {
                Toast.makeText(this, "비밀번호가 서로 다릅니다.", Toast.LENGTH_SHORT).show();
                return;
            }

            Toast.makeText(this, "회원가입 처리 중...", Toast.LENGTH_SHORT).show();

            // 서버에 최종 회원가입 요청 (액션: SIGNUP)
            // 파라미터: 액션, 이메일, 비번, 지역, 인증번호, 콜백
            NetworkHelper.sendUserInfo("SIGNUP", email, password, region, authCode, new NetworkHelper.NetworkCallback() {
                @Override
                public void onSuccess(int sprayCode, String message) {
                    Toast.makeText(SignupActivity.this, "가입 완료! 로그인해 주세요.", Toast.LENGTH_LONG).show();
                    finish(); // 가입 성공 시 로그인 화면으로 복귀
                }

                @Override
                public void onFailure(String errorMsg) {
                    // 서버 응답: "인증번호 불일치" 또는 "이미 가입된 계정" 등
                    Toast.makeText(SignupActivity.this, errorMsg, Toast.LENGTH_SHORT).show();
                }
            });
        });
    }
}