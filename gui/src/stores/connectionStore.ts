import { create } from "zustand";
import { createDaemonClient, type DaemonClient } from "@/api/daemon";
import { useConfigStore } from "@/stores/configStore";
import { useDataStore } from "@/stores/dataStore";
import { appLogger } from "@/lib/appLogger";
import type { GuiSession } from "@/config/defaults";

export type ConnectionState = "disconnected" | "connecting" | "connected";

const PING_TIMEOUT_MS = 4000;

interface ConnectionStoreState {
  connection: ConnectionState;
  client: DaemonClient | null;
  activeSession: GuiSession | null;
  pingTimerId: ReturnType<typeof setInterval> | null;
  reconnectClient: () => void;
  checkConnection: () => Promise<boolean>;
  startPingMonitor: () => void;
  stopPingMonitor: () => void;
}

function resolveActiveSession(): GuiSession | null {
  const { config } = useConfigStore.getState();
  return (
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0] ??
    null
  );
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
        appLogger.gui.warn("No client — disconnected");
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
      appLogger.gui.info(ok ? "Daemon reachable" : "Daemon unreachable");
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
