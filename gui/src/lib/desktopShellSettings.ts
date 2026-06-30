import { useConfigStore } from "@/stores/configStore";

async function syncDesktopShellSettings(): Promise<void> {
  if (!window.avar?.isElectron || !window.avar.setKeepInTrayOnClose) {
    return;
  }
  const { config } = useConfigStore.getState();
  await window.avar.setKeepInTrayOnClose(config.keepInTrayOnClose);
}

export function initDesktopShellSettings(): () => void {
  void syncDesktopShellSettings();

  const unsub = useConfigStore.subscribe((state, prev) => {
    if (state.config.keepInTrayOnClose !== prev.config.keepInTrayOnClose) {
      void syncDesktopShellSettings();
    }
  });

  return () => {
    unsub();
  };
}
