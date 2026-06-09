import { createBrowserRouter, Navigate, Outlet, ScrollRestoration, useLocation } from "react-router";
import { useEffect } from "react";
import { Login } from "./components/Login";
import { SignupScreen } from "./components/SignupScreen";
import { ModesScreen } from "./components/ModesScreen";
import { ScentsScreen } from "./components/ScentsScreen";
import { SettingsScreen } from "./components/SettingsScreen";
import { DiaryScreen } from "./components/DiaryScreen";
import { PrivacyScreen } from "./components/PrivacyScreen";
import { DeviceManagementScreen } from "./components/DeviceManagementScreen";
import { LedSettingsScreen } from "./components/LedSettingsScreen";
import { MusicSettingsScreen } from "./components/MusicSettingsScreen";
import { AiTasteReportScreen } from "./components/AiTasteReportScreen";

function Root() {
  const location = useLocation();
  
  useEffect(() => {
    window.scrollTo(0, 0);
  }, [location.pathname]);

  return (
    <>
      <ScrollRestoration />
      <Outlet />
    </>
  );
}

export const router = createBrowserRouter([
  {
    path: "/",
    Component: Root,
    children: [
      {
        index: true,
        Component: Login,
      },
      {
        path: "signup",
        Component: SignupScreen,
      },
      {
        path: "home",
        element: <Navigate to="/modes" replace />,
      },
      {
        path: "modes",
        Component: ModesScreen,
      },
      {
        path: "led",
        Component: LedSettingsScreen,
      },
      {
        path: "diary",
        Component: DiaryScreen,
      },
      {
        path: "scents",
        Component: ScentsScreen,
      },
      {
        path: "settings",
        Component: SettingsScreen,
      },
      {
        path: "privacy",
        Component: PrivacyScreen,
      },
      {
        path: "ai-report",
        Component: AiTasteReportScreen,
      },
      {
        path: "device",
        Component: DeviceManagementScreen,
      },
      {
        path: "music",
        Component: MusicSettingsScreen,
      },
      {
        path: "*",
        element: <Navigate to="/" replace />,
      }
    ],
  }
]);
