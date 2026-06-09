import React, { useState, useEffect } from "react";
import { Link } from "react-router";
import { BottomNav } from "./BottomNav";
import { Home as HomeIcon, Droplets, SlidersHorizontal, Settings, BookHeart, Brain, Sparkles, Search, ChevronDown, Quote } from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { useDiary } from "../store/DiaryContext";
import { useAuth } from "../store/AuthContext";
import { useDevice } from "../store/DeviceContext";

export function DiaryScreen() {
  const { diaries, fetchDiaries } = useDiary();
  const { currentUser } = useAuth();
  const { sendDeviceData, scentSlots } = useDevice();
  const [diaryText, setDiaryText] = useState("");
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [result, setResult] = useState<{ 
    emotion: string; 
    scent: string; 
    color: string; 
    tagColor: string; 
    spray?: number; 
    reason?: string;
    scentName?: string;
  } | null>(null);
  
  const [selectedDate, setSelectedDate] = useState<string | null>(null);
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);

  // 감정에 따른 시각적 테마 (색상, 그라디언트 등) 반환 함수
  const getEmotionTheme = (emotion: string) => {
    const e = (emotion || "편안함").toLowerCase();
    
    // 신남/활기 계열 (Orange/Yellow)
    if (e.includes("신남") || e.includes("행복") || e.includes("즐거움") || e.includes("설렘") || e.includes("뿌듯") || e.includes("활기") || e.includes("기쁨")) {
      return {
        color: "from-orange-400/90 to-yellow-300/90 dark:from-orange-600/80 dark:to-yellow-500/80 text-white shadow-orange-200/50",
        tagColor: "bg-white/20 text-white backdrop-blur-sm border-white/30"
      };
    }
    // 화남/짜증 계열 (Blue/Slate - 차분하게 가라앉히는 컨셉)
    if (e.includes("화남") || e.includes("짜증") || e.includes("답답") || e.includes("분노") || e.includes("스트레스")) {
       return {
        color: "from-blue-500/90 to-indigo-400/90 dark:from-blue-700/80 dark:to-indigo-600/80 text-white shadow-blue-200/50",
        tagColor: "bg-white/20 text-white backdrop-blur-sm border-white/30"
      };
    }
    // 슬픔/지침 계열 (Pink/Rose - 따뜻하게 감싸주는 컨셉)
    if (e.includes("슬픔") || e.includes("우울") || e.includes("외로움") || e.includes("지침") || e.includes("힘듦") || e.includes("피곤") || e.includes("고단")) {
      return {
        color: "from-rose-400/90 to-pink-300/90 dark:from-rose-600/80 dark:to-pink-500/80 text-white shadow-rose-200/50",
        tagColor: "bg-white/20 text-white backdrop-blur-sm border-white/30"
      };
    }
    // 편안함/평온/기본 계열 (Violet/Indigo)
    return {
      color: "from-violet-500/90 to-purple-400/90 dark:from-violet-700/80 dark:to-purple-600/80 text-white shadow-purple-200/50",
      tagColor: "bg-white/20 text-white backdrop-blur-sm border-white/30"
    };
  };

  // 향기 번호에 따른 실제 이름 찾기
  const getActualScentName = (sprayCode: number | undefined) => {
    if (!sprayCode) return "";
    // 블렌딩 코드인 경우 (예: 12)
    if (sprayCode > 10 && sprayCode < 90) {
      const first = Math.floor(sprayCode / 10);
      const second = sprayCode % 10;
      const name1 = scentSlots.find(s => s.id === first)?.name || "향기1";
      const name2 = scentSlots.find(s => s.id === second)?.name || "향기2";
      return `${name1} & ${name2}`;
    }
    const slot = scentSlots.find(s => s.id === sprayCode);
    return slot ? slot.name : "";
  };

  // 화면 진입 시 서버에서 일기 내역 가져오기
  useEffect(() => {
    if (currentUser?.email) {
      fetchDiaries(currentUser.email);
    }
  }, [currentUser?.email]);

  const history = currentUser && diaries[currentUser.email] ? diaries[currentUser.email] : [];
  const uniqueDates = Array.from(new Set(history.map(item => item.date)));

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!diaryText.trim() || !currentUser) return;

    setIsAnalyzing(true);
    setResult(null);

    try {
      // 인자: action, value, region(emotion tag), diaryText
      const res = await sendDeviceData("AI_EMOTION", 0, "분석중", diaryText);

      if (res.success) {
        // 서버에서 반환한 emotion_tag를 최우선으로 사용, 없으면 context에서 추출
        let detectedEmotion = res.emotion_tag || (res.context || "").replace("Emotion_", "").trim() || "편안함";
        if (detectedEmotion === "분석중") detectedEmotion = "편안함";
        
        // 동적 테마 적용
        const theme = getEmotionTheme(detectedEmotion);
        const actualScentName = getActualScentName(res.spray);

        setResult({
          emotion: detectedEmotion,
          scent: res.emotion_summary || "당신만을 위한 추천 향기", // 요약 메시지 활용
          reason: res.result_text || res.message || `${detectedEmotion} 감정에 어울리는 특별한 향기를 준비했어요.`,
          spray: res.spray,
          scentName: actualScentName,
          ...theme
        });

        // 서버에 저장된 최신 내역을 즉시 다시 불러오기
        if (currentUser?.email) {
          await fetchDiaries(currentUser.email);
        }
        
        setDiaryText(""); 
      } else {
        alert("분석 실패: " + res.message);
      }
    } catch (error) {
      console.error("AI Analysis failed:", error);
      alert("통신 오류가 발생했습니다.");
    } finally {
      setIsAnalyzing(false);
    }
  };

  const getHistoryEmotionColor = (emotion: string) => {
    const e = (emotion || "편안함").toLowerCase();
    if (e.includes("신남") || e.includes("행복") || e.includes("즐거움") || e.includes("설렘") || e.includes("뿌듯") || e.includes("활기") || e.includes("기쁨")) {
      return "bg-orange-100 dark:bg-orange-900/30 text-orange-700 dark:text-orange-400";
    }
    if (e.includes("화남") || e.includes("짜증") || e.includes("답답") || e.includes("분노") || e.includes("스트레스")) {
      return "bg-blue-100 dark:bg-blue-900/30 text-blue-700 dark:text-blue-400";
    }
    if (e.includes("슬픔") || e.includes("우울") || e.includes("외로움") || e.includes("지침") || e.includes("힘듦") || e.includes("피곤") || e.includes("고단")) {
      return "bg-pink-100 dark:bg-pink-900/30 text-pink-700 dark:text-pink-400";
    }
    return "bg-violet-100 dark:bg-violet-900/30 text-violet-700 dark:text-violet-400";
  };

  return (
    <div key="diary-screen" className="flex-1 flex flex-col bg-gray-50 dark:bg-gray-950 overflow-y-auto relative pb-24 min-h-screen transition-colors duration-300">
      <header className="px-6 pt-12 pb-4 flex justify-between items-center z-10 sticky top-0 bg-gray-50/90 dark:bg-gray-950/90 backdrop-blur-md transition-colors duration-300">
        <div>
          <h1 className="text-2xl font-bold tracking-tight text-gray-900 dark:text-white">감정 일기장</h1>
          <p className="text-sm font-medium text-gray-500 dark:text-gray-400 mt-1">오늘의 기분을 기록하고 향기를 추천받으세요</p>
        </div>
      </header>

      <div className="px-6 py-4 flex flex-col gap-6">
        <motion.div 
          initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}
          className="bg-white dark:bg-gray-900 rounded-3xl p-6 shadow-sm border border-gray-200 dark:border-gray-800 transition-colors duration-300"
        >
          <div className="flex items-center gap-2 mb-4">
            <BookHeart className="w-5 h-5 text-gray-700 dark:text-gray-300" />
            <h2 className="text-lg font-bold text-gray-900 dark:text-white">나의 3줄 일기</h2>
          </div>
          
          <form onSubmit={handleSubmit} className="flex flex-col gap-4">
            <textarea
              value={diaryText}
              onChange={(e) => setDiaryText(e.target.value)}
              placeholder="오늘 하루는 어땠나요? 3줄로 솔직한 감정을 적어주시면 AI가 문맥을 파악해 가장 어울리는 향기를 분사해드려요."
              className="w-full h-32 p-4 bg-gray-50 dark:bg-gray-800 border border-gray-200 dark:border-gray-700 rounded-2xl resize-none text-gray-800 dark:text-gray-200 focus:outline-none focus:ring-2 focus:ring-gray-900 dark:focus:ring-gray-100 focus:border-transparent transition-all text-sm placeholder:text-gray-400 dark:placeholder:text-gray-500"
            />
            
            <button
              type="submit"
              disabled={!diaryText.trim() || isAnalyzing}
              className="w-full py-4 bg-gray-900 dark:bg-gray-100 text-white dark:text-gray-900 rounded-2xl font-bold text-sm shadow-sm hover:bg-gray-800 dark:hover:bg-gray-200 transition-colors disabled:bg-gray-300 dark:disabled:bg-gray-800 disabled:text-gray-500 dark:disabled:text-gray-600 flex justify-center items-center gap-2"
            >
              {isAnalyzing ? (
                <>
                  <motion.div
                    animate={{ rotate: 360 }}
                    transition={{ duration: 1, repeat: Infinity, ease: "linear" }}
                  >
                    <Sparkles className="w-5 h-5" />
                  </motion.div>
                  <span>감정 분석 중...</span>
                </>
              ) : (
                <>
                  <Brain className="w-5 h-5" />
                  <span>AI 분석 및 향기 추천받기</span>
                </>
              )}
            </button>
          </form>
        </motion.div>

        <AnimatePresence>
          {result && (
            <motion.div
              initial={{ opacity: 0, scale: 0.95, y: 15 }}
              animate={{ opacity: 1, scale: 1, y: 0 }}
              exit={{ opacity: 0, scale: 0.95, y: -15 }}
              className={`bg-gradient-to-br ${result.color} rounded-[2.5rem] p-8 shadow-2xl border-none relative overflow-hidden transition-all duration-500`}
            >
              {/* 장식용 배경 요소 */}
              <div className="absolute right-[-5%] top-[-10%] w-56 h-56 bg-white/20 rounded-full blur-3xl -z-10" />
              <div className="absolute left-[-5%] bottom-[-10%] w-40 h-40 bg-black/5 rounded-full blur-2xl -z-10" />
              
              <div className="flex flex-col gap-8 relative z-10">
                <div className="flex justify-between items-start">
                  <div className="flex flex-col gap-1.5">
                    <div className="flex items-center gap-2 mb-1">
                      <span className="w-1 h-1 bg-white rounded-full animate-pulse" />
                      <span className="text-[10px] font-black uppercase tracking-[0.25em] opacity-80">AI HEART REPORT</span>
                    </div>
                    <div className="flex items-center gap-3">
                      <h3 className="text-4xl font-black tracking-tighter text-white">
                        {result.emotion}
                      </h3>
                      <span className={`px-3 py-1 rounded-full text-[10px] font-black border ${result.tagColor}`}>
                        분석 완료
                      </span>
                    </div>
                  </div>
                  <div className="bg-white/20 backdrop-blur-xl p-4 rounded-[1.5rem] border border-white/30 shadow-lg">
                    <Sparkles className="w-7 h-7 text-white" />
                  </div>
                </div>

                <div className="bg-white/10 backdrop-blur-md rounded-3xl p-6 border border-white/20">
                  <div className="flex items-center gap-2 mb-3">
                    <Quote className="w-4 h-4 text-white opacity-60 fill-current" />
                    <span className="text-[10px] font-black uppercase tracking-widest text-white opacity-80">Insight</span>
                  </div>
                  <p className="text-sm font-semibold leading-relaxed text-white opacity-95">
                    {result.reason}
                  </p>
                </div>

                <div className="flex flex-col gap-4">
                  <div className="flex flex-col gap-1.5">
                    <div className="flex items-center gap-2">
                      <div className="w-4 h-0.5 bg-white opacity-40 rounded-full" />
                      <span className="text-[10px] font-black uppercase tracking-[0.2em] text-white opacity-70">Recommend Scent</span>
                    </div>
                    <div className="flex items-center justify-between mt-1">
                      <div className="flex flex-col">
                        <span className="text-2xl font-black tracking-tight text-white drop-shadow-sm">
                          {result.scentName || "맞춤 향기"}
                        </span>
                        <span className="text-[10px] font-bold text-white opacity-60 mt-0.5">
                          {result.scent}
                        </span>
                      </div>
                      <div className="w-12 h-12 rounded-full bg-white/30 flex items-center justify-center border border-white/40 shadow-inner">
                        <Droplets className="w-6 h-6 text-white" />
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        <motion.div
          initial={{ opacity: 0, y: 10 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.1 }}
          className="bg-white dark:bg-gray-900 rounded-3xl p-6 shadow-sm border border-gray-200 dark:border-gray-800 mt-2 transition-colors duration-300"
        >
          <div className="flex items-center justify-between mb-6 relative z-20">
            <div className="flex items-center gap-2">
              <BookHeart className="w-5 h-5 text-gray-700 dark:text-gray-300" />
              <h2 className="text-lg font-bold text-gray-900 dark:text-white">지난 일기 기록</h2>
            </div>
            
            <div className="relative">
              <button 
                onClick={() => setIsDropdownOpen(!isDropdownOpen)}
                className="flex items-center gap-2 bg-gray-50 dark:bg-gray-800 hover:bg-gray-100 dark:hover:bg-gray-700 rounded-xl px-4 py-2 border border-gray-200 dark:border-gray-700 transition-colors"
              >
                <span className="text-sm font-bold text-gray-700 dark:text-gray-200">
                  {selectedDate ? selectedDate : "전체 날짜"}
                </span>
                <ChevronDown className={`w-4 h-4 text-gray-500 transition-transform ${isDropdownOpen ? 'rotate-180' : ''}`} />
              </button>

              <AnimatePresence>
                {isDropdownOpen && (
                  <>
                    <div 
                      className="fixed inset-0 z-20"
                      onClick={() => setIsDropdownOpen(false)}
                    />
                    <motion.div
                      initial={{ opacity: 0, y: -5, scale: 0.95 }}
                      animate={{ opacity: 1, y: 0, scale: 1 }}
                      exit={{ opacity: 0, y: -5, scale: 0.95 }}
                      transition={{ duration: 0.15 }}
                      className="absolute right-0 top-full mt-2 w-48 bg-white dark:bg-gray-900 border border-gray-200 dark:border-gray-700 rounded-2xl shadow-xl overflow-hidden z-30 ring-1 ring-black/5 dark:ring-white/10"
                    >
                      <div className="max-h-60 overflow-y-auto no-scrollbar py-1">
                        <button
                          onClick={() => {
                            setSelectedDate(null);
                            setIsDropdownOpen(false);
                          }}
                          className={`w-full text-left px-4 py-3 text-sm transition-colors ${!selectedDate ? 'font-bold text-gray-900 dark:text-white bg-gray-50 dark:bg-gray-800' : 'text-gray-600 dark:text-gray-400 hover:bg-gray-50 dark:hover:bg-gray-800'}`}
                        >
                          전체 날짜
                        </button>
                        {uniqueDates.map(date => (
                          <button
                            key={date}
                            onClick={() => {
                              setSelectedDate(date);
                              setIsDropdownOpen(false);
                            }}
                            className={`w-full text-left px-4 py-3 text-sm transition-colors ${selectedDate === date ? 'font-bold text-gray-900 dark:text-white bg-gray-50 dark:bg-gray-800' : 'text-gray-600 dark:text-gray-400 hover:bg-gray-50 dark:hover:bg-gray-800'}`}
                          >
                            {date}
                          </button>
                        ))}
                      </div>
                    </motion.div>
                  </>
                )}
              </AnimatePresence>
            </div>
          </div>
          
          <div className="flex flex-col gap-5">
            {history
              .filter((item) => !selectedDate || item.date === selectedDate)
              .map((item) => {
                let displayEmotion = item.emotion;
                if (!displayEmotion || displayEmotion === "분석중" || displayEmotion === "분석 완료") {
                  displayEmotion = "편안함";
                }
                
                return (
                  <div 
                    key={item.timestamp || item.id} 
                    className="group bg-gray-50/50 dark:bg-gray-800/30 rounded-[2rem] p-6 border border-gray-100 dark:border-gray-800/50 hover:border-gray-200 dark:hover:border-gray-700 transition-all duration-300"
                  >
                    <div className="flex items-center justify-between mb-4">
                      <div className="flex flex-col gap-1">
                        <span className="text-[10px] font-black text-gray-400 dark:text-gray-500 uppercase tracking-[0.15em]">
                          {item.date}
                        </span>
                        <div className="flex items-center gap-2 mt-1">
                          <span className={`px-3 py-1 rounded-full text-[10px] font-extrabold shadow-sm ${getHistoryEmotionColor(displayEmotion)}`}>
                            {displayEmotion}
                          </span>
                        </div>
                      </div>
                      <div className="w-10 h-10 rounded-2xl bg-white dark:bg-gray-900 flex items-center justify-center shadow-sm border border-gray-100 dark:border-gray-800 group-hover:scale-110 transition-transform duration-300">
                        <Sparkles className="w-4 h-4 text-gray-400 dark:text-gray-500" />
                      </div>
                    </div>

                    <p className="text-gray-900 dark:text-gray-100 text-lg font-semibold leading-relaxed tracking-tight mb-4 whitespace-pre-wrap transition-colors">
                      {item.text}
                    </p>

                    <div className="pt-4 border-t border-gray-100 dark:border-gray-800/50 flex items-center gap-3">
                      <div className="flex -space-x-1">
                        <div className="w-5 h-5 rounded-full bg-gray-200 dark:bg-gray-700 border-2 border-white dark:border-gray-900 flex items-center justify-center">
                          <Droplets className="w-2.5 h-2.5 text-gray-500 dark:text-gray-400" />
                        </div>
                      </div>
                      <span className="text-[11px] font-medium text-gray-500 dark:text-gray-400 italic">
                        {item.scent}
                      </span>
                    </div>
                  </div>
                );
              })}
          </div>
        </motion.div>
      </div>

      <BottomNav />
    </div>
  );
}
