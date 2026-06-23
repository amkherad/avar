import { create } from "zustand";
import { createDaemonClient, type DaemonClient } from "@/api/daemon";
import type { SystemStatsInfo } from "@/api/types";
import {
  createElectronSession,
  ELECTRON_SESSION_ID,
  type GuiSession,
} from "@/config/defaults";
import { isPushSyncChannel, resolveSyncChannel } from "@/lib/syncChannel";
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
  lastStreamAt: number | null;
  reconnectClient: () => void;
  checkConnection: () => Promise<boolean>;
  noteStreamActivity: (stats?: SystemStatsInfo) => void;
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

function activePushChannel(): boolean {
  const { config } = useConfigStore.getState();
  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0];
  return isPushSyncChannel(resolveSyncChannel(config.syncChannel, session));
}

async function pingWithTimeout(client: DaemonClient): Promise<boolean> {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), PING_TIMEOUT_MS);
  try {
    const stats = await client.systemStats(controller.signal);
    useDataStore.getState().setStats(stats);
    return stats.status === "ok";
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
  lastStreamAt: null,

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
      lastStreamAt: null,
    });
  },

  noteStreamActivity: (stats) => {
    const prevConnection = get().connection;
    if (stats) {
      useDataStore.getState().setStats(stats);
    }
    set({ lastStreamAt: Date.now() });
    if (prevConnection !== "connected") {
      appLogger.gui.info("Daemon reachable (stream)");
      set({ connection: "connected" });
    }
  },

  checkConnection: async () => {
    const { client, connection: prevConnection } = get();
    if (!client) {
      if (prevConnection !== "disconnected") {
        appLogger.gui.error("Cannot connect to daemon — no active session");
        set({ connection: "disconnected", lastStreamAt: null });
      }
      return false;
    }

    if (activePushChannel()) {
      const { config } = useConfigStore.getState();
      const intervalMs = Math.max(config.pingIntervalMs, 500);
      const lastStreamAt = get().lastStreamAt;
      const fresh =
        lastStreamAt !== null && Date.now() - lastStreamAt <= intervalMs * 2;
      const nextConnection = fresh ? "connected" : "connecting";
      if (nextConnection !== prevConnection) {
        if (fresh) {
          appLogger.gui.info("Daemon reachable");
        } else if (prevConnection === "connected") {
          appLogger.gui.error("Cannot connect to daemon");
        }
        set({ connection: nextConnection });
      }
      return fresh;
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
        useDataStore.getState().setStats(null);
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

  const syncChannelChanged =
    state.config.syncChannel !== prev.config.syncChannel;

  if (sessionChanged || syncChannelChanged) {
    useDataStore.getState().clear();
    useConnectionStore.getState().reconnectClient();
    useConnectionStore.getState().startPingMonitor();
  } else if (pingIntervalChanged) {
    useConnectionStore.getState().startPingMonitor();
  }
});
