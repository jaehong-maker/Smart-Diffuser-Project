package com.example.diffuser3; // 패키지명 확인

import android.os.Handler;
import android.os.Looper;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.json.JSONObject;

public class NetworkHelper {

    // ★ 서버 주소를 본인의 Lambda 주소로 변경하세요!
    private static final String SERVER_URL = "https://tgrwszo3iwurntqeq76s5rro640asnwq.lambda-url.ap-northeast-2.on.aws/";

    public interface NetworkCallback {
        // [수정] 성공 시 '향기 번호(sprayCode)'도 같이 전달받음
        void onSuccess(int sprayCode, String message);
        void onFailure(String errorMsg);
    }

    public static void sendData(String action, int value, String region, NetworkCallback callback) {
        new Thread(() -> {
            try {
                JSONObject json = new JSONObject();

                // 1. 요청 데이터 만들기 (상황에 따라 다르게 포장)
                if (action.equals("AI_WEATHER")) {
                    // 날씨 모드: 액션 없이, 모드와 지역만 보냄 -> 서버 AI가 작동함
                    json.put("mode", "weather");
                    json.put("region", region);
                    json.put("action", "");
                }
                else if (action.equals("AI_EMOTION")) {
                    // 감성 모드: 액션 없이, 모드와 감정값만 보냄 -> 서버 AI가 작동함
                    json.put("mode", "emotion");
                    json.put("user_emotion", region); // region 변수에 감정텍스트("Happy" 등)를 담아 보냄
                    json.put("action", "");
                }
                else if (action.equals(Constants.ACTION_FEEDBACK)) {
                    // 피드백 전송
                    json.put("action", "FEEDBACK");
                    json.put("rating", value); // 1=good, 0=bad
                    json.put("sprayNum", value); // 호환성용
                    // 컨텍스트는 서버가 알아서 하거나 앱이 보내야 함 (여기선 단순화)
                    json.put("context", region);
                }
                else {
                    // 수동 제어 / 기타 명령
                    json.put("action", action); // MANUAL, POLL 등
                    json.put("sprayNum", value);
                    json.put("region", region);
                }

                // 2. 서버 전송
                URL url = new URL(SERVER_URL);
                HttpURLConnection conn = (HttpURLConnection) url.openConnection();
                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json; charset=UTF-8");
                conn.setDoOutput(true);

                OutputStream os = conn.getOutputStream();
                os.write(json.toString().getBytes("UTF-8"));
                os.close();

                // 3. 응답 받기
                int status = conn.getResponseCode();
                BufferedReader br = new BufferedReader(new InputStreamReader(
                        status == 200 ? conn.getInputStream() : conn.getErrorStream(), "UTF-8"));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = br.readLine()) != null) sb.append(line);
                br.close();

                // 4. 결과 분석
                if (status == 200) {
                    JSONObject response = new JSONObject(sb.toString());
                    String msg = response.optString("result_text", "완료");
                    int spray = response.optInt("spray", 0); // ★ 서버가 결정한 향기 번호 받기

                    new Handler(Looper.getMainLooper()).post(() ->
                            callback.onSuccess(spray, msg) // 향기 번호 전달!
                    );
                } else {
                    new Handler(Looper.getMainLooper()).post(() ->
                            callback.onFailure("서버 에러: " + status)
                    );
                }

            } catch (Exception e) {
                e.printStackTrace();
                new Handler(Looper.getMainLooper()).post(() ->
                        callback.onFailure("통신 오류: " + e.getMessage())
                );
            }
        }).start();
    }
}