package com.example.diffuser3;

import android.os.Handler;
import android.os.Looper;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.json.JSONObject;

public class NetworkHelper {

    private static final String SERVER_URL = Constants.LAMBDA_URL;

    public interface NetworkCallback {
        void onSuccess(int sprayCode, String message);
        void onFailure(String errorMsg);
    }

    // ★ 유저 정보(로그인/회원가입) 전송용
    // ★ 유저 정보(로그인/회원가입) 전송용 (비밀번호 추가됨)
    // ★ 인증번호(authCode) 파라미터가 추가되었습니다.
    public static void sendUserInfo(String action, String email, String password, String region, String authCode, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();
                json.put("action", action);
                json.put("email", email);

                // 회원가입이나 로그인 시에만 비밀번호 포함
                if (!action.equals("SEND_AUTH")) {
                    json.put("password", password);
                }

                if (action.equals("SIGNUP")) {
                    json.put("region", region);
                    json.put("auth_code", authCode); // ★ 서버로 인증번호 전송
                }

                URL url = new URL(SERVER_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json; charset=UTF-8");
                conn.setDoOutput(true);

                OutputStream os = conn.getOutputStream();
                os.write(json.toString().getBytes("UTF-8"));
                os.close();

                int status = conn.getResponseCode();
                BufferedReader br = new BufferedReader(new InputStreamReader(
                        status == 200 ? conn.getInputStream() : conn.getErrorStream(), "UTF-8"));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null) sb.append(line);
                br.close();

                if (status == 200) {
                    JSONObject response = new JSONObject(sb.toString());
                    String msg = response.optString("message", "완료");
                    String result = response.optString("result", "FAIL");

                    if (result.equals("SUCCESS")) {
                        String resRegion = response.optString("region", region);
                        String resMusic = response.optString("music_tracks", "1_6_11_16");
                        new Handler(Looper.getMainLooper()).post(() ->
                                callback.onSuccess(1, resRegion + "|" + resMusic + "|" + msg));
                    } else {
                        new Handler(Looper.getMainLooper()).post(() -> callback.onFailure(msg));
                    }
                } else {
                    new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("서버 에러: " + status));
                }
            } catch (Exception e) {
                new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("통신 오류"));
            }
        }).start();
    }
    // ★ 기기 제어 명령 전송용 (이메일 파라미터 추가)
    public static void sendData(String email, String action, int value, String region, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();
                json.put("email", email); // 누구의 명령인지 이메일 전송

                if (action.equals("AI_WEATHER")) {
                    json.put("mode", "weather"); json.put("region", region); json.put("action", "START"); json.put("spray", 30);
                } else if (action.equals("AI_EMOTION")) {
                    json.put("mode", "emotion"); json.put("user_emotion", region); json.put("action", "START"); json.put("spray", 20);
                } else if (action.equals(Constants.ACTION_FEEDBACK)) {
                    json.put("mode", "feedback"); json.put("action", "FEEDBACK"); json.put("rating", value); json.put("context", region);
                } else if (action.equals("MENU_STOP")) {
                    json.put("mode", "menu"); json.put("action", "STOP_ALL"); json.put("spray", 90);
                } else if (action.equals("SAVE_MUSIC")) {
                    json.put("action", "SAVE_MUSIC"); json.put("region", region); // region 필드에 음악 데이터 탑재
                } else if (action.equals("SET_INTENSITY")) {
                    json.put("mode", "manual");
                    json.put("action", "SET_INTENSITY");
                    json.put("intensity", value); // 1, 2, 3 단계 값
                    // ▲ 여기까지 ▲
                } else {
                    json.put("mode", "manual"); json.put("action", action); json.put("spray", value); json.put("region", region);
                }

                URL url = new URL(SERVER_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json; charset=UTF-8");
                conn.setDoOutput(true);

                OutputStream os = conn.getOutputStream();
                os.write(json.toString().getBytes("UTF-8"));
                os.close();

                int status = conn.getResponseCode();
                BufferedReader br = new BufferedReader(new InputStreamReader(status == 200 ? conn.getInputStream() : conn.getErrorStream(), "UTF-8"));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null) sb.append(line);
                br.close();

                if (status == 200) {
                    JSONObject response = new JSONObject(sb.toString());
                    String msg = response.optString("result_text", response.optString("message", "완료"));
                    int spray = response.optInt("spray", 0);
                    new Handler(Looper.getMainLooper()).post(() -> callback.onSuccess(spray, msg));
                } else {
                    new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("서버 에러: " + status));
                }
            } catch (Exception e) {
                new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("통신 오류: " + e.getMessage()));
            }
        }).start();
    }
}