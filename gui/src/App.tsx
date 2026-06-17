import { useEffect, useState } from "react";
import { AppShell } from "@/components/layout/AppShell";
import type { AppPage } from "@/components/layout/Sidebar";
import { ThemeProvider } from "@/theme/ThemeContext";
import { PopupHost } from "@/components/ui/PopupHost";
import { DashboardPage } from "@/pages/DashboardPage";
import { SettingsPage } from "@/pages/SettingsPage";
import { HelpPage } from "@/pages/HelpPage";
import { DownloadDetailPopupPage } from "@/pages/DownloadDetailPopupPage";
import { useConfigStore } from "@/stores/configStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { useConsoleStore } from "@/stores/consoleStore";
import { initSyncCoordinator } from "@/sync/syncManager";
import { useConnectionStore } from "@/stores/connectionStore";
import { parsePopupHash } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";
import { ShortcutProvider } from "@/shortcuts/ShortcutProvider";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";
import i18n, { isRtlLocale } from "@/i18n";

function AppContent() {
  const [page, setPage] = useState<AppPage>("dashboard");
  const locale = useConfigStore((s) => s.config.locale);
  const toggleConsole = useConsoleStore((s) => s.toggleOpen);
  const toggleDetailPanel = useLayoutStore((s) => s.toggleDetailPanel);

  useEffect(() => {
    void i18n.changeLanguage(locale);
    document.documentElement.lang = locale;
    document.documentElement.dir = isRtlLocale(locale) ? "rtl" : "ltr";
  }, [locale]);

  useEffect(() => initSyncCoordinator(), []);

  useEffect(() => {
    appLogger.gui.info("Avar GUI started");
  }, []);

  function handleNavigate(next: AppPage) {
    appLogger.gui.debug(`Navigate to ${next}`);
    setPage(next);
  }

  useShortcutAction("nav.dashboard", () => setPage("dashboard"));
  useShortcutAction("nav.settings", () => setPage("settings"));
  useShortcutAction("nav.help", () => setPage("help"));
  useShortcutAction("console.toggle", () => toggleConsole());
  useShortcutAction("detailPanel.toggle", () => toggleDetailPanel());

  function renderPage() {
    switch (page) {
      case "settings":
        return <SettingsPage />;
      case "help":
        return <HelpPage />;
      default:
        return <DashboardPage />;
    }
  }

  return (
    <>
      <AppShell page={page} onNavigate={handleNavigate}>
        {renderPage()}
      </AppShell>
      <PopupHost />
    </>
  );
}

function PopupContent() {
  const [popupRoute, setPopupRoute] = useState(() => parsePopupHash(window.location.hash));
  useEffect(() => {
    useConnectionStore.getState().reconnectClient();
    useConnectionStore.getState().startPingMonitor();
    return () => useConnectionStore.getState().stopPingMonitor();
  }, []);

  useEffect(() => {
    const onHashChange = () => setPopupRoute(parsePopupHash(window.location.hash));
    window.addEventListener("hashchange", onHashChange);
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);

  if (popupRoute?.type === "download") {
    return <DownloadDetailPopupPage downloadId={popupRoute.id} />;
  }

  return null;
}

export function App() {
  const isPopup = window.location.hash.startsWith("#/popup/");

  return (
    <ThemeProvider>
      <ShortcutProvider>
        {isPopup ? <PopupContent /> : <AppContent />}
      </ShortcutProvider>
    </ThemeProvider>
  );
}
