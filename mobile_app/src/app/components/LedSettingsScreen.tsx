import React, { useState, useEffect, useRef } from "react";
import { motion, AnimatePresence } from "motion/react";
import { Lightbulb, Power, SlidersHorizontal, RefreshCw, Sparkles } from "lucide-react";
import { BottomNav } from "./BottomNav";
import { useDevice } from "../store/DeviceContext";
import { useAuth } from "../store/AuthContext";

// Hex to RGB 변환 유틸리티
function hexToRgb(hex: string) {
  const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
  return result ? {
    r: parseInt(result[1], 16),
    g: parseInt(result[2], 16),
    b: parseInt(result[3], 16)
  } : { r: 255, g: 255, b: 255 };
}

// HSL to Hex 변환 유틸리티 함수
function hslToHex(h: number, s: number, l: number) {
  h = Math.round(h);
  s = Math.round(s);
  l = Math.round(l);
  l /= 100;
  const a = s * Math.min(l, 1 - l) / 100;
  const f = (n: number) => {
    const k = (n + h / 30) % 12;
    const color = l - a * Math.max(Math.min(k - 3, 9 - k, 1), -1);
    return Math.round(255 * color).toString(16).padStart(2, '0');
  };
  return `#${f(0)}${f(8)}${f(4)}`;
}

interface ColorWheelProps {
  color: string;
  thumbPos: { x: number, y: number };
  setThumbPos: (pos: { x: number, y: number }) => void;
  onChange: (hex: string) => void;
  disabled: boolean;
  onInteractionStart: () => void;
}

function ColorWheel({ color, thumbPos, setThumbPos, onChange, disabled, onInteractionStart }: ColorWheelProps) {
  const wheelRef = useRef<HTMLDivElement>(null);
  const [isDragging, setIsDragging] = useState(false);

  const updateColor = (e: React.PointerEvent | PointerEvent) => {
    if (!wheelRef.current || disabled) return;
    const rect = wheelRef.current.getBoundingClientRect();
    
    let x = e.clientX - rect.left;
    let y = e.clientY - rect.top;
    const cx = rect.width / 2;
    const cy = rect.height / 2;

    const dx = x - cx;
    const dy = y - cy;
    let distance = Math.sqrt(dx * dx + dy * dy);
    
    if (distance > cx) {
      distance = cx;
    }
    
    let angle = Math.atan2(dy, dx) * (180 / Math.PI) + 90;
    if (angle < 0) angle += 360;

    const distNormalized = distance / cx;
    const lightness = 100 - (distNormalized * 50); 
    const hue = angle;
    
    const constrainedX = cx + Math.sin(angle * Math.PI / 180) * distance;
    const constrainedY = cy - Math.cos(angle * Math.PI / 180) * distance;

    setThumbPos({
      x: (constrainedX / rect.width) * 100,
      y: (constrainedY / rect.height) * 100,
    });

    onChange(hslToHex(hue, 100, lightness));
  };

  useEffect(() => {
    const handlePointerMove = (e: PointerEvent) => {
      if (isDragging) updateColor(e);
    };
    const handlePointerUp = () => setIsDragging(false);

    if (isDragging) {
      window.addEventListener('pointermove', handlePointerMove);
      window.addEventListener('pointerup', handlePointerUp);
    }
    return () => {
      window.removeEventListener('pointermove', handlePointerMove);
      window.removeEventListener('pointerup', handlePointerUp);
    };
  }, [isDragging]);

  return (
    <div 
      ref={wheelRef}
      className={`relative w-full aspect-square max-w-[260px] mx-auto rounded-full touch-none select-none shadow-inner transition-opacity duration-300 ${disabled ? 'opacity-30 cursor-not-allowed' : 'cursor-crosshair'}`}
      style={{
        background: 'conic-gradient(red, yellow, lime, cyan, blue, magenta, red)'
      }}
      onPointerDown={(e) => {
        if (!disabled) {
          onInteractionStart();
          setIsDragging(true);
          (e.target as HTMLElement).setPointerCapture(e.pointerId);
          updateColor(e);
        }
      }}
    >
      <div 
        className="absolute inset-0 rounded-full pointer-events-none"
        style={{
          background: 'radial-gradient(circle at center, white 0%, transparent 100%)'
        }}
      />
      <div 
        className="absolute w-8 h-8 -ml-4 -mt-4 rounded-full border-[3px] border-white shadow-[0_2px_8px_rgba(0,0,0,0.4)] pointer-events-none z-10"
        style={{
          left: `${thumbPos.x}%`,
          top: `${thumbPos.y}%`,
          backgroundColor: color,
          transform: isDragging ? 'scale(1.2)' : 'scale(1)',
          transition: isDragging ? 'none' : 'all 0.3s ease-out'
        }}
      />
    </div>
  );
}

export function LedSettingsScreen() {
  const { currentUser } = useAuth();
  const { isLedOn, ledColor, ledBrightness, sendDeviceData } = useDevice();
  
  const [isOn, setIsOn] = useState(false);
  const [color, setColor] = useState("#FBBF24");
  const [brightness, setBrightness] = useState(80);
  const [effect, setEffect] = useState(0); // 0: Solid, 1: Breathe, 2: Rainbow, 3: Sync
  const [thumbPos, setThumbPos] = useState({ x: 75, y: 25 });
  
  const [isSyncing, setIsSyncing] = useState(false);
  const isInteractingRef = useRef(false);
  const lastInteractionTimeRef = useRef(0);

  // 서버 상태가 변경되면 로컬 UI에 반영 (사용자가 조작 중이 아닐 때만)
  useEffect(() => {
    if (Date.now() - lastInteractionTimeRef.current > 5000) {
      if (isLedOn !== isOn) setIsOn(isLedOn);
      if (ledColor && ledColor !== color) setColor(ledColor);
      if (ledBrightness !== undefined && ledBrightness !== brightness) setBrightness(ledBrightness);
    }
  }, [isLedOn, ledColor, ledBrightness]);

  const syncTimerRef = useRef<NodeJS.Timeout | null>(null);

  const syncWithHardware = (r: number, g: number, b: number, br: number, powerOn: boolean, eff: number) => {
    if (!currentUser) return;
    
    if (syncTimerRef.current) clearTimeout(syncTimerRef.current);
    
    syncTimerRef.current = setTimeout(async () => {
      // 사용자가 조작했을 때만 서버로 전송
      if (isInteractingRef.current) {
        setIsSyncing(true);
        try {
          const targetBr = powerOn ? Math.round((br / 100) * 255) : 0;
          await sendDeviceData("SET_LED", 0, undefined, undefined, { r, g, b, br: targetBr, effect: eff } as any);
        } catch (err) {
          console.error("LED Sync Error:", err);
        } finally {
          setIsSyncing(false);
          isInteractingRef.current = false;
        }
      }
    }, 300);
  };

  const markInteraction = () => {
    isInteractingRef.current = true;
    lastInteractionTimeRef.current = Date.now();
  };

  useEffect(() => {
    const rgb = hexToRgb(color);
    syncWithHardware(rgb.r, rgb.g, rgb.b, brightness, isOn, effect);
  }, [color, brightness, isOn, effect]);

  return (
    <div key="led-screen" className="flex-1 flex flex-col bg-white dark:bg-gray-950 overflow-x-hidden relative pb-32 min-h-screen transition-colors duration-500">
      <div className="absolute top-0 left-0 right-0 h-[45vh] pointer-events-none overflow-hidden">
        <motion.div 
          animate={{ 
            background: isOn 
              ? `radial-gradient(circle at 50% 40%, ${color}33 0%, transparent 70%)` 
              : `radial-gradient(circle at 50% 40%, transparent 0%, transparent 70%)`
          }}
          className="absolute inset-0 transition-all duration-1000"
        />
        <div className="absolute bottom-0 left-0 right-0 h-32 bg-gradient-to-t from-white dark:from-gray-950 to-transparent" />
      </div>

      <header className="px-6 pt-12 pb-2 flex justify-between items-center z-20 relative">
        <div>
          <h1 className="text-2xl font-bold tracking-tight text-gray-900 dark:text-white uppercase tracking-tighter">LED Control</h1>
        </div>
      </header>

      <div className="px-6 py-4 flex flex-col gap-6 z-10 relative">
        <div className="flex flex-col items-center justify-center py-10 relative min-h-[280px]">
          <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
            <AnimatePresence>
              {isOn && (
                <motion.div
                  initial={{ opacity: 0, scale: 0.9 }}
                  animate={{ opacity: 1, scale: 1 }}
                  exit={{ opacity: 0, scale: 0.9 }}
                  className="absolute w-[320px] h-[320px] rounded-full"
                  style={{ 
                    background: `radial-gradient(circle, ${color}4d 0%, transparent 75%)`,
                    filter: 'blur(10px)'
                  }}
                />
              )}
            </AnimatePresence>
          </div>

          <motion.div 
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
            onClick={() => { markInteraction(); setIsOn(!isOn); }}
            className={`relative z-10 w-28 h-28 rounded-full flex items-center justify-center cursor-pointer transition-all duration-700 shadow-2xl ${
              isOn 
                ? "border-[0.5px] border-white/40" 
                : "bg-gray-50 border-gray-100 dark:bg-gray-900 dark:border-gray-800"
            }`}
            style={isOn ? { 
              backgroundColor: color,
              boxShadow: `0 15px 45px ${color}66, inset 0 0 20px rgba(255,255,255,0.4)`
            } : {
              boxShadow: "0 10px 30px rgba(0,0,0,0.02), inset 0 2px 5px rgba(255,255,255,0.9)"
            }}
          >
            <Lightbulb className={`w-10 h-10 transition-all duration-700 ${isOn ? "text-white drop-shadow-[0_0_10px_rgba(255,255,255,0.8)]" : "text-gray-300"}`} />
          </motion.div>

          <div className="mt-12 flex bg-gray-100/80 dark:bg-gray-900/80 backdrop-blur-md p-1 rounded-2xl w-40 shadow-inner z-10 relative border border-gray-200/50 dark:border-gray-800/50">
            <button
              onClick={() => { markInteraction(); setIsOn(true); }}
              className={`flex-1 py-2.5 rounded-xl text-[10px] font-black tracking-widest transition-all duration-300 ${
                isOn 
                  ? "bg-white dark:bg-gray-800 text-gray-900 dark:text-white shadow-md" 
                  : "text-gray-400 hover:text-gray-500"
              }`}
            >
              ON
            </button>
            <button
              onClick={() => { markInteraction(); setIsOn(false); }}
              className={`flex-1 py-2.5 rounded-xl text-[10px] font-black tracking-widest transition-all duration-300 ${
                !isOn 
                  ? "bg-white dark:bg-gray-800 text-gray-900 dark:text-white shadow-md" 
                  : "text-gray-400 hover:text-gray-500"
              }`}
            >
              OFF
            </button>
          </div>
        </div>

        <div className="grid grid-cols-1 gap-6">
          <div className="bg-white/70 dark:bg-gray-900/70 backdrop-blur-xl rounded-[2.5rem] p-8 shadow-xl border border-white/40 dark:border-gray-800/40 transition-all duration-500">
            <div className="flex items-center justify-between mb-8">
              <div>
                <h3 className="text-sm font-bold text-gray-900 dark:text-white uppercase tracking-wider">Mood Color</h3>
                <p className="text-[10px] text-gray-500 mt-1">원하는 색상을 선택해 보세요</p>
              </div>
              {isOn && (
                <span className="text-[10px] font-black tracking-widest text-gray-400 bg-gray-100 dark:bg-gray-800 px-3 py-1.5 rounded-full border border-gray-200/50 dark:border-gray-700/50">
                  {color.toUpperCase()}
                </span>
              )}
            </div>
            
            <ColorWheel 
              color={color}
              thumbPos={thumbPos}
              setThumbPos={setThumbPos}
              onChange={(newColor) => { markInteraction(); setColor(newColor); }}
              disabled={!isOn || effect === 2} // 무지개 모드일 때는 컬러휠 비활성화
              onInteractionStart={markInteraction}
            />
          </div>

          <div className="bg-white/70 dark:bg-gray-900/70 backdrop-blur-xl rounded-[2rem] p-6 shadow-lg border border-white/40 dark:border-gray-800/40">
            <div className="flex items-center justify-between mb-6">
              <h3 className="text-xs font-bold text-gray-900 dark:text-white flex items-center gap-2 uppercase">
                <SlidersHorizontal className="w-4 h-4 text-gray-400" />
                Brightness
              </h3>
              <span className="text-sm font-black text-gray-900 dark:text-white">{brightness}%</span>
            </div>
            
            <div className="px-2">
              <div className="relative w-full h-1.5 bg-gray-200 dark:bg-gray-800 rounded-full overflow-hidden">
                <motion.div 
                  initial={false}
                  animate={{ width: `${brightness}%`, backgroundColor: isOn ? color : "#9ca3af" }}
                  className="absolute left-0 top-0 h-full transition-colors duration-500"
                />
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={brightness}
                  onPointerDown={markInteraction}
                  onChange={(e) => { markInteraction(); setBrightness(Number(e.target.value)); }}
                  disabled={!isOn}
                  className="absolute inset-0 w-full h-full opacity-0 cursor-pointer disabled:cursor-not-allowed z-10"
                />
              </div>
            </div>
          </div>


        </div>
      </div>

      <BottomNav />
    </div>
  );
}

