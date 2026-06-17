import type { ReactNode } from "react";
import { Footer } from "./Footer";
import { Header } from "./Header";
import { Sidebar } from "./Sidebar";
import type { AppPage } from "./Sidebar";

export interface AppShellProps {
  page: AppPage;
  onNavigate: (page: AppPage) => void;
  children: ReactNode;
}

export function AppShell({ page, onNavigate, children }: AppShellProps) {
  return (
    <div className="avar-shell">
      <Header page={page} onNavigate={onNavigate} />
      <div className="avar-shell__body">
        <Sidebar />
        <main className="avar-page">{children}</main>
      </div>
      <Footer />
    </div>
  );
}
