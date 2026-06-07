import React, { useState } from "react";
import { useNavigate } from "react-router";
import { motion } from "motion/react";
import { 
  ArrowLeft, 
  Cpu, 
  Wifi, 
  Battery, 
  Wind, 
  RefreshCw, 
  Power, 
  Trash2, 
  Smartphone,
  CheckCircle2,
  AlertTriangle,
  Edit2,
  Check,
  X
} from "lucide-react";
import { useDevice } from "../store/DeviceContext";
import { useAuth } from "../store/AuthContext";

export function DeviceManagementScreen() {
  const navigate = useNavigate();
  const { currentUser, updateUser } = useAuth();
  const { wifiStrength, sendDeviceData } = useDevice();
  const [isUpdating, setIsUpdating] = useState(false);
  const [updateSuccess, setUpdateSuccess] = useState(false);
  
  const [isEditingId, setIsEditingId] = useState(false);
  const [newDeviceId, setNewDeviceId] = useState(currentUser?.deviceId || "ESP32_Test");

  const wifiSignalMap = {
    strong: "강함",
    medium: "양호",
    weak: "약함",
    disconnected: "연결 끊김"
  };

  const wifiSignal = wifiSignalMap[wifiStrength];

  const handleWifiReset = async () => {
    if (!window.confirm("기기의 Wi-Fi 설정을 초기화하고 재부팅하시겠습니까?")) return;
    
    try {
      const res = await sendDeviceData("WIFI_RESET", 0);
      if (res.success) {
        alert("기기에 Wi-Fi 초기화 명령을 보냈습니다. 기기가 재부팅되면 Wi-Fi 설정 모드로 진입합니다.");
      } else {
        alert("명령 전송 실패: " + res.message);
      }
    } catch (err) {
      console.error("WiFi Reset Error:", err);
      alert("통신 중 오류가 발생했습니다.");
    }
  };

  const handleUpdate = () => {
    setIsUpdating(true);
    setTimeout(() => {
      setIsUpdating(false);
      setUpdateSuccess(true);
      setTimeout(() => setUpdateSuccess(false), 3000);
    }, 2500);
  };

  const saveDeviceId = async () => {
    if (!currentUser) return;
    const res = await updateUser(currentUser.email, { deviceId: newDeviceId });
    if (res.success) {
      setIsEditingId(false);
      alert("기기 번호가 업데이트되었습니다.");
    } else {
      alert("업데이트 실패: " + res.message);
    }
  };

  return (
    <div key="device-screen" className="flex-1 flex flex-col bg-gray-50 dark:bg-gray-950 overflow-y-auto relative min-h-screen transition-colors duration-300">
      <header className="px-6 pt-12 pb-4 flex items-center justify-between z-10 sticky top-0 bg-gray-50/90 dark:bg-gray-950/90 backdrop-blur-md border-b border-gray-200/50 dark:border-gray-800/50 transition-colors duration-300">
        <div className="flex items-center">
          <button 
            onClick={() => navigate(-1)}
            className="w-10 h-10 -ml-2 rounded-full flex items-center justify-center text-gray-900 dark:text-white hover:bg-gray-200 dark:hover:bg-gray-800 transition-colors"
          >
            <ArrowLeft className="w-6 h-6" />
          </button>
          <h1 className="text-xl font-bold tracking-tight text-gray-900 dark:text-white ml-2">기기 관리</h1>
        </div>
        <div className="w-10 h-10 bg-gray-100 dark:bg-gray-800 rounded-full flex items-center justify-center text-gray-400 dark:text-gray-500">
          <Cpu className="w-5 h-5" />
        </div>
      </header>

      <div className="flex-1 px-6 py-6 flex flex-col gap-6 pb-12">
        
        {/* Device Status Hero */}
        <motion.div 
          initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}
          className="bg-white dark:bg-gray-900 rounded-3xl p-6 shadow-sm border border-gray-200 dark:border-gray-800 flex flex-col items-center text-center transition-colors duration-300"
        >
          <div className="relative mb-4">
            <div className="w-24 h-24 bg-gray-50 dark:bg-gray-800 rounded-full flex items-center justify-center border-4 border-white dark:border-gray-900 shadow-md z-10 relative">
              <Smartphone className="w-10 h-10 text-gray-800 dark:text-gray-200" />
            </div>
            {wifiStrength !== "disconnected" && (
              <motion.div 
                animate={{ scale: [1, 1.2, 1], opacity: [0.5, 0, 0.5] }} 
                transition={{ repeat: Infinity, duration: 2 }}
                className="absolute inset-0 bg-green-400 rounded-full z-0" 
              />
            )}
            <div className={`absolute -bottom-1 -right-1 w-8 h-8 ${wifiStrength === "disconnected" ? "bg-red-500" : "bg-green-500"} rounded-full border-4 border-white dark:border-gray-900 flex items-center justify-center z-20`}>
              <Wifi className="w-3 h-3 text-white" />
            </div>
          </div>
          
          <h2 className="text-xl font-bold text-gray-900 dark:text-white">Smart Diffuser v3</h2>
          
          <div className="mt-2 flex flex-col items-center">
            {isEditingId ? (
              <div className="flex items-center gap-2 mt-1">
                <input 
                  type="text"
                  value={newDeviceId}
                  onChange={(e) => setNewDeviceId(e.target.value)}
                  className="bg-gray-100 dark:bg-gray-800 border-none rounded-lg px-3 py-1.5 text-sm font-bold text-gray-900 dark:text-white focus:ring-2 focus:ring-blue-500 outline-none w-40"
                  placeholder="기기 번호 입력"
                />
                <button onClick={saveDeviceId} className="p-2 bg-blue-500 text-white rounded-lg"><Check className="w-4 h-4" /></button>
                <button onClick={() => { setIsEditingId(false); setNewDeviceId(currentUser?.deviceId || ""); }} className="p-2 bg-gray-200 dark:bg-gray-700 text-gray-600 dark:text-gray-400 rounded-lg"><X className="w-4 h-4" /></button>
              </div>
            ) : (
              <div className="flex items-center gap-2 text-sm font-medium text-gray-500 dark:text-gray-400">
                <span>기기 번호: <span className="font-bold text-gray-700 dark:text-gray-300">{currentUser?.deviceId || "미등록"}</span></span>
                <button onClick={() => setIsEditingId(true)} className="p-1 hover:bg-gray-100 dark:hover:bg-gray-800 rounded transition-colors">
                  <Edit2 className="w-3 h-3" />
                </button>
              </div>
            )}
          </div>

          <p className="text-sm font-medium text-gray-500 dark:text-gray-400 mt-3">
            {wifiStrength === "disconnected" ? "네트워크 연결 끊김" : "네트워크 연결됨 (클라우드)"}
          </p>
          
          <div className="flex items-center gap-6 mt-6 w-full justify-center">
            <div className="flex flex-col items-center gap-1 group transition-transform hover:scale-105 active:scale-95">
              <div className={`w-10 h-10 rounded-full flex items-center justify-center transition-colors ${
                wifiStrength === "strong" ? "bg-green-50 dark:bg-green-900/20 text-green-600 dark:text-green-400" :
                wifiStrength === "medium" ? "bg-yellow-50 dark:bg-yellow-900/20 text-yellow-600 dark:text-yellow-400" :
                wifiStrength === "weak" ? "bg-orange-50 dark:bg-orange-900/20 text-orange-600 dark:text-orange-400" :
                "bg-red-50 dark:bg-red-900/20 text-red-600 dark:text-red-400"
              }`}>
                <Wifi className={`w-5 h-5 ${wifiStrength === "disconnected" ? "opacity-50" : ""}`} />
              </div>
              <span className={`text-xs font-bold ${
                wifiStrength === "strong" ? "text-green-600 dark:text-green-400" :
                wifiStrength === "medium" ? "text-yellow-600 dark:text-yellow-400" :
                wifiStrength === "weak" ? "text-orange-600 dark:text-orange-400" :
                "text-red-600 dark:text-red-400"
              }`}>
                신호 {wifiSignal}
              </span>
            </div>
          </div>
        </motion.div>

        {/* Maintenance Actions */}
        <div className="flex flex-col gap-3">
          <h3 className="text-sm font-bold text-gray-500 dark:text-gray-400 px-1">기기 유지보수</h3>
          
          <motion.div initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.1 }} className="bg-white dark:bg-gray-900 rounded-3xl border border-gray-200 dark:border-gray-800 overflow-hidden shadow-sm">
            <div className="p-5 flex flex-col gap-4">
              <div className="flex items-start justify-between">
                <div className="flex items-center gap-3">
                  <div className="w-10 h-10 bg-gray-50 dark:bg-gray-800 text-gray-600 dark:text-gray-400 rounded-xl flex items-center justify-center">
                    <Cpu className="w-5 h-5" />
                  </div>
                  <div>
                    <h4 className="text-base font-bold text-gray-900 dark:text-white">펌웨어 업데이트</h4>
                    <p className="text-xs font-medium text-gray-500 dark:text-gray-400 mt-0.5">현재 버전: v3.0.2</p>
                  </div>
                </div>
                <div className="px-2 py-1 bg-red-50 dark:bg-red-900/20 text-red-600 dark:text-red-400 text-[10px] font-bold rounded-lg border border-red-100 dark:border-red-900/30">
                  v3.1.0 가능
                </div>
              </div>
              
              {updateSuccess ? (
                <div className="w-full py-3 bg-green-50 dark:bg-green-900/20 text-green-600 dark:text-green-400 font-bold text-sm rounded-xl flex items-center justify-center gap-2 border border-green-100 dark:border-green-900/30">
                  <CheckCircle2 className="w-4 h-4" /> 최신 버전입니다
                </div>
              ) : (
                <button 
                  onClick={handleUpdate} 
                  disabled={isUpdating}
                  className="w-full py-3 bg-gray-900 dark:bg-gray-100 hover:bg-gray-800 dark:hover:bg-gray-200 text-white dark:text-gray-900 font-bold text-sm rounded-xl transition-colors flex items-center justify-center gap-2"
                >
                  {isUpdating ? <RefreshCw className="w-4 h-4 animate-spin" /> : null}
                  {isUpdating ? "업데이트 중..." : "최신 펌웨어로 업데이트"}
                </button>
              )}
            </div>

            <div className="px-5 pb-5">
              <button 
                onClick={handleWifiReset}
                className="w-full py-3 bg-white dark:bg-gray-800 border border-gray-200 dark:border-gray-700 hover:bg-gray-50 dark:hover:bg-gray-750 text-gray-900 dark:text-white font-bold text-sm rounded-xl transition-colors flex items-center justify-center gap-2"
              >
                <Wifi className="w-4 h-4 text-blue-500" />
                Wi-Fi 초기화
              </button>
              <p className="text-[10px] text-gray-400 mt-2 text-center px-4 break-keep leading-relaxed">기기의 Wi-Fi 정보가 삭제되며 설정 모드로 진입합니다. 설정 시 스마트폰을 기기 근처로 가져가세요.</p>
            </div>
          </motion.div>
        </div>

        {/* System Settings */}
        <div className="flex flex-col gap-3">
          <h3 className="text-sm font-bold text-gray-500 dark:text-gray-400 px-1 mt-2">시스템 설정</h3>
          
          <motion.div initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} transition={{ delay: 0.2 }} className="bg-white dark:bg-gray-900 rounded-3xl border border-gray-200 dark:border-gray-800 overflow-hidden shadow-sm">
            <button className="w-full flex items-center justify-between p-4 border-b border-gray-100 dark:border-gray-800 hover:bg-gray-50 dark:hover:bg-gray-800 transition-colors text-left">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 bg-orange-50 dark:bg-orange-900/20 rounded-xl flex items-center justify-center text-orange-600 dark:text-orange-400">
                  <Power className="w-5 h-5" />
                </div>
                <div className="flex flex-col">
                  <span className="font-bold text-gray-800 dark:text-gray-200">기기 원격 재시작</span>
                  <span className="text-[11px] font-medium text-gray-400">연결 오류가 발생할 때 유용합니다</span>
                </div>
              </div>
            </button>
            
            <button className="w-full flex items-center justify-between p-4 hover:bg-red-50 dark:hover:bg-red-900/10 transition-colors text-left">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 bg-red-50 dark:bg-red-900/20 rounded-xl flex items-center justify-center text-red-500">
                  <Trash2 className="w-5 h-5" />
                </div>
                <div className="flex flex-col">
                  <span className="font-bold text-red-500">기기 공장 초기화</span>
                  <span className="text-[11px] font-medium text-red-400 dark:text-red-500/70">모든 설정과 향기 데이터가 삭제됩니다</span>
                </div>
              </div>
            </button>
          </motion.div>
        </div>

      </div>
    </div>
  );
}
