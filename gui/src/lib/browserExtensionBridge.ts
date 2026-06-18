import { useEffect, useState } from "react";

import { useConfigStore } from "@/stores/configStore";

import {

  DEFAULT_EXTENSION_GUI_URL,

  ELECTRON_EXTENSION_BRIDGE_URL,

} from "@/lib/browserExtensionUrls";



export const BUNDLED_EXTENSION_VERSION = "0.1.0";



export interface ExtensionBridgeState {

  enabled: boolean;

  connected: boolean;

  loading: boolean;

  bridgeVersion: string;

  protocolVersion: number;

  extensionVersion: string | null;

  updateAvailable: boolean;

  bridgeUrl: string;

}



async function syncBridgeSettings(): Promise<void> {

  const { config, sessionSecrets } = useConfigStore.getState();

  const session =

    config.sessions.find((s) => s.id === config.activeSessionId) ?? config.sessions[0];

  const authToken = session ? sessionSecrets[session.id] : undefined;



  const enabled = config.browserExtensionEnabled;

  const daemonUrl = session?.baseUrl ?? "http://127.0.0.1:8000";



  if (window.avar?.isElectron) {

    await window.avar.setExtensionBridgeEnabled(enabled);

    await window.avar.setExtensionBridgeConfig({ daemonUrl, authToken });

    return;

  }



  try {

    await fetch("/extension/settings", {

      method: "POST",

      headers: { "Content-Type": "application/json" },

      body: JSON.stringify({ enabled, daemonUrl, authToken }),

    });

  } catch {

    // Dev server may be unavailable outside Vite.

  }

}



export async function getExtensionBridgeUrl(): Promise<string> {

  if (window.avar?.isElectron) {

    return window.avar.getExtensionBridgeUrl();

  }

  if (typeof window !== "undefined" && window.location.origin && window.location.origin !== "null") {

    return window.location.origin;

  }

  return DEFAULT_EXTENSION_GUI_URL;

}



/** @deprecated Use getExtensionBridgeUrl */

export function getExtensionGuiUrl(): string {

  if (window.avar?.isElectron) {

    return ELECTRON_EXTENSION_BRIDGE_URL;

  }

  if (typeof window !== "undefined" && window.location.origin && window.location.origin !== "null") {

    return window.location.origin;

  }

  return DEFAULT_EXTENSION_GUI_URL;

}



export function initBrowserExtensionBridge(): () => void {

  void syncBridgeSettings();



  const unsubConfig = useConfigStore.subscribe((state, prev) => {

    const enabledChanged =

      state.config.browserExtensionEnabled !== prev.config.browserExtensionEnabled;

    const sessionChanged = state.config.activeSessionId !== prev.config.activeSessionId;

    const sessionsChanged = state.config.sessions !== prev.config.sessions;

    if (enabledChanged || sessionChanged || sessionsChanged) {

      void syncBridgeSettings();

    }

  });



  const unsubSecrets = useConfigStore.subscribe((state, prev) => {

    if (state.sessionSecrets !== prev.sessionSecrets) {

      void syncBridgeSettings();

    }

  });



  return () => {

    unsubConfig();

    unsubSecrets();

  };

}



async function fetchBridgeState(): Promise<Omit<ExtensionBridgeState, "enabled" | "loading">> {

  const bridgeUrl = await getExtensionBridgeUrl();



  if (window.avar?.isElectron) {

    const payload = await window.avar.getExtensionBridgeState();

    const extensionVersion = payload.lastExtensionVersion ?? null;

    return {

      connected: Boolean(payload.connected),

      bridgeVersion: payload.bridgeVersion ?? "0.1.0",

      protocolVersion: payload.protocolVersion ?? 1,

      extensionVersion,

      updateAvailable:

        extensionVersion !== null && extensionVersion !== BUNDLED_EXTENSION_VERSION,

      bridgeUrl,

    };

  }



  const response = await fetch(`${bridgeUrl}/extension/status`);

  if (!response.ok) {

    throw new Error("status unavailable");

  }

  const payload = (await response.json()) as {

    connected?: boolean;

    bridgeVersion?: string;

    protocolVersion?: number;

    lastExtensionVersion?: string | null;

  };

  const extensionVersion = payload.lastExtensionVersion ?? null;

  return {

    connected: Boolean(payload.connected),

    bridgeVersion: payload.bridgeVersion ?? "0.1.0",

    protocolVersion: payload.protocolVersion ?? 1,

    extensionVersion,

    updateAvailable:

      extensionVersion !== null && extensionVersion !== BUNDLED_EXTENSION_VERSION,

    bridgeUrl,

  };

}



export function useExtensionBridgeStatus(): ExtensionBridgeState {

  const enabled = useConfigStore((s) => s.config.browserExtensionEnabled);

  const [status, setStatus] = useState<Omit<ExtensionBridgeState, "enabled">>({

    connected: false,

    loading: true,

    bridgeVersion: "0.1.0",

    protocolVersion: 1,

    extensionVersion: null,

    updateAvailable: false,

    bridgeUrl: getExtensionGuiUrl(),

  });



  useEffect(() => {

    if (!enabled) {

      setStatus((prev) => ({

        ...prev,

        connected: false,

        loading: false,

      }));

      return;

    }



    let cancelled = false;



    async function poll() {

      try {

        const next = await fetchBridgeState();

        if (!cancelled) {

          setStatus({ ...next, loading: false });

        }

      } catch {

        if (!cancelled) {

          setStatus((prev) => ({

            ...prev,

            connected: false,

            loading: false,

          }));

        }

      }

    }



    setStatus((prev) => ({ ...prev, loading: true }));

    void poll();

    const timer = window.setInterval(() => void poll(), 3000);

    return () => {

      cancelled = true;

      window.clearInterval(timer);

    };

  }, [enabled]);



  return { enabled, ...status };

}

