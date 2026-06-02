import { RouterProvider } from "react-router";
import { router } from "./routes";
import { useEffect } from "react";
import { AuthProvider } from "./store/AuthContext";
import { DeviceProvider } from "./store/DeviceContext";
import { DiaryProvider } from "./store/DiaryContext";
import { UIProvider } from "./store/UIContext";

export default function App() {
  useEffect(() => {
    document.title = "Smart Diffuser";
  }, []);

  return (
    <AuthProvider>
      <DeviceProvider>
        <DiaryProvider>
          <UIProvider>
            <RouterProvider router={router} />
          </UIProvider>
        </DiaryProvider>
      </DeviceProvider>
    </AuthProvider>
  );
}
