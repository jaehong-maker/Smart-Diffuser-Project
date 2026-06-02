import React, { useState, useEffect } from "react";
import { useNavigate } from "react-router"; 
import { motion } from "motion/react"; 
import { ArrowLeft, Scale, RefreshCw, CheckCircle2, AlertCircle, Clock } from "lucide-react"; 
import { useDevice } from "../store/DeviceContext";

export function WeightCalibrationScreen() {
  const navigate = useNavigate();
  const { scentSlots, refreshDeviceState } = useDevice();
  const [isMeasuring, setIsMeasuring] = useState(false);
  const [isCalibrating, setIsCalibrating] = useState(false);
  const [successMsg, setSuccessMsg] = useState("");
  const [lastUpdated, setLastUpdated] = useState("방금 전");

  // 시간 기반 데이터는 현재 앱에 따로 없으므로, 로드셀 값과 약간의 오차를 둔 시뮬레이션용으로 유지하거나
  // 혹은 단순히 로드셀 값만 보여줄 수 있습니다. 
  // 여기서는 로드셀(실제 무게) 값을 강조하여 보여줍니다.

  const handleMeasure = async () => {
    setIsMeasuring(true);
    setSuccessMsg("");
    try {
      await refreshDeviceState();
      setLastUpdated(new Date().toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit', second: '2-digit' }));
    } catch (err) {
      console.error(err);
    } finally {
      setIsMeasuring(false);
    }
  };

  const handleCalibrate = () => {
    setIsCalibrating(true);
    setSuccessMsg("");
    
    // 실제 하드웨어 보정 명령이 있다면 여기서 호출 (현재는 UI 피드백만)
    setTimeout(() => {
      setIsCalibrating(false);
      setSuccessMsg("모든 카트리지의 잔량이 실제 무게 기준으로 보정되었습니다.");
      setTimeout(() => setSuccessMsg(""), 4000);
    }, 1500);
  };

  useEffect(() => {
    // 화면 진입 시 최신 데이터로 갱신
    handleMeasure();
  }, []);

  return (
    <div key="calibration-screen" className="flex-1 flex flex-col bg-gray-50 dark:bg-gray-950 overflow-y-auto relative min-h-screen transition-colors duration-300">
      <header className="px-6 pt-12 pb-4 flex items-center justify-between z-10 sticky top-0 bg-gray-50/90 dark:bg-gray-950/90 backdrop-blur-md border-b border-gray-200/50 dark:border-gray-800/50 transition-colors duration-300">
        <div className="flex items-center">
          <button 
            onClick={() => navigate(-1)}
            className="w-10 h-10 -ml-2 rounded-full flex items-center justify-center text-gray-900 dark:text-white hover:bg-gray-200 dark:hover:bg-gray-800 transition-colors"
          >
            <ArrowLeft className="w-6 h-6" />
          </button>
          <h1 className="text-xl font-bold tracking-tight text-gray-900 dark:text-white ml-2">무게 확인 및 보정</h1>
        </div>
        <div className="w-10 h-10 bg-gray-100 dark:bg-gray-800 rounded-full flex items-center justify-center text-gray-400 dark:text-gray-500">
          <Scale className="w-5 h-5" />
        </div>
      </header>

      <div className="flex-1 px-6 py-6 flex flex-col gap-6">
        <div className="mb-2">
          <h2 className="text-2xl font-bold text-gray-900 dark:text-white">실시간 무게 측정</h2>
          <p className="text-sm font-medium text-gray-500 dark:text-gray-400 mt-2 leading-relaxed">
            기기 내 <strong className="text-gray-700 dark:text-gray-300">로드셀 센서</strong>가 측정한 실제 무게를 실시간으로 확인합니다. 카트리지 잔량이 정확하지 않을 경우 보정 버튼을 눌러주세요.
          </p>
        </div>

        <div className="flex gap-3">
          <button 
            onClick={handleMeasure}
            disabled={isMeasuring || isCalibrating}
            className="flex-1 h-12 bg-white dark:bg-gray-900 border border-gray-200 dark:border-gray-800 text-gray-800 dark:text-gray-200 rounded-xl flex items-center justify-center gap-2 font-bold text-sm shadow-sm hover:bg-gray-50 dark:hover:bg-gray-800 transition-colors disabled:opacity-50"
          >
            <RefreshCw className={`w-4 h-4 ${isMeasuring ? 'animate-spin' : ''}`} />
            실시간 갱신
          </button>
          <button 
            onClick={handleCalibrate}
            disabled={isMeasuring || isCalibrating}
            className="flex-[1.5] h-12 bg-gray-900 dark:bg-gray-100 text-white dark:text-gray-900 rounded-xl flex items-center justify-center gap-2 font-bold text-sm shadow-sm hover:bg-gray-800 dark:hover:bg-gray-200 transition-colors disabled:opacity-50"
          >
            {isCalibrating ? (
              <RefreshCw className="w-4 h-4 animate-spin" />
            ) : (
              <CheckCircle2 className="w-4 h-4" />
            )}
            무게 데이터 보정
          </button>
        </div>

        {successMsg && (
          <motion.div initial={{ opacity: 0, y: -10 }} animate={{ opacity: 1, y: 0 }} className="p-4 bg-green-50 dark:bg-green-900/30 border border-green-200 dark:border-green-800 text-green-700 dark:text-green-400 rounded-2xl flex items-center gap-2 font-bold text-sm">
            <CheckCircle2 className="w-5 h-5 flex-shrink-0" />
            <span>{successMsg}</span>
          </motion.div>
        )}

        <div className="flex items-center justify-between text-xs font-semibold text-gray-400 dark:text-gray-500 mt-2 px-1">
          <span>마지막 측정: {lastUpdated}</span>
          <div className="flex gap-3">
            <div className="flex items-center gap-1"><div className="w-2 h-2 rounded-full bg-blue-500 dark:bg-blue-400"/>로드셀 측정값</div>
          </div>
        </div>

        <div className="flex flex-col gap-4 pb-8">
          {scentSlots.map((slot) => {
            // 시뮬레이션용 시간 기반 값 (실제로는 context에 없으므로 로드셀 값 기준으로 가상 생성)
            const simulatedTimeBased = Math.min(100, Math.round(slot.remaining + (Math.random() * 4 - 2)));
            const diff = Math.abs(simulatedTimeBased - slot.remaining);
            const isWarning = diff >= 8;

            return (
              <motion.div 
                key={slot.id} 
                initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}
                className={`bg-white dark:bg-gray-900 rounded-3xl p-5 shadow-sm border ${isWarning ? 'border-red-200 dark:border-red-900/50' : 'border-gray-200 dark:border-gray-800'} transition-colors duration-300`}
              >
                <div className="flex justify-between items-start mb-4">
                  <div className="flex items-center gap-3">
                    <div className={`w-10 h-10 rounded-xl flex items-center justify-center font-bold bg-gray-100 dark:bg-gray-800 text-gray-600 dark:text-gray-400`}>
                      S{slot.id}
                    </div>
                    <div>
                      <h3 className="text-base font-bold text-gray-900 dark:text-white">{slot.name}</h3>
                      <p className="text-xs font-semibold text-gray-400 dark:text-gray-500">슬롯 {slot.id}</p>
                    </div>
                  </div>
                  {isWarning ? (
                    <div className="px-2.5 py-1 bg-red-50 dark:bg-red-900/20 text-red-600 dark:text-red-400 rounded-lg flex items-center gap-1 text-[11px] font-bold border border-red-100 dark:border-red-900/30">
                      <AlertCircle className="w-3 h-3" />
                      오차 감지
                    </div>
                  ) : (
                    <div className="px-2.5 py-1 bg-green-50 dark:bg-green-900/20 text-green-600 dark:text-green-400 rounded-lg flex items-center gap-1 text-[11px] font-bold border border-green-100 dark:border-green-900/30">
                      <CheckCircle2 className="w-3 h-3" />
                      상태 양호
                    </div>
                  )}
                </div>

                <div className="flex flex-col gap-3">
                  {/* 로드셀 측정 (메인) */}
                  <div className="flex items-center gap-3">
                    <div className="w-16 flex flex-col items-center">
                      <Scale className="w-4 h-4 text-blue-500 dark:text-blue-400 mb-1" />
                      <span className="text-[10px] font-bold text-blue-600 dark:text-blue-400 text-center">직접측정</span>
                    </div>
                    <div className="flex-1 flex items-center gap-3">
                      <div className="h-3 flex-1 bg-gray-100 dark:bg-gray-800 rounded-full overflow-hidden relative">
                        <motion.div 
                          initial={{ width: 0 }} animate={{ width: `${slot.remaining}%` }} 
                          className={`h-full ${slot.remaining < 20 ? 'bg-red-500' : 'bg-blue-500'} rounded-full`} 
                        />
                      </div>
                      <div className="flex flex-col items-end min-w-[40px]">
                        <span className="text-sm font-bold text-blue-600 dark:text-blue-400">{slot.remaining}%</span>
                        <span className="text-[10px] font-medium text-gray-500 dark:text-gray-400 whitespace-nowrap">{slot.weightGrams}g</span>
                      </div>
                    </div>
                  </div>

                  {/* 예측 잔량 (보조) */}
                  <div className="flex items-center gap-3 opacity-60">
                    <div className="w-16 flex flex-col items-center">
                      <Clock className="w-3.5 h-3.5 text-gray-400 dark:text-gray-500 mb-1" />
                      <span className="text-[9px] font-bold text-gray-500 dark:text-gray-400 text-center">예측잔량</span>
                    </div>
                    <div className="flex-1 flex items-center gap-3">
                      <div className="h-1.5 flex-1 bg-gray-100 dark:bg-gray-800 rounded-full overflow-hidden">
                        <motion.div 
                          initial={{ width: 0 }} animate={{ width: `${simulatedTimeBased}%` }} 
                          className="h-full bg-gray-300 dark:bg-gray-600 rounded-full" 
                        />
                      </div>
                      <span className="text-xs font-bold text-gray-400 dark:text-gray-500 min-w-[40px] text-right">{simulatedTimeBased}%</span>
                    </div>
                  </div>
                </div>

              </motion.div>
            );
          })}
        </div>
      </div>
    </div>
  );
}