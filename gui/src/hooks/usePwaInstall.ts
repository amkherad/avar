import { useCallback, useEffect, useState } from "react";
import { isPwaSupported, isStandalonePwa } from "@/lib/pwa";
import { appLogger } from "@/lib/appLogger";

interface BeforeInstallPromptEvent extends Event {
  prompt: () => Promise<void>;
  userChoice: Promise<{ outcome: "accepted" | "dismissed" }>;
}

export function usePwaInstall() {
  const [canInstall, setCanInstall] = useState(false);
  const [installed, setInstalled] = useState(isStandalonePwa());
  const [promptEvent, setPromptEvent] = useState<BeforeInstallPromptEvent | null>(null);

  useEffect(() => {
    if (!isPwaSupported()) {
      return;
    }

    const onBeforeInstall = (event: Event) => {
      event.preventDefault();
      setPromptEvent(event as BeforeInstallPromptEvent);
      setCanInstall(true);
    };

    const onAppInstalled = () => {
      setInstalled(true);
      setCanInstall(false);
      setPromptEvent(null);
    };

    window.addEventListener("beforeinstallprompt", onBeforeInstall);
    window.addEventListener("appinstalled", onAppInstalled);

    return () => {
      window.removeEventListener("beforeinstallprompt", onBeforeInstall);
      window.removeEventListener("appinstalled", onAppInstalled);
    };
  }, []);

  const install = useCallback(async () => {
    if (!promptEvent) {
      return false;
    }

    appLogger.gui.debug("PWA install prompted");
    await promptEvent.prompt();
    const choice = await promptEvent.userChoice;
    appLogger.gui.debug("PWA install choice", choice.outcome);
    if (choice.outcome === "accepted") {
      setInstalled(true);
      setCanInstall(false);
      setPromptEvent(null);
      return true;
    }
    return false;
  }, [promptEvent]);

  return {
    supported: isPwaSupported(),
    canInstall,
    installed,
    install,
  };
}
