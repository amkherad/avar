import { QueuePanel } from "@/components/queue/QueuePanel";
import { SessionSelector } from "@/components/session/SessionSelector";

export type AppPage = "dashboard" | "settings";

export function Sidebar() {
  return (
    <aside className="avar-shell__sidebar">
      <div className="avar-sidebar__queues">
        <QueuePanel />
      </div>
      <SessionSelector />
    </aside>
  );
}
