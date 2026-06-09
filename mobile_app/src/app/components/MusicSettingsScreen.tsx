import React, { useState, useMemo, useEffect } from "react";
import { useNavigate } from "react-router";
import { motion, AnimatePresence } from "motion/react";
import { 
  ArrowLeft, Music, Music4, Check, Search, RefreshCw
} from "lucide-react";

import { TRACKS } from "../utils/tracks";

import { useDevice } from "../store/DeviceContext";
import { useAuth } from "../store/AuthContext";

export function MusicSettingsScreen() {
  const navigate = useNavigate();
  const { currentUser, updateUser } = useAuth();
  const { scentSlots, sendDeviceData } = useDevice();
  const [playingTrackId, setPlayingTrackId] = useState<string | null>(null);
  const [isSaving, setIsSaving] = useState(false);

  // 4개의 카트리지(슬롯) 데이터
  const [slots, setSlots] = useState(() => {
    // 기본값 설정 (1, 6, 11, 16번 곡)
    let initialTrackIds = ["1", "6", "11", "16"];
    
    // 서버에서 저장된 데이터가 있다면 해당 데이터 사용
    if (currentUser?.musicTracks && currentUser.musicTracks.includes("_")) {
      initialTrackIds = currentUser.musicTracks.split("_");
    }
    
    return scentSlots.map((s, i) => ({
      id: s.id,
      scentName: s.name,
      selectedTrackId: initialTrackIds[i] === "0" || !initialTrackIds[i] ? "none" : `song_${initialTrackIds[i]}`
    }));
  });

  const [isModalOpen, setIsModalOpen] = useState(false);
  const [activeSlotId, setActiveSlotId] = useState<number | null>(null);
  const [searchTerm, setSearchTerm] = useState("");

  const togglePlay = (trackId: string) => {
    if (trackId === "none") return;
    setPlayingTrackId(prev => prev === trackId ? null : trackId);
  };

  const openSelectModal = (slotId: number) => {
    setActiveSlotId(slotId);
    setSearchTerm("");
    setIsModalOpen(true);
  };

  const selectTrackForSlot = (trackId: string) => {
    if (activeSlotId === null) return;
    setSlots(prev => prev.map(s => s.id === activeSlotId ? { ...s, selectedTrackId: trackId } : s));
    setIsModalOpen(false);
  };

  const handleSave = async () => {
    if (!currentUser) return;
    setIsSaving(true);
    try {
      // 숫자만 추출하여 _ 로 연결 (예: 1_4_12_0)
      const musicData = slots.map(s => {
        if (s.selectedTrackId === "none") return "0";
        return s.selectedTrackId.replace("song_", "");
      }).join("_");
      
      const result = await sendDeviceData("SAVE_MUSIC", 0, musicData);
      if (result.success) { 
        // 🚀 중요: 서버 저장 성공 시, 앱이 기억하는 로컬 사용자 정보도 즉시 갱신!
        await updateUser(currentUser.email, { musicTracks: musicData });
        
        alert("음악 설정이 저장되었습니다.");
        navigate(-1); 
      }
    } catch (error) { 
      console.error(error); 
      alert("저장 중 오류가 발생했습니다.");
    } finally { 
      setIsSaving(false); 
    }
  };

  const filteredTracks = useMemo(() => {
    if (!searchTerm) return TRACKS;
    return TRACKS.filter(t => t.name.includes(searchTerm) || t.artist.includes(searchTerm));
  }, [searchTerm]);

  return (
    <div key="music-screen" className="flex-1 flex flex-col bg-white dark:bg-gray-950 min-h-screen transition-colors duration-300 relative">
      <header className="px-6 pt-12 pb-4 flex items-center justify-between z-10 sticky top-0 bg-white/80 dark:bg-gray-950/80 backdrop-blur-md transition-colors duration-300">
        <div className="flex items-center">
          <button onClick={() => navigate(-1)} className="w-10 h-10 -ml-2 rounded-full flex items-center justify-center text-gray-900 dark:text-white hover:bg-gray-200 dark:hover:bg-gray-800 transition-colors">
            <ArrowLeft className="w-6 h-6" />
          </button>
          <h1 className="text-xl font-bold tracking-tight text-gray-900 dark:text-white ml-2">음악 설정</h1>
        </div>
        <button onClick={handleSave} disabled={isSaving} className="px-5 py-2.5 bg-blue-600 dark:bg-blue-500 text-white rounded-2xl text-sm font-bold hover:bg-blue-700 transition-all flex items-center gap-1.5 disabled:opacity-50 shadow-lg shadow-blue-500/20">
          {isSaving ? <RefreshCw className="w-4 h-4 animate-spin" /> : <Check className="w-4 h-4" />}
          저장하기
        </button>
      </header>

      <div className="flex-1 px-6 py-4 flex flex-col gap-6 pb-24 overflow-y-auto">
        <div className="mb-2">
          <p className="text-sm font-medium text-gray-500 dark:text-gray-400 leading-relaxed">
            각 향기가 분사될 때 기기(SD카드)에서 재생될 곡을 선택해 주세요.
          </p>
        </div>

        <div className="flex flex-col gap-4">
          {slots.map((slot) => {
            const currentTrack = TRACKS.find(t => t.id === slot.selectedTrackId) || TRACKS[0];
            const isNone = currentTrack.id === "none";
            const isPlaying = playingTrackId === currentTrack.id;

            return (
              <motion.div 
                key={slot.id} 
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: slot.id * 0.1 }}
                className="bg-gray-50 dark:bg-gray-900/50 rounded-[2rem] p-6 border border-gray-100 dark:border-gray-800/50 flex flex-col gap-5"
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 rounded-2xl flex items-center justify-center font-black text-lg bg-gray-200 dark:bg-gray-800 text-gray-700 dark:text-gray-300">
                      {slot.id}
                    </div>
                    <div>
                      <h3 className="text-base font-bold text-gray-900 dark:text-white">{slot.scentName}</h3>
                      <p className="text-[10px] font-bold text-gray-400 uppercase tracking-tighter">Scent Slot {slot.id}</p>
                    </div>
                  </div>
                  <button 
                    onClick={() => openSelectModal(slot.id)} 
                    className="px-4 py-2 bg-white dark:bg-gray-800 text-gray-900 dark:text-white rounded-xl text-xs font-bold border border-gray-200 dark:border-gray-700 shadow-sm active:scale-95 transition-all"
                  >
                    곡 변경
                  </button>
                </div>

                <div className="flex items-center gap-4 bg-white dark:bg-gray-800 p-4 rounded-2xl shadow-sm border border-gray-100 dark:border-gray-800">
                  <div className={`w-10 h-10 rounded-full flex items-center justify-center ${isNone ? 'bg-gray-100 text-gray-300' : 'bg-blue-50 text-blue-500'}`}>
                    <Music className="w-5 h-5" />
                  </div>
                  <div className="flex-1 min-w-0">
                    <p className={`text-sm font-bold truncate ${isNone ? 'text-gray-400' : 'text-gray-900 dark:text-white'}`}>
                      {currentTrack.name}
                    </p>
                    <p className="text-[10px] font-medium text-gray-400 uppercase tracking-wider">
                      {isNone ? "Silent Mode" : currentTrack.artist}
                    </p>
                  </div>
                  {!isNone && (
                    <div className="flex gap-1">
                      <div className="w-1 h-3 bg-blue-500 rounded-full animate-bounce" style={{ animationDelay: '0s' }} />
                      <div className="w-1 h-5 bg-blue-500 rounded-full animate-bounce" style={{ animationDelay: '0.2s' }} />
                      <div className="w-1 h-3 bg-blue-500 rounded-full animate-bounce" style={{ animationDelay: '0.4s' }} />
                    </div>
                  )}
                </div>
              </motion.div>
            );
          })}
        </div>
      </div>

      <AnimatePresence>
        {isModalOpen && activeSlotId !== null && (
          <>
            <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }} onClick={() => setIsModalOpen(false)} className="fixed inset-0 bg-black/60 backdrop-blur-sm z-40" />
            <motion.div initial={{ y: "100%" }} animate={{ y: 0 }} exit={{ y: "100%" }} transition={{ type: "spring", damping: 30, stiffness: 250 }} className="fixed inset-x-0 bottom-0 z-50 bg-white dark:bg-gray-900 rounded-t-[3rem] shadow-2xl flex flex-col h-[75vh]">
              <div className="flex-shrink-0 flex flex-col items-center pt-3 pb-4 px-8 border-b border-gray-100 dark:border-gray-800">
                <div className="w-12 h-1.5 bg-gray-200 dark:bg-gray-700 rounded-full mb-6" />
                <div className="w-full relative">
                  <Search className="absolute left-4 top-1/2 -translate-y-1/2 h-5 w-5 text-gray-400" />
                  <input type="text" placeholder="곡 제목 또는 번호 검색..." value={searchTerm} onChange={(e) => setSearchTerm(e.target.value)} className="w-full bg-gray-100 dark:bg-gray-800 border-none rounded-2xl pl-12 pr-4 py-4 text-sm font-bold outline-none text-gray-900 dark:text-white" />
                </div>
              </div>
              <div className="flex-1 overflow-y-auto p-6 flex flex-col gap-3">
                {filteredTracks.map(track => {
                   const isSelected = slots.find(s => s.id === activeSlotId)?.selectedTrackId === track.id;
                   return (
                    <div 
                      key={track.id} 
                      className={`flex items-center p-5 rounded-[1.5rem] transition-all cursor-pointer border ${isSelected ? 'bg-blue-500 border-blue-500 text-white shadow-lg' : 'bg-white dark:bg-gray-800 border-gray-100 dark:border-gray-800 hover:border-blue-200'}`} 
                      onClick={() => selectTrackForSlot(track.id)}
                    >
                      <div className={`w-10 h-10 rounded-full flex items-center justify-center mr-4 ${isSelected ? 'bg-white/20' : 'bg-gray-50 dark:bg-gray-900 text-gray-400'}`}>
                        <Music4 className="w-5 h-5" />
                      </div>
                      <div className="flex-1 min-w-0">
                        <p className={`text-sm font-bold truncate ${isSelected ? 'text-white' : 'text-gray-900 dark:text-white'}`}>{track.name}</p>
                        <p className={`text-[10px] font-bold uppercase tracking-wider ${isSelected ? 'text-white/70' : 'text-gray-500'}`}>{track.artist}</p>
                      </div>
                      {isSelected && <Check className="w-5 h-5 ml-2" />}
                    </div>
                  );
                })}
              </div>
            </motion.div>
          </>
        )}
      </AnimatePresence>
    </div>
  );
}
