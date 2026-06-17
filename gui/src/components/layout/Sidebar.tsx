import { SessionSelector } from "@/components/session/SessionSelector";
import { QueuePanel } from "@/components/queue/QueuePanel";
import { HelpSidebarNav } from "./HelpSidebarNav";
import { SettingsSidebarNav } from "./SettingsSidebarNav";
import type { SettingsCategory } from "@/pages/SettingsPage";

export type AppPage = "dashboard" | "settings" | "help" | "queues";

export interface SidebarProps {
  page: AppPage;
  helpTopicId: string;
  onHelpTopicChange: (id: string) => void;
  settingsCategory: SettingsCategory;
  onSettingsCategoryChange: (category: SettingsCategory) => void;
  onNavigate?: (page: AppPage) => void;
}

export function Sidebar({
  page,
  helpTopicId,
  onHelpTopicChange,
  settingsCategory,
  onSettingsCategoryChange,
  onNavigate,
}: SidebarProps) {
  function renderBody() {
    switch (page) {
      case "help":
        return (
          <HelpSidebarNav topicId={helpTopicId} onTopicChange={onHelpTopicChange} />
        );
      case "settings":
        return (
          <SettingsSidebarNav
            category={settingsCategory}
            onCategoryChange={onSettingsCategoryChange}
          />
        );
      case "queues":
        return null;
      default:
        return (
          <QueuePanel
            mode="select"
            onManageQueues={onNavigate ? () => onNavigate("queues") : undefined}
            onModifyQueue={onNavigate ? () => onNavigate("queues") : undefined}
          />
        );
    }
  }

  return (
    <>
      <div className="avar-sidebar__body">{renderBody()}</div>
      <SessionSelector />
    </>
  );
}
