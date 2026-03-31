package com.example.diffuser3;

import android.os.Handler;
import android.os.Looper;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.json.JSONObject;
import java.net.URLEncoder;
public class NetworkHelper {

    private static final String SERVER_URL = Constants.LAMBDA_URL;

    public interface NetworkCallback {
        void onSuccess(int sprayCode, String message);
        void onFailure(String errorMsg);
    }

    public static void sendUserInfo(String action, String email, String password, String region, String authCode, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();
                json.put("action", action);
                json.put("email", email);

                if (!action.equals("SEND_AUTH")) {
                    json.put("password", password);
                }

                if (action.equals("SIGNUP")) {
                    json.put("region", region);
                    json.put("auth_code", authCode);
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

    public static void sendData(String email, String action, int value, String region, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();
                json.put("email", email);

                if (action.equals("AI_WEATHER")) {
                    json.put("mode", "weather"); json.put("region", region);
                    // ★ action="START" 와 spray 강제 주입 삭제 (서버의 분석 결과를 따르도록 함)
                } else if (action.equals("AI_EMOTION")) {
                    json.put("mode", "emotion"); json.put("user_emotion", region);
                    // ★ 여기도 START 와 spray 강제 주입 삭제
                } else if (action.equals(Constants.ACTION_FEEDBACK)) {
                    json.put("mode", "feedback"); json.put("action", "FEEDBACK");
                    json.put("value", value);  // ★ rating -> value 로 수정
                    json.put("data", region);  // ★ context -> data 로 수정
                } else if (action.equals("MENU_STOP")) {
                    json.put("mode", "menu"); json.put("action", "STOP_ALL"); json.put("spray", 90);
                } else if (action.equals("SAVE_MUSIC")) {
                    json.put("action", "SAVE_MUSIC");
                    json.put("music", region); // ★ region -> music 으로 수정
                } else if (action.equals("SET_INTENSITY")) {
                    json.put("mode", "manual");
                    json.put("action", "SET_INTENSITY");
                    json.put("intensity", value);
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

    // ★ 추가된 예약 시간 전송 메서드
    public static void sendTimerData(String email, String action, int sprayCode, int runMinutes, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();
                json.put("email", email);
                json.put("action", action);
                json.put("spray", sprayCode);
                json.put("run_minutes", runMinutes); // 서버가 정지 타이머를 계산할 수 있도록 분단위 전송

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
                    String msg = response.optString("message", "완료");
                    int spray = response.optInt("spray", 0);
                    new Handler(Looper.getMainLooper()).post(() -> callback.onSuccess(spray, msg));
                } else {
                    new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("서버 에러: " + status));
                }
            } catch (Exception e) {
                new Handler(Looper.getMainLooper()).post(() -> callback.onFailure("통신 오류"));
            }
        }).start();
    }

    public static void sendAudioData(String email, String region, byte[] wavData, NetworkCallback callback) {
        new Thread(() -> {
            try {
                URL url = new URL(SERVER_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "audio/wav");

                // 모듈화된 람다가 공통으로 인식할 수 있도록 실제 유저 정보를 헤더에 주입
                conn.setRequestProperty("email", email);
                conn.setRequestProperty("region", URLEncoder.encode(region, "UTF-8"));
                conn.setDoOutput(true);

                OutputStream os = conn.getOutputStream();
                os.write(wavData);
                os.flush();
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
                    String msg = response.optString("message", "음성 처리 완료");
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