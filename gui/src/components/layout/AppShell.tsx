import type { ReactNode } from "react";
import { Header } from "./Header";
import { Sidebar } from "./Sidebar";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { useLayoutStore } from "@/stores/layoutStore";
import type { AppPage } from "./Sidebar";
import { useTranslation } from "react-i18next";

export interface AppShellProps {
  page: AppPage;
  onNavigate: (page: AppPage) => void;
  children: ReactNode;
}

export function AppShell({ page, onNavigate, children }: AppShellProps) {
  const { t } = useTranslation();
  const sidebarWidth = useLayoutStore((s) => s.sidebarWidth);
  const adjustSidebarWidth = useLayoutStore((s) => s.adjustSidebarWidth);
  const isDashboard = page === "dashboard";

  return (
    <div className="avar-shell">
      <Header page={page} onNavigate={onNavigate} />
      <div className="avar-shell__body">
        {isDashboard ? (
          <>
            <aside className="avar-shell__sidebar" style={{ width: sidebarWidth }}>
              <Sidebar />
            </aside>
            <ResizeHandle
              axis="horizontal"
              min={180}
              max={480}
              label={t("layout.resizeSidebar")}
              onResize={adjustSidebarWidth}
            />
          </>
        ) : null}
        <main className={`avar-page ${isDashboard ? "" : "avar-page--full"}`}>{children}</main>
      </div>
    </div>
  );
}
