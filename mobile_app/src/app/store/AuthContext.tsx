import React, { createContext, useContext, useState } from "react";
import { apiLogin, apiSignup, apiUpdateUser } from "../utils/api";

export interface User {
  email: string;
  password?: string;
  region: string;
  musicTracks?: string;
  deviceId?: string;
}

interface AuthContextType {
  users: User[];
  currentUser: User | null;
  registerUser: (email: string, pass: string, region: string, authCode: string, deviceId: string) => Promise<{ success: boolean; message: string }>;
  loginUser: (email: string, pass: string) => Promise<{ success: boolean; message: string }>;
  logoutUser: () => void;
  updateUser: (email: string, data: Partial<User>, oldPassword?: string) => Promise<{ success: boolean; message: string }>;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export const AuthProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [users, setUsers] = useState<User[]>([]);
  const [currentUser, setCurrentUser] = useState<User | null>(null);

  const registerUser = async (email: string, pass: string, region: string, authCode: string, deviceId: string) => {
    const response = await apiSignup(email, pass, region, authCode, deviceId);
    if (response.result === "SUCCESS") {
      const newUser: User = { email, region, deviceId };
      setUsers((prev) => [...prev, newUser]);
      return { success: true, message: response.message || "회원가입 성공" };
    }
    return { success: false, message: response.message || "회원가입 실패" };
  };

  const loginUser = async (email: string, pass: string) => {
    const response = await apiLogin(email, pass);
    if (response.result === "SUCCESS") {
      const user: User = { 
        email, 
        region: response.region || "", 
        musicTracks: response.music_tracks,
        deviceId: (response as any).deviceId
      };
      setCurrentUser(user);
      return { success: true, message: response.message || "로그인 성공" };
    }
    return { success: false, message: response.message || "로그인 실패" };
  };

  const logoutUser = () => setCurrentUser(null);

  const updateUser = async (email: string, data: Partial<User>, oldPassword?: string) => {
    // 백엔드 UPDATE_USER 액션 호출 (password, region 등)
    const response = await apiUpdateUser(email, oldPassword, data.password, data.region);
    
    // 백엔드 응답이 성공이거나, deviceId만 로컬에서 변경하는 경우 처리
    if (response.result === "SUCCESS" || (data.deviceId && !data.password && !data.region)) {
      setUsers(prev => prev.map(u => u.email === email ? { ...u, ...data } : u));
      if (currentUser?.email === email) {
        setCurrentUser(prev => prev ? { ...prev, ...data } : null);
      }
      return { success: true, message: response.message || "정보가 수정되었습니다." };
    }
    return { success: false, message: response.message || "정보 수정에 실패했습니다." };
  };

  return (
    <AuthContext.Provider value={{ users, currentUser, registerUser, loginUser, logoutUser, updateUser }}>
      {children}
    </AuthContext.Provider>
  );
};

export const useAuth = () => {
  const context = useContext(AuthContext);
  if (!context) throw new Error("useAuth must be used within an AuthProvider");
  return context;
};
