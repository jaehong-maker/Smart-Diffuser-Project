import React, { createContext, useContext, useState, useEffect, useRef } from "react";
import { apiPollDeviceState, apiSendData } from "../utils/api";
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
  ledColor: string;
  ledBrightness: number;
  isLedOn: boolean;
  activeMode: "manual" | "weather" | "voice" | "ai" | "noise" | null;
  setActiveMode: (mode: "manual" | "weather" | "voice" | "ai" | "noise" | null) => void;
  isDeviceActionLoading: boolean;
  sendDeviceData: (action: string, value: number, region?: string, diaryText?: string, ledData?: { r: number, g: number, b: number, br: number }) => Promise<any>;
  calibrateWeight: () => Promise<boolean>;
  dbLevel: number;
  dbAvg: number;
  dbStdDev: number;
  noiseType: string;
  dbChange: number;
  deviceStatus: string;
}

const DeviceContext = createContext<DeviceContextType | undefined>(undefined);

export const DeviceProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const { currentUser } = useAuth();
  
  const [scentSlots, setScentSlots] = useState<ScentSlot[]>([
    { id: 1, name: "시트러스", color: "bg-orange-500", remaining: 0, weightGrams: 0 },
    { id: 2, name: "센달우드", color: "bg-amber-700", remaining: 0, weightGrams: 0 },
    { id: 3, name: "패츌리", color: "bg-green-700", remaining: 0, weightGrams: 0 },
    { id: 4, name: "페퍼민트", color: "bg-teal-500", remaining: 0, weightGrams: 0 },
  ]);

  const [wifiStrength, setWifiStrength] = useState<"strong" | "medium" | "weak" | "disconnected">("disconnected");
  const [volume, setVolume] = useState<number>(5);
  const [intensity, setIntensity] = useState<number>(2);
  const [activeTimerMinutes, setActiveTimerMinutes] = useState<number | null>(null);
  
  const [timerEnabled, setTimerEnabled] = useState<boolean>(false);
  const [timerStart, setTimerStart] = useState<number>(9);
  const [timerEnd, setTimerEnd] = useState<number>(22);
  
  const [currentScent, setCurrentScent] = useState<string>("1");
  const [isDiffuserOn, setIsDiffuserOn] = useState<boolean>(false);
  const [ledColor, setLedColor] = useState<string>("#FBBF24");
  const [ledBrightness, setLedBrightness] = useState<number>(80);
  const [isLedOn, setIsLedOn] = useState<boolean>(false);
  const [activeMode, setActiveMode] = useState<"manual" | "weather" | "voice" | "ai" | "noise" | null>(null);

  const [isDeviceActionLoading, setIsDeviceActionLoading] = useState(false);
  const [dbLevel, setDbLevel] = useState<number>(0);
  const [dbAvg, setDbAvg] = useState<number>(0);
  const [dbStdDev, setDbStdDev] = useState<number>(0);
  const [noiseType, setNoiseType] = useState<string>("분석 전");
  const [dbChange, setDbChange] = useState<number>(0);
  const [deviceStatus, setDeviceStatus] = useState<string>("대기 중");
  
  const lastIntensityUpdateTimeRef = useRef<number>(0);
  const lastVolumeUpdateTimeRef = useRef<number>(0);
  const lastLedUpdateTimeRef = useRef<number>(0);
  const lastDbLevelRef = useRef<number>(0);

  const handleVolumeChange = (newVol: number) => {
    const rounded = Math.max(0, Math.min(10, Math.round(newVol)));
    setVolume(rounded);
    lastVolumeUpdateTimeRef.current = Date.now();

    if (currentUser) {
        const dId = currentUser.deviceId || "ESP32_Test";
        apiSendData({
          email: currentUser.email,
          action: "SET_VOLUME",
          value: rounded,
          region: currentUser.region || "서울",
          deviceId: dId
        });
    }
  };

  const handleIntensityChange = async (newLevel: number) => {
    setIntensity(newLevel);
    lastIntensityUpdateTimeRef.current = Date.now();
    if (currentUser) {
      setIsDeviceActionLoading(true);
      try {
        const dId = currentUser.deviceId || "ESP32_Test";
        await apiSendData({
          email: currentUser.email,
          action: "SET_INTENSITY",
          value: newLevel, 
          region: currentUser.region || "서울",
          deviceId: dId
        });
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
      
      // Wi-Fi 상태 동기화 (라스트 신 타임스탬프 파싱)
      if (response.last_seen) {
        const lastSeen = new Date(response.last_seen);
        const now = new Date();
        const diffInSeconds = (now.getTime() - lastSeen.getTime()) / 1000;
        
        if (diffInSeconds < 20) {
          setWifiStrength("strong");
        } else if (diffInSeconds < 60) {
          setWifiStrength("medium");
        } else if (diffInSeconds < 180) {
          setWifiStrength("weak");
        } else {
          setWifiStrength("disconnected");
        }
      } else {
        setWifiStrength("disconnected");
      }

      // 잔량, 실중량 값 및 가변 슬롯 매핑 정보(이름, 색상) 동시 업데이트
      if (response.weights && Array.isArray(response.weights)) {
        const rawWeights = response.weights_raw || [];
        setScentSlots(prev => prev.map((slot, index) => {
          const weightPercent = response.weights![index];
          let estimatedGrams = (weightPercent / 100) * 75.6 + 19.8;
          const weightGrams = rawWeights[index] !== undefined ? rawWeights[index] : estimatedGrams;
          
          let updatedName = slot.name;
          let updatedColor = slot.color;
          
          if (response.mapping && response.mapping[String(slot.id)]) {
            const kindId = response.mapping[String(slot.id)];
            const knownScents: Record<number, { name: string, color: string }> = {
              1: { name: "시트러스", color: "bg-orange-500" },
              2: { name: "센달우드", color: "bg-amber-700" },
              3: { name: "패츌리", color: "bg-green-700" },
              4: { name: "페퍼민트", color: "bg-teal-500" },
              5: { name: "라벤더", color: "bg-violet-500" },
              6: { name: "바닐라", color: "bg-orange-300" },
              7: { name: "무향(물)", color: "bg-cyan-500" }
            };
            if (knownScents[kindId]) {
              updatedName = knownScents[kindId].name;
              updatedColor = knownScents[kindId].color;
            }
          }
          
          return { 
            ...slot, 
            name: updatedName,
            color: updatedColor,
            remaining: weightPercent !== undefined ? Math.min(100, Math.max(0, Math.round(weightPercent))) : slot.remaining,
            weightGrams: weightGrams
          };
        }));
      }

      // 제어 조작 직후의 덮어쓰기 방지 쿨다운 가드 (5초)
      if (response.intensity !== undefined && (Date.now() - lastIntensityUpdateTimeRef.current > 5000)) {
        setIntensity(response.intensity);
      }

      if (response.volume !== undefined && (Date.now() - lastVolumeUpdateTimeRef.current > 5000)) {
        setVolume(response.volume);
      }

      if (response.timer_enabled !== undefined) setTimerEnabled(response.timer_enabled);
      if (response.timer_start !== undefined) setTimerStart(response.timer_start);
      if (response.timer_end !== undefined) setTimerEnd(response.timer_end);

      // 소음 환경 분석 데이터 매핑
      if (response.db_level !== undefined) {
        setDbChange(response.db_level - lastDbLevelRef.current);
        setDbLevel(response.db_level);
        lastDbLevelRef.current = response.db_level;
      }
      if (response.db_avg !== undefined) setDbAvg(response.db_avg);
      if (response.db_stddev !== undefined) setDbStdDev(response.db_stddev);
      
      if (response.noise_type !== undefined) {
        setNoiseType(response.noise_type);
      } else if (response.db_stddev !== undefined) {
        setNoiseType(response.db_stddev > 8.0 ? "불규칙적(대화/활동)" : "안정적(음악/정적)");
      }

      // 기기 운영태스크 상태 메시지 동기화
      if (response.status !== undefined) {
        setDeviceStatus(response.status);
      }

      // 조명 동기화 및 전원 오프 시 Hex 무너짐 예방 로직 적용
      if (response.led_br !== undefined && (Date.now() - lastLedUpdateTimeRef.current > 5000)) {
        const brValueRaw = response.led_br;
        const brPercent = Math.round((brValueRaw / 255) * 100);
        
        setIsLedOn(brValueRaw > 0);
        
        if (brValueRaw > 0) {
          setLedBrightness(brPercent);
          
          if (response.led_r !== undefined && response.led_g !== undefined && response.led_b !== undefined) {
            const hex = `#${response.led_r.toString(16).padStart(2, '0')}${response.led_g.toString(16).padStart(2, '0')}${response.led_b.toString(16).padStart(2, '0')}`;
            if (hex !== "#000000") {
              setLedColor(hex.toUpperCase());
            }
          }
        }
      }

      // 가동 노즐 번호 동기화 
      if (response.spray !== undefined && response.spray > 0 && response.spray !== 90) {
        setCurrentScent(String(response.spray));
        setIsDiffuserOn(true);
      }
    } catch (err) {
      console.error("Failed to refresh device state", err);
    }
  }, [currentUser]);

  const sendDeviceData = React.useCallback(async (action: string, value: number, region?: string, diaryText?: string, ledData?: { r: number, g: number, b: number, br: number }) => {
    if (!currentUser) return { success: false, message: "로그인이 필요합니다." };
    
    if (action === "SET_LED") lastLedUpdateTimeRef.current = Date.now();
    if (action === "SET_VOLUME") lastVolumeUpdateTimeRef.current = Date.now();

    const dId = currentUser.deviceId || "ESP32_Test";
    const response = await apiSendData({
      email: currentUser.email,
      action,
      value,
      region: region || currentUser.region || "서울",
      deviceId: dId,
      diaryText,
      ledData
    });

    if (response.result === "SUCCESS" || response.result_text || response.spray !== undefined) {
      setTimeout(() => refreshDeviceState(), 500);
      return { 
        ...response,
        success: true, 
        message: response.result_text || response.message || "명령 전송 완료",
      };
    }
    return { ...response, success: false, message: response.message || "명령 전송 실패" };
  }, [currentUser, refreshDeviceState]);

  const updateScentSlot = (id: number, slot: Partial<ScentSlot>) => {
    setScentSlots(prev => prev.map(s => s.id === id ? { ...s, ...slot } : s));
  };

  // 저울 교정/영점 매핑 명령 처리 수동 분기 액션
  const calibrateWeight = async () => {
    if (!currentUser) return false;
    try {
      const response = await apiSendData({
        email: currentUser.email,
        action: "CALIBRATE",
        value: 0,
        region: "CALIBRATE",
        deviceId: currentUser.deviceId || "ESP32_Test"
      });
      return response.result === "SUCCESS";
    } catch (e) {
      console.error("Calibration error", e);
      return false;
    }
  };

  useEffect(() => {
    if (currentUser) {
      refreshDeviceState();
      const interval = setInterval(refreshDeviceState, 10000); // 10초 주기 데이터 폴링 가동
      return () => clearInterval(interval);
    }
  }, [currentUser, refreshDeviceState]);

  return (
    <DeviceContext.Provider value={{
      scentSlots, updateScentSlot, refreshDeviceState,
      wifiStrength, setWifiStrength,
      volume, handleVolumeChange,
      intensity, handleIntensityChange,
      activeTimerMinutes, handleTimerChange,
      timerEnabled, timerStart, timerEnd, updateTimerSettings,
      currentScent, isDiffuserOn, setIsDiffuserOn, 
      ledColor, ledBrightness, isLedOn,
      activeMode, setActiveMode,
      isDeviceActionLoading,
      sendDeviceData,
      calibrateWeight,
      dbLevel,
      dbAvg,
      dbStdDev,
      noiseType,
      dbChange,
      deviceStatus
    }}>
      {children}
    </DeviceContext.Provider>
  );
};

export const useDevice = () => {
  const context = useContext(DeviceContext);
  if (context === undefined) {
    throw new Error("useDevice must be used within a DeviceProvider");
  }
  return context;
};