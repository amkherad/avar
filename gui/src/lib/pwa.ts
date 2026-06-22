function isElectron(): boolean {
  return window.avar?.isElectron === true;
}

export function isPwaSupported(): boolean {
  return !isElectron() && "serviceWorker" in navigator;
}

type ServiceWorkerUpdateListener = () => void;

const updateListeners = new Set<ServiceWorkerUpdateListener>();
let reloadOnControllerChange = false;

function notifyUpdateListeners(): void {
  for (const listener of updateListeners) {
    listener();
  }
}

function watchWaitingWorker(registration: ServiceWorkerRegistration): void {
  if (registration.waiting) {
    notifyUpdateListeners();
    return;
  }

  registration.addEventListener("updatefound", () => {
    const installing = registration.installing;
    if (!installing) {
      return;
    }

    installing.addEventListener("statechange", () => {
      if (installing.state === "installed" && registration.waiting) {
        notifyUpdateListeners();
      }
    });
  });
}

async function clearPwaCaches(): Promise<void> {
  if (!("caches" in window)) {
    return;
  }

  const keys = await caches.keys();
  await Promise.all(keys.filter((key) => key.startsWith("avar-gui-")).map((key) => caches.delete(key)));
}

async function unregisterServiceWorkers(): Promise<void> {
  const registrations = await navigator.serviceWorker.getRegistrations();
  await Promise.all(registrations.map((registration) => registration.unregister()));
  await clearPwaCaches();
}

export async function registerServiceWorker(): Promise<ServiceWorkerRegistration | null> {
  if (!isPwaSupported()) {
    return null;
  }

  if (import.meta.env.DEV) {
    await unregisterServiceWorkers();
    return null;
  }

  try {
    const registration = await navigator.serviceWorker.register("./sw.js", { scope: "./" });
    watchWaitingWorker(registration);

    navigator.serviceWorker.addEventListener("controllerchange", () => {
      if (reloadOnControllerChange) {
        window.location.reload();
      }
    });

    const checkForUpdates = () => void registration.update().catch(() => undefined);
    checkForUpdates();
    window.addEventListener("focus", checkForUpdates);
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "visible") {
        checkForUpdates();
      }
    });

    return registration;
  } catch {
    return null;
  }
}

export function onServiceWorkerUpdate(listener: ServiceWorkerUpdateListener): () => void {
  updateListeners.add(listener);
  return () => updateListeners.delete(listener);
}

export function activateWaitingServiceWorker(): void {
  reloadOnControllerChange = true;
  void navigator.serviceWorker.ready.then((registration) => {
    registration.waiting?.postMessage({ type: "SKIP_WAITING" });
  });
}

export async function getServiceWorkerRegistration(): Promise<ServiceWorkerRegistration | null> {
  if (!isPwaSupported()) {
    return null;
  }

  try {
    return await navigator.serviceWorker.ready;
  } catch {
    return null;
  }
}

export function isStandalonePwa(): boolean {
  return (
    window.matchMedia("(display-mode: standalone)").matches ||
    (window.navigator as Navigator & { standalone?: boolean }).standalone === true
  );
}
