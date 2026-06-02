import React, { useState, useEffect } from "react";
import { Link } from "react-router";
import { BottomNav } from "./BottomNav";
import { Home as HomeIcon, Droplets, SlidersHorizontal, Settings, BookHeart, Brain, Sparkles, Search, ChevronDown, ThumbsUp, ThumbsDown } from "lucide-react";
import { motion, AnimatePresence } from "motion/react";
import { useDiary } from "../store/DiaryContext";
import { useAuth } from "../store/AuthContext";
import { useDevice } from "../store/DeviceContext";

export function DiaryScreen() {
  const { diaries, fetchDiaries } = useDiary();
  const { currentUser } = useAuth();
  const { sendDeviceData } = useDevice();
  const [diaryText, setDiaryText] = useState("");
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [result, setResult] = useState<{ emotion: string; scent: string; color: string; tagColor: string; spray?: number } | null>(null);
  const [feedback, setFeedback] = useState<"like" | "dislike" | null>(null);
  
  const [selectedDate, setSelectedDate] = useState<string | null>(null);
  const [isDropdownOpen, setIsDropdownOpen] = useState(false);

  // 화면 진입 시 서버에서 일기 내역 가져오기
  useEffect(() => {
    if (currentUser?.email) {
      fetchDiaries(currentUser.email);
    }
  }, [currentUser?.email]);

  const history = currentUser && diaries[currentUser.email] ? diaries[currentUser.email] : [];
  const uniqueDates = Array.from(new Set(history.map(item => item.date)));

  const handleFeedback = async (type: "like" | "dislike") => {
    if (!result || !currentUser) return;
    setFeedback(type);
    
    // 백엔드 FEEDBACK 액션 형식에 맞게 데이터 준비
    // value: 1 (좋아요), -1 (별로예요)
    const val = type === "like" ? 1 : -1;
    const context = `Emotion_${result.emotion}`;
    const sprayNo = result.spray || 1;
    
    try {
      await sendDeviceData("FEEDBACK", val, `${sprayNo}_${context}`);
      alert("피드백이 반영되었습니다. 감사합니다!");
    } catch (err) {
      console.error("Feedback failed", err);
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!diaryText.trim() || !currentUser) return;

    setIsAnalyzing(true);
    setResult(null);

    try {
      // 인자: action, value, region(emotion tag), diaryText
      // 태그는 AI가 서버에서 직접 생성하므로 placeholder 전달
      const res = await sendDeviceData("AI_EMOTION", 0, "분석중", diaryText);

      if (res.success) {
        // AI 분석 결과 설정 (서버 응답 활용)
        const mockResults: Record<string, any> = {
          "신남": { emotion: "신남", scent: "상쾌한 시트러스", color: "from-orange-100 to-yellow-50 dark:from-orange-900/40 dark:to-yellow-900/20 text-orange-700 dark:text-orange-300 border-orange-200 dark:border-orange-800", tagColor: "bg-orange-100 dark:bg-orange-900/30 text-orange-700 dark:text-orange-400" },
          "편안함": { emotion: "편안함", scent: "라벤더 & 센달우드", color: "from-violet-100 to-fuchsia-50 dark:from-violet-900/40 dark:to-fuchsia-900/20 text-violet-700 dark:text-violet-300 border-violet-200 dark:border-violet-800", tagColor: "bg-violet-100 dark:bg-violet-900/30 text-violet-700 dark:text-violet-400" },
          "화남": { emotion: "화남", scent: "차분한 우디", color: "from-blue-100 to-slate-50 dark:from-blue-900/40 dark:to-slate-900/20 text-blue-700 dark:text-blue-300 border-blue-200 dark:border-blue-800", tagColor: "bg-blue-100 dark:bg-blue-900/30 text-blue-700 dark:text-blue-400" },
          "슬픔": { emotion: "슬픔", scent: "따뜻한 머스크", color: "from-pink-100 to-rose-50 dark:from-pink-900/40 dark:to-rose-900/20 text-pink-700 dark:text-pink-300 border-pink-200 dark:border-pink-800", tagColor: "bg-pink-100 dark:bg-pink-900/30 text-pink-700 dark:text-pink-400" },
        };

        // 서버에서 반환한 context(Emotion_태그) 또는 별도의 emotion_summary 등에서 태그 추출
        // res.context 형태: "Emotion_신남"
        let detectedEmotion = (res.context || "").replace("Emotion_", "").trim() || "편안함";
        
        // 만약 서버에서 "분석중"이라는 태그를 잘못 반환했다면 기본값으로 대체
        if (detectedEmotion === "분석중") detectedEmotion = "편안함";
        
        const resultData = mockResults[detectedEmotion] || mockResults["편안함"];

        setResult({
          ...resultData,
          scentName: resultData.scent, // 원래 향기 이름 보존
          reason: res.message || `${resultData.emotion} 감정에 어울리는 특별한 향기를 준비했어요.`, // 분석 이유
          spray: res.spray
        });
        setFeedback(null); // 새로운 결과가 나오면 피드백 초기화

        // ★ 핵심: 서버에 저장된 최신 내역을 즉시 다시 불러오기 (DB 동기화)
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

  const getEmotionColor = (emotion: string) => {
    const colors: Record<string, string> = {
      "신남": "bg-orange-100 dark:bg-orange-900/30 text-orange-700 dark:text-orange-400",
      "편안함": "bg-violet-100 dark:bg-violet-900/30 text-violet-700 dark:text-violet-400",
      "화남": "bg-blue-100 dark:bg-blue-900/30 text-blue-700 dark:text-blue-400",
      "슬픔": "bg-pink-100 dark:bg-pink-900/30 text-pink-700 dark:text-pink-400",
    };
    return colors[emotion] || "bg-gray-100 dark:bg-gray-800 text-gray-600 dark:text-gray-400";
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
              initial={{ opacity: 0, scale: 0.95, y: 10 }}
              animate={{ opacity: 1, scale: 1, y: 0 }}
              exit={{ opacity: 0, scale: 0.95, y: -10 }}
              className={`bg-gradient-to-br ${result.color} rounded-[2.5rem] p-8 shadow-xl border-none relative overflow-hidden transition-colors duration-300`}
            >
              <div className="absolute right-[-10%] top-[-10%] w-48 h-48 bg-white/20 dark:bg-black/10 rounded-full blur-3xl -z-10" />
              
              <div className="flex flex-col gap-6 relative z-10">
                <div className="flex justify-between items-start">
                  <div className="flex flex-col gap-1">
                    <span className="text-[10px] font-black uppercase tracking-[0.2em] opacity-60">AI Emotion Analysis</span>
                    <h3 className="text-3xl font-black tracking-tighter">
                      {result.emotion}
                    </h3>
                  </div>
                  <div className="bg-white/30 dark:bg-black/20 backdrop-blur-md p-3 rounded-2xl">
                    <Brain className="w-6 h-6" />
                  </div>
                </div>

                <div className="space-y-1">
                  <span className="text-[10px] font-black uppercase tracking-[0.2em] opacity-60 italic">Recommended Scent</span>
                  <div className="flex items-baseline gap-2">
                    <span className="text-xl font-bold tracking-tight">{result.scent}</span>
                  </div>
                </div>

                <div className="flex items-center gap-3 pt-2 border-t border-white/20 dark:border-black/10">
                  <button
                    onClick={() => handleFeedback("like")}
                    disabled={feedback !== null}
                    className={`flex-1 flex items-center justify-center gap-2 py-3 rounded-2xl text-[11px] font-black transition-all ${feedback === "like" ? "bg-white dark:bg-gray-800 text-gray-900 dark:text-white shadow-lg scale-[1.02]" : "bg-white/20 hover:bg-white/30 text-white backdrop-blur-sm"}`}
                  >
                    <ThumbsUp className={`w-3.5 h-3.5 ${feedback === "like" ? "fill-current" : ""}`} />
                    <span className="whitespace-nowrap">좋아요</span>
                  </button>
                  <button
                    onClick={() => handleFeedback("dislike")}
                    disabled={feedback !== null}
                    className={`flex-1 flex items-center justify-center gap-2 py-3 rounded-2xl text-[11px] font-black transition-all ${feedback === "dislike" ? "bg-black/40 dark:bg-gray-800 text-white shadow-lg scale-[1.02]" : "bg-black/10 hover:bg-black/20 text-white backdrop-blur-sm"}`}
                  >
                    <ThumbsDown className={`w-3.5 h-3.5 ${feedback === "dislike" ? "fill-current" : ""}`} />
                    <span className="whitespace-nowrap">별로예요</span>
                  </button>
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
                // 만약 emotion이 "분석중"이라면 "편안함" 등으로 표시하거나 보정
                const displayEmotion = item.emotion === "분석중" ? "분석 완료" : item.emotion;
                
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
                          <span className={`px-3 py-1 rounded-full text-[10px] font-extrabold shadow-sm ${item.color || getEmotionColor(displayEmotion)}`}>
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
