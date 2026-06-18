import { SessionSelector } from "@/components/session/SessionSelector";
import { QueuePanel } from "@/components/queue/QueuePanel";
import { HelpSidebarNav } from "./HelpSidebarNav";
import { SettingsSidebarNav } from "./SettingsSidebarNav";
import type { SettingsCategory } from "@/pages/SettingsPage";

export type AppPage = "dashboard" | "settings" | "help";

export interface SidebarProps {
  page: AppPage;
  helpTopicId: string;
  onHelpTopicChange: (id: string) => void;
  settingsCategory: SettingsCategory;
  onSettingsCategoryChange: (category: SettingsCategory) => void;
  onNavigate?: (page: AppPage) => void;
  onOpenSettings?: (category: SettingsCategory) => void;
}

export function Sidebar({
  page,
  helpTopicId,
  onHelpTopicChange,
  settingsCategory,
  onSettingsCategoryChange,
  onNavigate,
  onOpenSettings,
}: SidebarProps) {
  function openSettingsCategory(category: SettingsCategory) {
    if (onOpenSettings) {
      onOpenSettings(category);
      return;
    }
    onSettingsCategoryChange(category);
    onNavigate?.("settings");
  }

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
      default:
        return (
          <QueuePanel
            mode="select"
            onManageQueues={() => openSettingsCategory("queues")}
            onModifyQueue={() => openSettingsCategory("queues")}
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
