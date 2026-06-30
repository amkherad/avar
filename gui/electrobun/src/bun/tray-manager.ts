import Electrobun, { Tray, type MenuItemConfig } from "electrobun/bun";
import type { AvarTrayActiveDownloads, AvarTrayLabels } from "../../shared/avar-rpc";
import { DAEMON_TARGET } from "./desktop-shell";

const TRAY_ICON = "views://mainview/icon-16.png";

const trayLabels: Required<AvarTrayLabels> = {
  show: "Show Avar",
  exit: "Exit Avar",
  startAll: "Start All",
  pauseAll: "Pause All",
  resumeAll: "Resume All",
  stopAll: "Stop All",
};

let trayActiveDownloads: {
  sectionLabel: string;
  items: Array<{ id: string; filename: string; percent: number }>;
} = { sectionLabel: "Active downloads", items: [] };

let tray: Tray | null = null;
let showMainWindow: (() => void) | null = null;
let requestQuit: (() => void) | null = null;

async function daemonRpc(method: string, params: unknown) {
  const response = await fetch(`${DAEMON_TARGET}/api/rpc`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ jsonrpc: "2.0", method, params, id: Date.now() }),
  });
  if (!response.ok) {
    throw new Error(`Daemon HTTP ${response.status}`);
  }
  const payload = await response.json();
  if (payload.error) {
    throw new Error(payload.error.message || "RPC error");
  }
  return payload.result;
}

async function listDownloads() {
  const result = await daemonRpc("cli.exec", {
    argv: ["avar", "config", "get", "dm.items", "--format=json"],
  });
  if (result?.exitCode !== 0 || !result.stdout) {
    return [];
  }
  try {
    const parsed = JSON.parse(result.stdout);
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    return [];
  }
}

function canStart(status: string) {
  return ["queued", "stopped", "error", "failed", "cancelled"].includes(status);
}

function canPause(status: string) {
  return status === "downloading";
}

function canResume(status: string) {
  return status === "paused";
}

function canStop(status: string) {
  return ["downloading", "paused", "queued"].includes(status);
}

async function runBulkAction(kind: "start" | "pause" | "resume" | "stop") {
  const downloads = await listDownloads();
  const ids = downloads
    .filter((item: { status?: string }) => {
      const status = item.status ?? "";
      if (kind === "start") return canStart(status);
      if (kind === "pause") return canPause(status);
      if (kind === "resume") return canResume(status);
      if (kind === "stop") return canStop(status);
      return false;
    })
    .map((item: { id?: string }) => item.id)
    .filter(Boolean);

  for (const id of ids) {
    try {
      if (kind === "start") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "start", id] });
      } else if (kind === "pause") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "pause", id] });
      } else if (kind === "resume") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "resume", id] });
      } else if (kind === "stop") {
        await daemonRpc("cli.exec", { argv: ["avar", "download", "stop", id] });
      }
    } catch {
      // Continue with remaining downloads.
    }
  }
}

function buildTrayMenu(): MenuItemConfig[] {
  const template: MenuItemConfig[] = [
    { type: "normal", label: trayLabels.show, action: "tray-show" },
    { type: "separator" },
  ];

  if (trayActiveDownloads.items.length > 0) {
    template.push({
      type: "normal",
      label: trayActiveDownloads.sectionLabel,
      enabled: false,
    });
    for (const item of trayActiveDownloads.items) {
      const label =
        item.filename.length > 48 ? `${item.filename.slice(0, 47)}…` : item.filename;
      template.push({
        type: "normal",
        label: `${label} (${item.percent}%)`,
        enabled: false,
      });
    }
    template.push({ type: "separator" });
  }

  template.push(
    { type: "normal", label: trayLabels.startAll, action: "tray-start-all" },
    { type: "normal", label: trayLabels.pauseAll, action: "tray-pause-all" },
    { type: "normal", label: trayLabels.resumeAll, action: "tray-resume-all" },
    { type: "normal", label: trayLabels.stopAll, action: "tray-stop-all" },
    { type: "separator" },
    { type: "normal", label: trayLabels.exit, action: "tray-exit" },
  );

  return template;
}

function updateTrayTooltip() {
  if (!tray) {
    return;
  }
  if (trayActiveDownloads.items.length === 0) {
    tray.setTitle("Avar");
    return;
  }
  const lines = trayActiveDownloads.items.map(
    (item) => `${item.filename} (${item.percent}%)`,
  );
  tray.setTitle(["Avar", ...lines].join("\n"));
}

function handleTrayAction(action: string) {
  if (action === "tray-show") {
    showMainWindow?.();
    return;
  }
  if (action === "tray-exit") {
    requestQuit?.();
    return;
  }
  if (action === "tray-start-all") {
    void runBulkAction("start");
    return;
  }
  if (action === "tray-pause-all") {
    void runBulkAction("pause");
    return;
  }
  if (action === "tray-resume-all") {
    void runBulkAction("resume");
    return;
  }
  if (action === "tray-stop-all") {
    void runBulkAction("stop");
  }
}

export function createTray(options: {
  onShowMainWindow: () => void;
  onQuit: () => void;
}): Tray {
  showMainWindow = options.onShowMainWindow;
  requestQuit = options.onQuit;

  tray = new Tray({
    image: TRAY_ICON,
    template: false,
    width: 16,
    height: 16,
  });
  tray.setTitle("Avar");
  tray.setMenu(buildTrayMenu());
  updateTrayTooltip();

  tray.on("tray-clicked", () => {
    showMainWindow?.();
  });

  Electrobun.events.on("tray-clicked", (data: { action: string }) => {
    handleTrayAction(data.action);
  });

  return tray;
}

export function setTrayLabels(labels: AvarTrayLabels): void {
  Object.assign(trayLabels, labels);
  tray?.setMenu(buildTrayMenu());
}

export function setTrayActiveDownloads(payload: AvarTrayActiveDownloads): void {
  trayActiveDownloads = {
    sectionLabel:
      typeof payload.sectionLabel === "string"
        ? payload.sectionLabel
        : trayActiveDownloads.sectionLabel,
    items: Array.isArray(payload.items)
      ? payload.items
          .filter(
            (item) =>
              item &&
              typeof item.id === "string" &&
              typeof item.filename === "string" &&
              typeof item.percent === "number",
          )
          .slice(0, 3)
      : [],
  };
  tray?.setMenu(buildTrayMenu());
  updateTrayTooltip();
}

export function destroyTray(): void {
  tray?.remove();
  tray = null;
}
