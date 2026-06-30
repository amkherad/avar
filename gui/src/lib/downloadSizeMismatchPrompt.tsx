import { createRoot } from "react-dom/client";
import { StrictMode } from "react";
import type { DownloadInfo } from "@/api/types";
import { DownloadSizeMismatchModal } from "@/components/download/DownloadSizeMismatchModal";

export type DownloadSizeMismatchChoice = "restart" | "new" | "cancel";

export interface DownloadSizeMismatchContext {
  persistedBytes: number;
  probedBytes: number;
}

let promptHost: HTMLDivElement | null = null;
let promptRoot: ReturnType<typeof createRoot> | null = null;

function ensurePromptHost(): HTMLDivElement {
  if (promptHost === null) {
    promptHost = document.createElement("div");
    promptHost.id = "avar-download-size-mismatch-host";
    document.body.appendChild(promptHost);
    promptRoot = createRoot(promptHost);
  }
  return promptHost;
}

function closePrompt(): void {
  promptRoot?.render(null);
}

export function promptDownloadSizeMismatch(
  download: DownloadInfo,
  context: DownloadSizeMismatchContext,
): Promise<DownloadSizeMismatchChoice> {
  return new Promise((resolve) => {
    ensurePromptHost();

    const finish = (choice: DownloadSizeMismatchChoice) => {
      closePrompt();
      resolve(choice);
    };

    promptRoot?.render(
      <StrictMode>
        <DownloadSizeMismatchModal
          download={download}
          persistedBytes={context.persistedBytes}
          probedBytes={context.probedBytes}
          onRestart={() => finish("restart")}
          onNewDownload={() => finish("new")}
          onCancel={() => finish("cancel")}
        />
      </StrictMode>,
    );
  });
}
