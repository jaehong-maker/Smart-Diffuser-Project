바import React, { useState } from "react";
import { Link } from "react-router";
import { motion, AnimatePresence } from "motion/react";
import { Home, Droplets, SlidersHorizontal, RefreshCw, X, CheckCircle2 } from "lucide-react";

export default function ScentsScreen() {
  const [cartridges, setCartridges] = useState([
    { id: 1, scent: "시트러스", level: 85, color: "bg-orange-400" },
    { id: 2, scent: "라벤더", level: 42, color: "bg-purple-400" },
    { id: 3, scent: "우디", level: 15, color: "bg-stone-500" },
    { id: 4, scent: "아쿠아", level: 90, color: "bg-blue-400" },
  ]);

  const [replacingId, setReplacingId] = useState<number | null>(null);
  
  const availableScents = [
    { name: "시트러스", color: "bg-orange-400" },
    { name: "라벤더", color: "bg-purple-400" },
    { name: "우디", color: "bg-stone-500" },
    { name: "아쿠아", color: "bg-blue-400" },
    { name: "유칼립투스", color: "bg-emerald-500" },
    { name: "로즈마리", color: "bg-teal-500" },
    { name: "자스민", color: "bg-indigo-400" },
  ];

  const handleReplace = (newName: string, newColor: string) => {
    if (replacingId === null) return;
    setCartridges(prev => prev.map(c => c.id === replacingId ? { ...c, scent: newName, level: 100, color: newColor } : c));
    setReplacingId(null);
  };

  return (
    <div className="flex-1 flex flex-col bg-gray-50 overflow-y-auto relative pb-24 min-h-screen">
      <header className="px-6 pt-12 pb-4 flex justify-between items-center z-10 sticky top-0 bg-gray-50/90 backdrop-blur-md">
        <div>
          <h1 className="text-2xl font-bold tracking-tight text-gray-900">향기 관리</h1>
          <p className="text-sm font-medium text-gray-500 mt-1">디퓨저 카트리지 상태 및 교체</p>
        </div>
      </header>

      <div className="px-6 py-4 flex flex-col gap-4">
        {cartridges.map((cart) => (
          <motion.div 
            key={cart.id} initial={{ opacity: 0, y: 10 }} animate={{ opacity: 1, y: 0 }}
            className="bg-white rounded-3xl p-5 shadow-sm border border-gray-100 flex flex-col gap-4"
          >
            <div className="flex justify-between items-start">
              <div className="flex items-center gap-3">
                <div className={`w-10 h-10 rounded-xl flex items-center justify-center text-white font-bold ${cart.color}`}>
                  {cart.id}
                </div>
                <div>
                  <h3 className="text-lg font-bold text-gray-900">{cart.scent}</h3>
                  <p className="text-sm font-semibold text-gray-400">슬롯 {cart.id}</p>
                </div>
              </div>
              <button 
                onClick={() => setReplacingId(cart.id)}
                className="w-10 h-10 bg-gray-50 rounded-full flex items-center justify-center text-gray-600 hover:bg-gray-100 transition-colors"
              >
                <RefreshCw className="w-5 h-5" />
              </button>
            </div>

            <div>
              <div className="flex justify-between items-center text-sm mb-2">
                <span className="font-semibold text-gray-500">잔량</span>
                <span className={`font-bold ${cart.level < 20 ? 'text-red-500' : 'text-gray-900'}`}>{cart.level}%</span>
              </div>
              <div className="h-2 w-full bg-gray-100 rounded-full overflow-hidden">
                <motion.div initial={{ width: 0 }} animate={{ width: `${cart.level}%` }} className={`h-full rounded-full ${cart.color}`} />
              </div>
            </div>
          </motion.div>
        ))}
      </div>

      <AnimatePresence>
        {replacingId !== null && (
          <div className="fixed inset-0 z-50 flex items-end justify-center">
            <motion.div 
              initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }}
              onClick={() => setReplacingId(null)}
              className="absolute inset-0 bg-gray-900/40 backdrop-blur-sm"
            />
            <motion.div 
              initial={{ y: "100%" }} animate={{ y: 0 }} exit={{ y: "100%" }}
              transition={{ type: "spring", damping: 25, stiffness: 200 }}
              className="relative w-full bg-white rounded-t-[2.5rem] p-6 pb-12 shadow-2xl"
            >
              <div className="flex justify-between items-center mb-6">
                <div>
                  <h2 className="text-xl font-bold text-gray-900">새 향기 등록</h2>
                  <p className="text-sm text-gray-500 font-medium">슬롯 {replacingId}에 장착할 카트리지를 선택하세요</p>
                </div>
                <button onClick={() => setReplacingId(null)} className="w-8 h-8 bg-gray-100 rounded-full flex items-center justify-center text-gray-600">
                  <X className="w-5 h-5" />
                </button>
              </div>

              <div className="grid grid-cols-2 gap-3 max-h-[300px] overflow-y-auto pr-2">
                {availableScents.map((scent) => (
                  <button
                    key={scent.name}
                    onClick={() => handleReplace(scent.name, scent.color)}
                    className="flex items-center gap-3 p-4 border border-gray-100 rounded-2xl hover:bg-gray-50 transition-colors"
                  >
                    <div className={`w-8 h-8 rounded-full ${scent.color} flex items-center justify-center`}>
                      <CheckCircle2 className="w-4 h-4 text-white opacity-0" />
                    </div>
                    <span className="font-bold text-gray-700">{scent.name}</span>
                  </button>
                ))}
              </div>
            </motion.div>
          </div>
        )}
      </AnimatePresence>

      <nav className="fixed bottom-0 left-0 right-0 bg-white/90 backdrop-blur-md border-t border-gray-100 px-6 pb-6 pt-3 flex justify-around items-center z-20">
        <Link to="/home" className="flex flex-col items-center gap-1 text-gray-400 hover:text-gray-900 transition-colors">
          <Home className="w-6 h-6" />
          <span className="text-[10px] font-bold">홈</span>
        </Link>
        <Link to="/modes" className="flex flex-col items-center gap-1 text-gray-400 hover:text-gray-900 transition-colors">
          <SlidersHorizontal className="w-6 h-6" />
          <span className="text-[10px] font-bold">제어</span>
        </Link>
        <Link to="/scents" className="flex flex-col items-center gap-1 text-gray-900 transition-colors">
          <Droplets className="w-6 h-6" />
          <span className="text-[10px] font-bold">향기 관리</span>
        </Link>
      </nav>
    </div>
  );
}