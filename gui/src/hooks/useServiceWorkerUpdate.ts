import { useEffect, useState } from "react";
import { activateWaitingServiceWorker, onServiceWorkerUpdate } from "@/lib/pwa";
import { appLogger } from "@/lib/appLogger";

export function useServiceWorkerUpdate(): {
  updateAvailable: boolean;
  reload: () => void;
} {
  const [updateAvailable, setUpdateAvailable] = useState(false);

  useEffect(() => onServiceWorkerUpdate(() => setUpdateAvailable(true)), []);

  return {
    updateAvailable,
    reload: () => {
      appLogger.gui.debug("PWA reload for update");
      void activateWaitingServiceWorker();
    },
  };
}
