import { useMemo } from "react";
import { useConfigStore } from "@/stores/configStore";
import { useConnectionStore } from "@/stores/connectionStore";
import { isRemoteSession } from "@/lib/sessionRemote";
import type { DownloadDoubleClickAction } from "@/config/defaults";

export function useDownloadDoubleClickAction(): DownloadDoubleClickAction {
  const action = useConfigStore((s) => s.config.downloadDoubleClickAction);
  const activeSession = useConnectionStore((s) => s.activeSession);

  return useMemo(() => {
    const localSession = activeSession != null && !isRemoteSession(activeSession);
    const canOpenFile = localSession && Boolean(window.avar?.openPath);
    if (action === "openFile" && canOpenFile) {
      return "openFile";
    }
    return "openDetails";
  }, [action, activeSession]);
}

export function useCanConfigureDownloadDoubleClick(): boolean {
  const activeSession = useConnectionStore((s) => s.activeSession);
  return activeSession != null && !isRemoteSession(activeSession);
}

export function useCanOpenDownloadFile(): boolean {
  const activeSession = useConnectionStore((s) => s.activeSession);
  return (
    activeSession != null &&
    !isRemoteSession(activeSession) &&
    Boolean(window.avar?.openPath)
  );
}
