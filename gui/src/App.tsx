import { useEffect, useState } from "react";
import { AppShell } from "@/components/layout/AppShell";
import type { AppPage } from "@/components/layout/Sidebar";
import { ThemeProvider } from "@/theme/ThemeContext";
import { DashboardPage } from "@/pages/DashboardPage";
import { SettingsPage } from "@/pages/SettingsPage";
import { useConfigStore } from "@/stores/configStore";
import { initSyncCoordinator } from "@/sync/syncManager";
import i18n, { isRtlLocale } from "@/i18n";

function AppContent() {
  const [page, setPage] = useState<AppPage>("dashboard");
  const locale = useConfigStore((s) => s.config.locale);

  useEffect(() => {
    void i18n.changeLanguage(locale);
    document.documentElement.lang = locale;
    document.documentElement.dir = isRtlLocale(locale) ? "rtl" : "ltr";
  }, [locale]);

  useEffect(() => initSyncCoordinator(), []);

  return (
    <AppShell page={page} onNavigate={setPage}>
      {page === "dashboard" ? <DashboardPage /> : <SettingsPage />}
    </AppShell>
  );
}

export function App() {
  return (
    <ThemeProvider>
      <AppContent />
    </ThemeProvider>
  );
}
