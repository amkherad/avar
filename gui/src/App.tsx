import { useEffect, useState } from "react";
import { AppShell } from "@/components/layout/AppShell";
import { ThemeProvider } from "@/theme/ThemeContext";
import { PopupHost } from "@/components/ui/PopupHost";
import { DashboardPage } from "@/pages/DashboardPage";
import { SettingsPage } from "@/pages/SettingsPage";
import { HelpPage } from "@/pages/HelpPage";
import { ConfirmDialogPopupPage } from "@/pages/ConfirmDialogPopupPage";
import { BatchAddDownloadsPopupPage } from "@/pages/BatchAddDownloadsPopupPage";
import { AddDownloadPopupPage } from "@/pages/AddDownloadPopupPage";
import { DownloadDetailPopupPage } from "@/pages/DownloadDetailPopupPage";
import { useConfigStore } from "@/stores/configStore";
import { useConsoleStore } from "@/stores/consoleStore";
import { initSyncCoordinator } from "@/sync/syncManager";
import { initNotificationWatcher } from "@/lib/notificationWatcher";
import { initResumeUnsupportedWatcher } from "@/lib/resumeUnsupportedWatcher";
import { initBrowserExtensionBridge } from "@/lib/browserExtensionBridge";
import { initDesktopShellSettings } from "@/lib/desktopShellSettings";
import { toggleDetailPanelWithSelection } from "@/lib/detailPanel";
import { useElectronTrayLabels } from "@/hooks/useElectronTrayLabels";
import { useElectronTrayDownloads } from "@/hooks/useElectronTrayDownloads";
import { ensureElectronSession, useConnectionStore } from "@/stores/connectionStore";
import { parsePopupHash } from "@/lib/popup";
import { appLogger } from "@/lib/appLogger";
import { ShortcutProvider } from "@/shortcuts/ShortcutProvider";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";
import { useAppLocation, useAppNavigation } from "@/hooks/useAppLocation";
import { buildAppHash, defaultAppLocation, navigateAppLocation } from "@/lib/appRouting";
import { openAddDownloadDialog } from "@/lib/openAddDownloadDialog";
import i18n, { isRtlLocale } from "@/i18n";

function AppContent() {
  const location = useAppLocation();
  const { goToPage, openSettings, setHelpTopic, setSettingsCategory } = useAppNavigation();
  const locale = useConfigStore((s) => s.config.locale);
  const toggleConsole = useConsoleStore((s) => s.toggleOpen);

  useEffect(() => {
    if (!window.location.hash || window.location.hash === "#") {
      navigateAppLocation(defaultAppLocation(), true);
    }
  }, []);

  useEffect(() => {
    void i18n.changeLanguage(locale);
    document.documentElement.lang = locale;
    document.documentElement.dir = isRtlLocale(locale) ? "rtl" : "ltr";
  }, [locale]);

  useEffect(() => {
    if (window.avar?.isElectron) {
      document.documentElement.classList.add("avar-electron");
      if (window.avar.platform === "darwin") {
        document.documentElement.classList.add("avar-electron-darwin");
      }
      void ensureElectronSession().then(() => {
        useConnectionStore.getState().reconnectClient();
        useConnectionStore.getState().startPingMonitor();
      });
    }
  }, []);

  useEffect(() => initSyncCoordinator(), []);
  useEffect(() => initNotificationWatcher(), []);
  useEffect(() => initResumeUnsupportedWatcher(), []);
  useEffect(() => initBrowserExtensionBridge(), []);
  useEffect(() => initDesktopShellSettings(), []);
  useElectronTrayLabels();
  useElectronTrayDownloads();

  useEffect(() => {
    appLogger.gui.info("Avar GUI started");
  }, []);

  useEffect(() => {
    appLogger.gui.debug("Route changed", buildAppHash(location));
  }, [location]);

  useShortcutAction("nav.dashboard", () => goToPage("dashboard"));
  useShortcutAction("nav.settings", () => openSettings("general"));
  useShortcutAction("nav.help", () => goToPage("help"));
  useShortcutAction("console.toggle", () => toggleConsole());
  useShortcutAction("detailPanel.toggle", () => toggleDetailPanelWithSelection());
  useShortcutAction("download.add", () => openAddDownloadDialog(i18n.t("download.add")));

  function renderPage() {
    switch (location.page) {
      case "settings":
        return <SettingsPage category={location.settingsCategory} />;
      case "help":
        return <HelpPage topicId={location.helpTopicId} />;
      default:
        return <DashboardPage />;
    }
  }

  return (
    <>
      <AppShell
        page={location.page}
        onNavigate={goToPage}
        helpTopicId={location.helpTopicId}
        onHelpTopicChange={setHelpTopic}
        settingsCategory={location.settingsCategory}
        onSettingsCategoryChange={setSettingsCategory}
        onOpenSettings={openSettings}
      >
        {renderPage()}
      </AppShell>
      <PopupHost />
    </>
  );
}

function PopupContent() {
  const [popupRoute, setPopupRoute] = useState(() => parsePopupHash(window.location.hash));

  useEffect(() => initSyncCoordinator(), []);

  useEffect(() => {
    const onHashChange = () => setPopupRoute(parsePopupHash(window.location.hash));
    window.addEventListener("hashchange", onHashChange);
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);

  if (popupRoute?.type === "download") {
    return <DownloadDetailPopupPage downloadId={popupRoute.id} />;
  }

  if (popupRoute?.type === "confirm") {
    return <ConfirmDialogPopupPage confirmId={popupRoute.id} />;
  }

  if (popupRoute?.type === "batch-add") {
    return <BatchAddDownloadsPopupPage batchId={popupRoute.id} />;
  }

  if (popupRoute?.type === "add-download") {
    return <AddDownloadPopupPage addId={popupRoute.id} />;
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
