import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { BrowserView, Utils } from "electrobun/bun";
import type {
  AvarExtensionBridgeConfig,
  AvarNotificationOptions,
  AvarPopupOptions,
  AvarRpcSchema,
  AvarSaveRemoteDownloadFileOptions,
  AvarSelectDirectoryOptions,
  AvarTrayActiveDownloads,
  AvarTrayLabels,
} from "../../shared/avar-rpc";
import { getProxyBaseUrl } from "./desktop-shell";
import { getExtensionBridgeModule } from "./extension-host";
import { openPopup } from "./popup-manager";
import { setTrayActiveDownloads, setTrayLabels } from "./tray-manager";
import { setKeepInTrayOnClose } from "./close-behavior";

function fsExists(filePath: string): boolean {
  try {
    return existsSync(filePath);
  } catch {
    return false;
  }
}

function resolveUniqueDestPath(destDir: string, filename: string): string {
  const baseName = basename(filename);
  let destPath = join(destDir, baseName);
  if (!fsExists(destPath)) {
    return destPath;
  }

  const ext = baseName.includes(".") ? baseName.slice(baseName.lastIndexOf(".")) : "";
  const stem = ext ? baseName.slice(0, -ext.length) : baseName;

  for (let n = 1; n < 10_000; n++) {
    const candidate = join(destDir, `${stem} (${n})${ext}`);
    if (!fsExists(candidate)) {
      return candidate;
    }
  }

  throw new Error("Could not find a unique filename");
}

export function createAvarRpc() {
  return BrowserView.defineRPC<AvarRpcSchema>({
    maxRequestTime: 60_000,
    handlers: {
      requests: {
        openPopup: (options: AvarPopupOptions) => openPopup(options ?? {}),
        selectDirectory: async (options?: AvarSelectDirectoryOptions) => {
          const paths = await Utils.openFileDialog({
            startingFolder: options?.defaultPath ?? "~/",
            allowedFileTypes: "*",
            canChooseFiles: false,
            canChooseDirectory: true,
            allowsMultipleSelection: false,
          });
          return paths[0] ?? null;
        },
        showNotification: (options: AvarNotificationOptions) => {
          const title = options?.title?.trim() || "Avar";
          Utils.showNotification({
            title,
            body: options?.body,
          });
          return true;
        },
        getProxyBaseUrl: () => getProxyBaseUrl(),
        saveRemoteDownloadFile: async (options: AvarSaveRemoteDownloadFileOptions) => {
          const fileUrl = options?.fileUrl;
          const destDir = options?.destDir;
          const filename = options?.filename;
          if (!fileUrl || !destDir || !filename) {
            throw new Error("Missing download copy parameters");
          }

          const headers: Record<string, string> = {};
          if (options.authToken?.trim()) {
            headers.Authorization = `Bearer ${options.authToken.trim()}`;
          }

          const response = await fetch(fileUrl, { headers });
          if (!response.ok) {
            throw new Error(`Failed to fetch remote file (${response.status})`);
          }

          const buffer = Buffer.from(await response.arrayBuffer());
          mkdirSync(destDir, { recursive: true });
          const destPath = resolveUniqueDestPath(destDir, basename(filename));
          writeFileSync(destPath, buffer);
        },
        setExtensionBridgeEnabled: (enabled: boolean) => {
          getExtensionBridgeModule().setExtensionBridgeEnabled(Boolean(enabled));
        },
        setExtensionBridgeConfig: (config: AvarExtensionBridgeConfig) => {
          if (config?.daemonUrl) {
            getExtensionBridgeModule().setExtensionBridgeConfig({
              daemonUrl: config.daemonUrl,
              authToken: config.authToken,
            });
          }
        },
        getExtensionBridgeState: () => {
          return getExtensionBridgeModule().getExtensionBridgeState();
        },
        getExtensionBridgeUrl: () => {
          return getExtensionBridgeModule().ELECTRON_EXTENSION_BRIDGE_URL;
        },
        setTrayLabels: (labels: AvarTrayLabels) => {
          if (labels && typeof labels === "object") {
            setTrayLabels(labels);
          }
        },
        setTrayActiveDownloads: (payload: AvarTrayActiveDownloads) => {
          if (payload && typeof payload === "object") {
            setTrayActiveDownloads(payload);
          }
        },
        openPath: (filePath: string) => {
          if (typeof filePath !== "string" || filePath.trim() === "") {
            return "Invalid file path";
          }
          const ok = Utils.openPath(filePath);
          return ok ? "" : "Failed to open path";
        },
        showItemInFolder: (filePath: string) => {
          if (typeof filePath !== "string" || filePath.trim() === "") {
            return "Invalid file path";
          }
          Utils.showItemInFolder(filePath);
          return "";
        },
        openExternal: (url: string) => {
          if (typeof url !== "string") {
            return false;
          }
          const trimmed = url.trim();
          if (!/^https?:\/\//i.test(trimmed)) {
            return false;
          }
          return Utils.openExternal(trimmed);
        },
        setKeepInTrayOnClose: (enabled: boolean) => {
          setKeepInTrayOnClose(Boolean(enabled));
        },
      },
    },
  });
}
