import type { ReactNode } from "react";
import { Header } from "./Header";
import { Sidebar, type AppPage } from "./Sidebar";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { ErrorBoundary } from "@/components/ui/ErrorBoundary";
import { useLayoutStore } from "@/stores/layoutStore";
import type { SettingsCategory } from "@/pages/SettingsPage";
import { useTranslation } from "react-i18next";

export interface AppShellProps {
  page: AppPage;
  onNavigate: (page: AppPage) => void;
  helpTopicId: string;
  onHelpTopicChange: (id: string) => void;
  settingsCategory: SettingsCategory;
  onSettingsCategoryChange: (category: SettingsCategory) => void;
  children: ReactNode;
}

export function AppShell({
  page,
  onNavigate,
  helpTopicId,
  onHelpTopicChange,
  settingsCategory,
  onSettingsCategoryChange,
  children,
}: AppShellProps) {
  const { t } = useTranslation();
  const sidebarWidth = useLayoutStore((s) => s.sidebarWidth);
  const adjustSidebarWidth = useLayoutStore((s) => s.adjustSidebarWidth);
  const isDashboard = page === "dashboard";

  return (
    <div className="avar-shell">
      <Header page={page} onNavigate={onNavigate} />
      <div className="avar-shell__body">
        <aside className="avar-shell__sidebar" style={{ width: sidebarWidth }}>
          <ErrorBoundary name="Sidebar">
            <Sidebar
              page={page}
              helpTopicId={helpTopicId}
              onHelpTopicChange={onHelpTopicChange}
              settingsCategory={settingsCategory}
              onSettingsCategoryChange={onSettingsCategoryChange}
              onNavigate={onNavigate}
            />
          </ErrorBoundary>
        </aside>
        <ResizeHandle
          axis="horizontal"
          min={180}
          max={480}
          label={t("layout.resizeSidebar")}
          onResize={adjustSidebarWidth}
        />
        <main className={`avar-page ${isDashboard ? "" : "avar-page--full"}`}>
          <ErrorBoundary name="Page" resetLabel={t("common.tryAgain")}>
            {children}
          </ErrorBoundary>
        </main>
      </div>
    </div>
  );
}
