import { createRoot } from "react-dom/client";
import { StrictMode } from "react";
import type { DownloadInfo } from "@/api/types";
import { ResumeUnsupportedModal } from "@/components/download/ResumeUnsupportedModal";
import { isResumeUnsupportedDownload } from "@/lib/resumeUnsupported";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";

type PromptChoice = "restart" | "new" | "dismiss";

let promptHost: HTMLDivElement | null = null;
let promptRoot: ReturnType<typeof createRoot> | null = null;
let activeDownloadId: string | null = null;
const handledIds = new Set<string>();
const dismissedIds = new Set<string>();

function ensurePromptHost(): HTMLDivElement {
  if (promptHost === null) {
    promptHost = document.createElement("div");
    promptHost.id = "avar-resume-unsupported-host";
    document.body.appendChild(promptHost);
    promptRoot = createRoot(promptHost);
  }
  return promptHost;
}

function closePrompt(): void {
  activeDownloadId = null;
  promptRoot?.render(null);
}

async function runPromptAction(
  download: DownloadInfo,
  choice: PromptChoice,
): Promise<void> {
  const client = useConnectionStore.getState().client;
  if (!client) {
    return;
  }

  try {
    if (choice === "restart") {
      await client.restartDownload(download.id);
      handledIds.add(download.id);
    } else if (choice === "new") {
      const details = await client.getDownloadDetails(download.id);
      if (!details.url) {
        return;
      }
      await client.dismissResumePrompt(download.id);
      await client.addDownload({
        url: details.url,
        queue: download.queueId,
        name: download.filename,
        startNow: true,
        forceNew: true,
      });
      handledIds.add(download.id);
      dismissedIds.add(download.id);
    } else {
      dismissedIds.add(download.id);
    }
  } finally {
    await useDataStore.getState().refresh();
    closePrompt();
  }
}

function renderPrompt(download: DownloadInfo, busy: boolean): void {
  ensurePromptHost();
  promptRoot?.render(
    <StrictMode>
      <ResumeUnsupportedModal
        download={download}
        busy={busy}
        onCancel={() => void runPromptAction(download, "dismiss")}
        onRestart={() => void runPromptAction(download, "restart")}
        onNewDownload={() => void runPromptAction(download, "new")}
      />
    </StrictMode>,
  );
}

function maybePrompt(download: DownloadInfo): void {
  if (activeDownloadId !== null) {
    return;
  }
  if (handledIds.has(download.id) || dismissedIds.has(download.id)) {
    return;
  }
  if (!isResumeUnsupportedDownload(download)) {
    return;
  }

  activeDownloadId = download.id;
  renderPrompt(download, false);
}

export function initResumeUnsupportedWatcher(): () => void {
  for (const download of useDataStore.getState().downloads) {
    maybePrompt(download);
  }

  return useDataStore.subscribe((state) => {
    if (activeDownloadId !== null) {
      return;
    }
    for (const download of state.downloads) {
      maybePrompt(download);
      if (activeDownloadId !== null) {
        break;
      }
    }
  });
}
