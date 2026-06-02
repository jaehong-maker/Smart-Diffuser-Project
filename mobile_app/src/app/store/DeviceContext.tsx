import React, { createContext, useContext, useState, useEffect, useRef } from "react";
import { apiSendData, apiPollDeviceState } from "../utils/api";
import { useAuth } from "./AuthContext";

export interface ScentSlot {
  id: number;
  name: string;
  color: string;
  remaining: number;
  weightGrams: number;
}

interface DeviceContextType {
  scentSlots: ScentSlot[];
  updateScentSlot: (id: number, slot: Partial<ScentSlot>) => void;
  refreshDeviceState: () => Promise<void>;
  wifiStrength: "strong" | "medium" | "weak" | "disconnected";
  setWifiStrength: (val: "strong" | "medium" | "weak" | "disconnected") => void;
  volume: number;
  handleVolumeChange: (newVolume: number) => void;
  intensity: number;
  handleIntensityChange: (newLevel: number) => void;
  activeTimerMinutes: number | null;
  handleTimerChange: (minutes: number | null) => void;
  timerEnabled: boolean;
  timerStart: number;
  timerEnd: number;
  updateTimerSettings: (enabled: boolean, start: number, end: number) => Promise<void>;
  currentScent: string;
  isDiffuserOn: boolean;
  setIsDiffuserOn: (val: boolean) => void;
  activeMode: "manual" | "weather" | "voice" | "ai" | "noise" | null;
  setActiveMode: (mode: "manual" | "weather" | "voice" | "ai" | "noise" | null) => void;
  isDeviceActionLoading: boolean;
  sendDeviceData: (action: string, value: number, region?: string, diaryText?: string) => Promise<{ 
    success: boolean; 
    message: string; 
    spray?: number;
    temp?: string;
    weather?: string;
  }>;
  calibrateWeight: () => Promise<boolean>;
  dbLevel: number;
}

const DeviceContext = createContext<DeviceContextType | undefined>(undefined);

export const DeviceProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const { currentUser } = useAuth();
  const [scentSlots, setScentSlots] = useState<ScentSlot[]>([
    { id: 1, name: "시트러스 가든", color: "bg-orange-500", remaining: 0, weightGrams: 0 },
    { id: 2, name: "라벤더 포레스트", color: "bg-purple-500", remaining: 0, weightGrams: 0 },
    { id: 3, name: "오션 브리즈", color: "bg-blue-500", remaining: 0, weightGrams: 0 },
    { id: 4, name: "화이트 머스크", color: "bg-gray-400", remaining: 0, weightGrams: 0 },
  ]);
  const [wifiStrength, setWifiStrength] = useState<"strong" | "medium" | "weak" | "disconnected">("strong");
  const [volume, setVolume] = useState<number>(5);
  const [intensity, setIntensity] = useState<number>(2);
  const [activeTimerMinutes, setActiveTimerMinutes] = useState<number | null>(null);
  
  const [timerEnabled, setTimerEnabled] = useState<boolean>(false);
  const [timerStart, setTimerStart] = useState<number>(9);
  const [timerEnd, setTimerEnd] = useState<number>(22);
  
  const [currentScent, setCurrentScent] = useState<string>("1");
  const [isDiffuserOn, setIsDiffuserOn] = useState<boolean>(false);
  const [activeMode, setActiveMode] = useState<"manual" | "weather" | "voice" | "ai" | "noise" | null>(null);

  const [isDeviceActionLoading, setIsDeviceActionLoading] = useState(false);
  const [dbLevel, setDbLevel] = useState<number>(0);
  
  const lastSentVolumeRef = useRef<number>(5);
  const volumeDebounceRef = useRef<NodeJS.Timeout | null>(null);
  const lastIntensityUpdateTimeRef = useRef<number>(0);

  // 볼륨 변경 시 서버 전송 (하드웨어 제어: 앱은 0~10 사용, 서버 및 펌웨어에서 스케일링)
  const syncVolumeWithHardware = (newVol: number) => {
    const rounded = Math.max(0, Math.min(10, Math.round(newVol)));
    setVolume(rounded);

    if (volumeDebounceRef.current) clearTimeout(volumeDebounceRef.current);
    volumeDebounceRef.current = setTimeout(() => {
      if (currentUser && lastSentVolumeRef.current !== rounded) {
        const dId = currentUser.deviceId || "ESP32_Test";
        console.log(`[Volume] Syncing with Hardware: ${rounded}/10`);
        apiSendData({
          email: currentUser.email,
          action: "SET_VOLUME",
          value: rounded,
          region: currentUser.region || "서울",
          deviceId: dId
        });
        lastSentVolumeRef.current = rounded;
      }
    }, 200);
  };

  const handleVolumeChange = (newVol: number) => {
    syncVolumeWithHardware(newVol);
  };

  const handleIntensityChange = async (newLevel: number) => {
    setIntensity(newLevel);
    lastIntensityUpdateTimeRef.current = Date.now();
    if (currentUser) {
      setIsDeviceActionLoading(true);
      try {
        const dId = currentUser.deviceId || "ESP32_Test";
        const res = await apiSendData({
          email: currentUser.email,
          action: "SET_INTENSITY",
          value: newLevel, 
          region: currentUser.region || "서울",
          deviceId: dId
        });

        if (res.result === "SUCCESS" || res.result_text === "OK") {
           // 성공 시 별도 처리 불필요 (UI는 이미 반영됨)
        }
      } finally {
        setIsDeviceActionLoading(false);
      }
    }
  };

  const updateTimerSettings = async (enabled: boolean, start: number, end: number) => {
    setTimerEnabled(enabled);
    setTimerStart(start);
    setTimerEnd(end);
    
    if (currentUser) {
      setIsDeviceActionLoading(true);
      try {
        const dId = currentUser.deviceId || "ESP32_Test";
        // 서버의 SET_INTENSITY 핸들러가 타이머 설정도 같이 처리하도록 구현됨
        await apiSendData({
          email: currentUser.email,
          action: "SET_INTENSITY",
          value: intensity,
          region: currentUser.region || "서울",
          deviceId: dId,
          timer_enabled: enabled,
          timer_start: start,
          timer_end: end
        });
      } finally {
        setIsDeviceActionLoading(false);
        refreshDeviceState();
      }
    }
  };

  const handleTimerChange = async (minutes: number | null) => {
    setActiveTimerMinutes(minutes);

    if (currentUser) {
      setIsDeviceActionLoading(true);
      try {
        const dId = currentUser.deviceId || "ESP32_Test";
        if (minutes === null) {
          await apiSendData({ email: currentUser.email, action: "MENU_STOP", value: 0, region: "STOP", deviceId: dId });
        } else {
          await apiSendData({ email: currentUser.email, action: "TIMER_START", value: minutes, region: currentScent, deviceId: dId });
        }
      } finally {
        setIsDeviceActionLoading(false);
        refreshDeviceState();
      }
    }
  };

  const refreshDeviceState = React.useCallback(async () => {
    if (!currentUser) return;
    try {
      const dId = currentUser.deviceId || "ESP32_Test";
      const response = await apiPollDeviceState(currentUser.email, dId) as any;
      
      console.log("[refreshDeviceState] API Response:", response);

      // 무게 업데이트
      if (response.weights && Array.isArray(response.weights)) {
        const rawWeights = response.weights_raw || [];
        setScentSlots(prev => prev.map((slot, index) => {
          const weightPercent = response.weights![index];
          let estimatedGrams = (weightPercent / 100) * 75.6 + 19.8;
          const weightGrams = rawWeights[index] !== undefined ? rawWeights[index] : estimatedGrams;
          return { 
            ...slot, 
            remaining: weightPercent !== undefined ? Math.min(100, Math.max(0, Math.round(weightPercent))) : slot.remaining,
            weightGrams: weightGrams
          };
        }));
      }

      // 강도 업데이트
      if (response.intensity !== undefined && (Date.now() - lastIntensityUpdateTimeRef.current > 5000)) {
        setIntensity(response.intensity);
      }

      // 볼륨 업데이트
      if (response.volume !== undefined) {
        setVolume(response.volume);
        lastSentVolumeRef.current = response.volume;
      }

      // 타이머 설정 업데이트
      if (response.timer_enabled !== undefined) setTimerEnabled(response.timer_enabled);
      if (response.timer_start !== undefined) setTimerStart(response.timer_start);
      if (response.timer_end !== undefined) setTimerEnd(response.timer_end);

      // 소음 레벨 업데이트
      if (response.db_level !== undefined) setDbLevel(response.db_level);

      // 현재 향기 정보 및 자동 켜짐 상태 동기화
      if (response.spray !== undefined && response.spray > 0 && response.spray !== 90) {
        setCurrentScent(String(response.spray));
        setIsDiffuserOn(true); // 서버에서 동작 중이면 앱에서도 켜짐으로 표시
      }
    } catch (err) {
      console.error("Failed to refresh device state", err);
    }
  }, [currentUser]);

  const sendDeviceData = React.useCallback(async (action: string, value: number, region?: string, diaryText?: string) => {
    if (!currentUser) return { success: false, message: "로그인이 필요합니다." };
    
    const isFeedback = action === "FEEDBACK";
    const targetRegion = isFeedback ? (currentUser.region || "서울") : (region || currentUser.region || "서울");
    const dataPayload = isFeedback ? region : undefined;
    
    const dId = currentUser.deviceId || "ESP32_Test";
    
    const response = await apiSendData({
      email: currentUser.email,
      action,
      value,
      region: targetRegion,
      deviceId: dId,
      diaryText,
      dataPayload: dataPayload
    });

    if (response.result === "SUCCESS" || response.result_text || response.spray !== undefined) {
      if (response.spray !== undefined && response.spray > 0 && response.spray !== 90) {
        setCurrentScent(String(response.spray));
      } else if (action === "MANUAL") {
        setCurrentScent(String(value));
      }
      setTimeout(() => refreshDeviceState(), 500);
      return { 
        success: true, 
        message: response.result_text || response.message || "명령 전송 완료",
        spray: response.spray,
        temp: response.temp,
        weather: response.weather
      };
    }
    return { success: false, message: response.message || "명령 전송 실패" };
  }, [currentUser, refreshDeviceState]);

  const updateScentSlot = (id: number, slot: Partial<ScentSlot>) => {
    setScentSlots(prev => prev.map(s => s.id === id ? { ...s, ...slot } : s));
  };

  useEffect(() => {
    if (currentUser) {
      refreshDeviceState();
      const interval = setInterval(refreshDeviceState, 3000);
      return () => clearInterval(interval);
    }
  }, [currentUser?.email]);

  const calibrateWeight = async () => { return true; };

  return (
    <DeviceContext.Provider value={{
      scentSlots, updateScentSlot, refreshDeviceState,
      wifiStrength, setWifiStrength,
      volume, handleVolumeChange,
      intensity, handleIntensityChange,
      activeTimerMinutes, handleTimerChange,
      timerEnabled, timerStart, timerEnd, updateTimerSettings,
      currentScent, isDiffuserOn, setIsDiffuserOn, activeMode, setActiveMode,
      isDeviceActionLoading,
      sendDeviceData,
      calibrateWeight,
      dbLevel
    }}>
      {children}
    </DeviceContext.Provider>
  );
};

export const useDevice = () => {
  const context = useContext(DeviceContext);
  if (!context) throw new Error("useDevice must be used within a DeviceProvider");
  return context;
};
