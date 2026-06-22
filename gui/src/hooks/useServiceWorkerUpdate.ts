import { useEffect, useState } from "react";
import { activateWaitingServiceWorker, onServiceWorkerUpdate } from "@/lib/pwa";

export function useServiceWorkerUpdate(): {
  updateAvailable: boolean;
  reload: () => void;
} {
  const [updateAvailable, setUpdateAvailable] = useState(false);

  useEffect(() => onServiceWorkerUpdate(() => setUpdateAvailable(true)), []);

  return {
    updateAvailable,
    reload: () => void activateWaitingServiceWorker(),
  };
}
