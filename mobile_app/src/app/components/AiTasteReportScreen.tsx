import React, { useState, useEffect } from "react";
import { useNavigate } from "react-router";
import { motion, AnimatePresence } from "motion/react";
import { 
  ChevronLeft, 
  BookHeart, 
  BarChart3, 
  Sparkles, 
  Trophy, 
  CloudSun,
  RefreshCw,
  AlertCircle,
  ThumbsUp
} from "lucide-react";
import { useAuth } from "../store/AuthContext";
import { apiSendData } from "../utils/api";

interface ReportData {
  result: string;
  summary: string;
  total_analyzed: number;
  top_scent: string;
  top_context: string;
}

export function AiTasteReportScreen() {
  const navigate = useNavigate();
  const { currentUser } = useAuth();
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [data, setData] = useState<ReportData | null>(null);

  const fetchReport = async () => {
    if (!currentUser) return;
    setLoading(true);
    setError(null);
    try {
      // 최신 API 가이드에 따라 action: "REPORT", email 전달
      const response = await apiSendData({
        action: "REPORT",
        email: currentUser.email
      });
      
      if (response && response.result === "SUCCESS") {
        setData(response);
      } else {
        setError("리포트를 불러오는 데 실패했습니다.");
      }
    } catch (err) {
      console.error("Fetch Report Error:", err);
      setError("서버와의 통신 중 오류가 발생했습니다.");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchReport();
  }, [currentUser]);

  const hasData = data && data.total_analyzed > 0;

  return (
    <div key="ai-report-screen" className="flex-1 flex flex-col bg-white dark:bg-gray-950 overflow-y-auto relative pb-12 min-h-screen transition-colors duration-500">
      {/* 프리미엄 배경 글로우 */}
      <div className="absolute top-0 left-0 right-0 h-[50vh] overflow-hidden pointer-events-none">
        <div className="absolute inset-0 bg-gradient-to-b from-blue-50/50 dark:from-blue-900/10 to-transparent" />
        <motion.div 
          animate={{ 
            scale: [1, 1.1, 1],
            opacity: [0.1, 0.15, 0.1]
          }}
          transition={{ duration: 8, repeat: Infinity, ease: "easeInOut" }}
          className="absolute -top-24 -right-24 w-96 h-96 bg-blue-400 blur-[100px] rounded-full"
        />
      </div>

      <header className="px-6 pt-12 pb-4 flex items-center gap-4 z-20 relative">
        <button 
          onClick={() => navigate(-1)}
          className="w-10 h-10 rounded-full flex items-center justify-center bg-white/80 dark:bg-gray-900/80 shadow-sm border border-gray-200 dark:border-gray-800 transition-transform active:scale-95"
        >
          <ChevronLeft className="w-5 h-5 text-gray-700 dark:text-gray-300" />
        </button>
        <div>
          <h1 className="text-xl font-black tracking-tight text-gray-900 dark:text-white uppercase">Scent Report</h1>
          <p className="text-[10px] font-bold text-blue-500 dark:text-blue-400 tracking-widest uppercase">AI Preference Analysis</p>
        </div>
      </header>

      <main className="px-6 py-4 flex flex-col gap-6 z-10 relative">
        <AnimatePresence mode="wait">
          {loading ? (
            <motion.div 
              key="loading"
              initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }}
              className="flex flex-col items-center justify-center py-32 gap-6"
            >
              <div className="relative">
                <RefreshCw className="w-10 h-10 text-blue-500 animate-spin" />
                <div className="absolute inset-0 blur-lg bg-blue-500/20 animate-pulse" />
              </div>
              <p className="text-sm font-black text-gray-400 uppercase tracking-widest animate-pulse">Analyzing Preference...</p>
            </motion.div>
          ) : error ? (
            <motion.div 
              key="error"
              initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }} exit={{ opacity: 0 }}
              className="bg-red-50 dark:bg-red-900/10 border border-red-100 dark:border-red-900/20 rounded-[2.5rem] p-10 flex flex-col items-center gap-6 text-center"
            >
              <AlertCircle className="w-12 h-12 text-red-500" />
              <div className="flex flex-col gap-2">
                <h3 className="font-black text-red-900 dark:text-red-400 uppercase tracking-tight">System Error</h3>
                <p className="text-sm font-medium text-red-700 dark:text-red-500 break-keep">{error}</p>
              </div>
              <button 
                onClick={fetchReport}
                className="px-8 py-3 bg-red-500 text-white rounded-2xl text-xs font-black tracking-widest shadow-xl shadow-red-500/20 active:scale-95 transition-transform uppercase"
              >
                Retry
              </button>
            </motion.div>
          ) : data ? (
            <motion.div 
              key="content"
              initial={{ opacity: 0, y: 20 }} animate={{ opacity: 1, y: 0 }}
              className="flex flex-col gap-6"
            >
              {/* Summary Card */}
              <div className="bg-white/80 dark:bg-gray-900/80 backdrop-blur-2xl border border-white dark:border-gray-800 rounded-[2.5rem] p-8 shadow-2xl shadow-blue-500/5 overflow-hidden relative">
                {/* Decorative Element */}
                <div className="absolute -top-10 -right-10 w-32 h-32 bg-blue-500/5 rounded-full blur-3xl" />
                
                <div className="flex items-center gap-3 mb-8">
                  <div className="w-10 h-10 bg-blue-500 text-white rounded-2xl flex items-center justify-center shadow-lg shadow-blue-500/20">
                    <Sparkles className="w-5 h-5" />
                  </div>
                  <h3 className="font-black text-gray-900 dark:text-white uppercase tracking-[0.2em] text-[10px]">AI Insight</h3>
                </div>

                <div className="min-h-[80px] flex items-center">
                  <p className="text-xl font-bold leading-relaxed text-gray-800 dark:text-gray-100 break-keep">
                    {data.summary}
                  </p>
                </div>

                <div className="mt-10 pt-8 border-t border-gray-100 dark:border-gray-800 flex justify-between items-center">
                  <div className="flex flex-col gap-1">
                    <span className="text-[9px] font-black text-gray-400 uppercase tracking-[0.15em]">Total Feedback</span>
                    <div className="flex items-center gap-2">
                      <ThumbsUp className="w-3 h-3 text-blue-500" />
                      <span className="text-base font-black text-gray-900 dark:text-white">
                        {data.total_analyzed} <span className="text-xs font-bold text-gray-400 ml-0.5">points</span>
                      </span>
                    </div>
                  </div>
                  <div className="px-4 py-2 bg-blue-50 dark:bg-blue-900/30 rounded-xl border border-blue-100 dark:border-blue-900/20">
                    <span className="text-[10px] font-black text-blue-600 dark:text-blue-400 uppercase tracking-widest">Verified</span>
                  </div>
                </div>
              </div>

              {/* Grid Stats */}
              <div className="grid grid-cols-2 gap-4">
                <div className="bg-white/70 dark:bg-gray-900/70 backdrop-blur-xl border border-white dark:border-gray-800 rounded-[2.2rem] p-6 flex flex-col gap-5 shadow-sm transition-transform active:scale-[0.98]">
                  <div className="w-10 h-10 bg-amber-500/10 text-amber-500 rounded-2xl flex items-center justify-center">
                    <Trophy className="w-5 h-5" />
                  </div>
                  <div>
                    <p className="text-[9px] font-black text-gray-400 uppercase tracking-[0.2em] mb-1.5">Top Scent</p>
                    <h4 className="text-base font-black text-gray-900 dark:text-white truncate">
                      {data.top_scent}
                    </h4>
                  </div>
                </div>
                
                <div className="bg-white/70 dark:bg-gray-900/70 backdrop-blur-xl border border-white dark:border-gray-800 rounded-[2.2rem] p-6 flex flex-col gap-5 shadow-sm transition-transform active:scale-[0.98]">
                  <div className="w-10 h-10 bg-purple-500/10 text-purple-500 rounded-2xl flex items-center justify-center">
                    <CloudSun className="w-5 h-5" />
                  </div>
                  <div>
                    <p className="text-[9px] font-black text-gray-400 uppercase tracking-[0.2em] mb-1.5">Best Context</p>
                    <h4 className="text-base font-black text-gray-900 dark:text-white truncate">
                      {data.top_context}
                    </h4>
                  </div>
                </div>
              </div>

              {/* Tip Section */}
              <div className="bg-gray-50/50 dark:bg-gray-900/50 backdrop-blur-sm rounded-[2rem] p-6 border border-gray-100 dark:border-gray-800 mt-2">
                <div className="flex gap-4 items-start">
                  <div className="w-8 h-8 bg-white dark:bg-gray-800 rounded-xl flex items-center justify-center shadow-sm border border-gray-100 dark:border-gray-700 shrink-0">
                    <BarChart3 className="w-4 h-4 text-gray-400" />
                  </div>
                  <div className="flex flex-col gap-1.5">
                    <h5 className="text-xs font-black text-gray-800 dark:text-gray-200 uppercase tracking-wider">How to improve accuracy?</h5>
                    <p className="text-[11px] font-medium text-gray-500 dark:text-gray-400 leading-relaxed break-keep">
                      디퓨저 사용 중 남겨주시는 피드백이 많아질수록 AI가 당신의 취향을 더 정밀하게 학습합니다. 다양한 상황에서 피드백을 남겨보세요.
                    </p>
                  </div>
                </div>
              </div>
            </motion.div>
          ) : null}
        </AnimatePresence>
      </main>
    </div>
  );
}