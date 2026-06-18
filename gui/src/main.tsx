import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { App } from "./App";
import "@/icons";
import "@/theme/global.css";
import "@/theme/components.css";
import "@/i18n";
import { registerServiceWorker } from "@/lib/pwa";

void registerServiceWorker();

const root = document.getElementById("root");
if (!root) {
  throw new Error("Root element #root not found");
}

createRoot(root).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
