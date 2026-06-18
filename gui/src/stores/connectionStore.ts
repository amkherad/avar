import { create } from "zustand";
import { createDaemonClient, type DaemonClient } from "@/api/daemon";
import {
  createElectronSession,
  ELECTRON_SESSION_ID,
  type GuiSession,
} from "@/config/defaults";
import { useConfigStore } from "@/stores/configStore";
import { useDataStore } from "@/stores/dataStore";
import { appLogger } from "@/lib/appLogger";

export type ConnectionState = "disconnected" | "connecting" | "connected";

const PING_TIMEOUT_MS = 4000;

interface ConnectionStoreState {
  connection: ConnectionState;
  client: DaemonClient | null;
  activeSession: GuiSession | null;
  pingTimerId: number | null;
  reconnectClient: () => void;
  checkConnection: () => Promise<boolean>;
  startPingMonitor: () => void;
  stopPingMonitor: () => void;
}

async function resolveElectronProxyUrl(): Promise<string | null> {
  if (!window.avar?.isElectron || !window.avar.getProxyBaseUrl) {
    return null;
  }
  try {
    return await window.avar.getProxyBaseUrl();
  } catch {
    return null;
  }
}

function resolveActiveSession(): GuiSession | null {
  const { config, getSessionWithSecrets } = useConfigStore.getState();
  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0] ??
    null;
  return session ? getSessionWithSecrets(session) : null;
}

function buildClient(session: GuiSession | null): DaemonClient | null {
  if (!session) {
    return null;
  }
  return createDaemonClient({
    baseUrl: session.baseUrl,
    authToken: session.authToken,
    useRelativeApi: session.useRelativeApi,
  });
}

async function pingWithTimeout(client: DaemonClient): Promise<boolean> {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), PING_TIMEOUT_MS);
  try {
    return await client.ping(controller.signal);
  } catch {
    return false;
  } finally {
    window.clearTimeout(timer);
  }
}

export async function ensureElectronSession(): Promise<void> {
  if (!window.avar?.isElectron) {
    return;
  }

  const proxyUrl = await resolveElectronProxyUrl();
  if (!proxyUrl) {
    return;
  }

  const { config, setConfig } = useConfigStore.getState();
  const existing = config.sessions.find((s) => s.id === ELECTRON_SESSION_ID);
  const electronSession = createElectronSession(proxyUrl);

  if (!existing) {
    setConfig({
      ...config,
      sessions: [electronSession, ...config.sessions],
      activeSessionId: ELECTRON_SESSION_ID,
    });
    return;
  }

  if (existing.baseUrl !== proxyUrl) {
    setConfig({
      ...config,
      sessions: config.sessions.map((session) =>
        session.id === ELECTRON_SESSION_ID
          ? { ...session, baseUrl: proxyUrl }
          : session,
      ),
    });
  }
}

export const useConnectionStore = create<ConnectionStoreState>()((set, get) => ({
  connection: "disconnected",
  client: null,
  activeSession: null,
  pingTimerId: null,

  reconnectClient: () => {
    const session = resolveActiveSession();
    appLogger.gui.debug(
      "Reconnecting client",
      session?.label ?? "no session",
    );
    set({
      activeSession: session,
      client: buildClient(session),
      connection: session ? "connecting" : "disconnected",
    });
  },

  checkConnection: async () => {
    const { client, connection: prevConnection } = get();
    if (!client) {
      if (prevConnection !== "disconnected") {
        appLogger.gui.error("Cannot connect to daemon — no active session");
        set({ connection: "disconnected" });
      }
      return false;
    }

    if (prevConnection === "disconnected") {
      set({ connection: "connecting" });
    }

    appLogger.gui.debug("Ping daemon");
    const ok = await pingWithTimeout(client);
    const nextConnection = ok ? "connected" : "disconnected";
    if (nextConnection !== get().connection) {
      if (ok) {
        appLogger.gui.info("Daemon reachable");
      } else {
        appLogger.gui.error("Cannot connect to daemon");
      }
      set({ connection: nextConnection });
    }
    return ok;
  },

  startPingMonitor: () => {
    const existing = get().pingTimerId;
    if (existing !== null) {
      window.clearInterval(existing);
    }

    const { config } = useConfigStore.getState();
    const intervalMs = Math.max(config.pingIntervalMs, 500);

    const timerId = window.setInterval(() => {
      void get().checkConnection();
    }, intervalMs);

    set({ pingTimerId: timerId });
    void get().checkConnection();
  },

  stopPingMonitor: () => {
    const timerId = get().pingTimerId;
    if (timerId !== null) {
      window.clearInterval(timerId);
      set({ pingTimerId: null });
    }
  },
}));

useConfigStore.subscribe((state, prev) => {
  const sessionChanged =
    state.config.activeSessionId !== prev.config.activeSessionId ||
    state.config.sessions !== prev.config.sessions;

  const pingIntervalChanged =
    state.config.pingIntervalMs !== prev.config.pingIntervalMs;

  if (sessionChanged) {
    useDataStore.getState().clear();
    useConnectionStore.getState().reconnectClient();
    useConnectionStore.getState().startPingMonitor();
  } else if (pingIntervalChanged) {
    useConnectionStore.getState().startPingMonitor();
  }
});
