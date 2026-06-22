import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";
import { isRemoteSession } from "@/lib/sessionRemote";

export type DirectoryPathInputMode = "local" | "remote" | "text-only";

export function useDaemonDirectoryPathMode(): DirectoryPathInputMode {
  const activeSession = useConnectionStore((s) => s.activeSession);
  const fsBrowseEnabled = useDataStore((s) => s.health?.fsBrowseEnabled === true);

  if (activeSession !== null && isRemoteSession(activeSession)) {
    return fsBrowseEnabled ? "remote" : "text-only";
  }

  return window.avar?.isElectron ? "local" : "text-only";
}

export function useLocalDirectoryPathMode(): DirectoryPathInputMode {
  return window.avar?.isElectron ? "local" : "text-only";
}
