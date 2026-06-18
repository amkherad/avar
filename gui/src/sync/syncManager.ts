import { parseSnapshotPayload } from "@/api/snapshot";
import type { SnapshotPayload } from "@/api/types";
import type { DaemonClient } from "@/api/daemon";
import type { SyncChannelId } from "@/config/defaults";
import { appLogger } from "@/lib/appLogger";
import { useConfigStore } from "@/stores/configStore";
import { ensureElectronSession, useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";

type SyncStopFn = () => void;

let activeStop: SyncStopFn | null = null;

function stopActiveSync(): void {
  if (activeStop) {
    appLogger.gui.debug("Stopping data sync");
    activeStop();
    activeStop = null;
  }
}

function startPollSync(intervalMs: number): SyncStopFn {
  appLogger.gui.info(`Poll sync started (${intervalMs}ms)`);
  void useDataStore.getState().refresh();
  const timerId = window.setInterval(() => {
    if (useConnectionStore.getState().connection === "connected") {
      appLogger.gui.debug("Poll sync tick");
      void useDataStore.getState().refresh();
    }
  }, intervalMs);
  return () => window.clearInterval(timerId);
}

function resolveSyncChannel(
  preferred: SyncChannelId,
  session: { authToken?: string } | undefined,
): SyncChannelId {
  if (preferred === "sse") {
    if (typeof EventSource === "undefined") {
      appLogger.gui.warn("SSE unavailable in this environment; falling back to poll");
      return "poll";
    }
    if (session?.authToken) {
      appLogger.gui.warn("SSE unavailable with auth token; falling back to poll");
      return "poll";
    }
    return "sse";
  }
  if (preferred === "websocket") {
    if (session?.authToken) {
      appLogger.gui.warn("WebSocket unavailable with auth token; falling back to poll");
      return "poll";
    }
    return "websocket";
  }
  return preferred;
}

function startSseSync(client: DaemonClient): SyncStopFn {
  const url = client.getEventsUrl();
  appLogger.gui.info("SSE sync connecting", url);
  const source = new EventSource(url, { withCredentials: false });

  const apply = (payload: SnapshotPayload) => {
    appLogger.gui.debug("SSE snapshot received");
    useDataStore.getState().applySnapshot(payload);
  };

  source.addEventListener("snapshot", (event) => {
    try {
      const parsed = parseSnapshotPayload(
        JSON.parse((event as MessageEvent<string>).data),
      );
      if (parsed) {
        apply(parsed);
      }
    } catch {
      appLogger.gui.warn("Malformed SSE snapshot event");
    }
  });

  source.onopen = () => {
    appLogger.gui.info("SSE connection opened");
  };

  source.onerror = () => {
    if (source.readyState === EventSource.CLOSED) {
      appLogger.gui.error("SSE connection closed");
      useConnectionStore.setState({ connection: "disconnected" });
    } else {
      appLogger.gui.debug("SSE connection error (reconnecting)");
    }
  };

  void useDataStore.getState().refresh();

  return () => source.close();
}

function startWebSocketSync(client: DaemonClient): SyncStopFn {
  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let closed = false;

  const connect = () => {
    const wsUrl = client.getWebSocketUrl();
    appLogger.gui.info("WebSocket sync connecting", wsUrl);
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      appLogger.gui.info("WebSocket connection opened");
      if (useConnectionStore.getState().connection !== "connected") {
        useConnectionStore.setState({ connection: "connected" });
      }
      void useDataStore.getState().refresh();
    };

    ws.onmessage = (event) => {
      try {
        const parsed = parseSnapshotPayload(JSON.parse(String(event.data)));
        if (parsed?.type === "snapshot") {
          appLogger.gui.debug("WebSocket snapshot received");
          useDataStore.getState().applySnapshot(parsed);
        }
      } catch {
        appLogger.gui.debug("Ignored WebSocket message");
      }
    };

    ws.onclose = () => {
      appLogger.gui.debug("WebSocket closed");
      if (!closed) {
        reconnectTimer = window.setTimeout(connect, 2000);
      }
    };

    ws.onerror = () => {
      appLogger.gui.warn("WebSocket error");
    };
  };

  connect();

  const keepalive = window.setInterval(() => {
    if (ws?.readyState === WebSocket.OPEN) {
      appLogger.gui.debug("WebSocket keepalive ping");
      ws.send("ping");
    }
  }, 15000);

  return () => {
    closed = true;
    window.clearInterval(keepalive);
    if (reconnectTimer !== null) {
      window.clearTimeout(reconnectTimer);
    }
    ws?.close();
  };
}

export function restartDataSync(): void {
  stopActiveSync();

  const { config } = useConfigStore.getState();
  const { client, connection } = useConnectionStore.getState();
  if (!client || connection !== "connected") {
    appLogger.gui.debug("Data sync not started (not connected)");
    return;
  }

  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0];
  const channel = resolveSyncChannel(config.syncChannel, session);

  appLogger.gui.info(`Starting data sync (${channel})`);

  switch (channel) {
    case "sse":
      activeStop = startSseSync(client);
      break;
    case "websocket":
      activeStop = startWebSocketSync(client);
      break;
    default:
      activeStop = startPollSync(Math.max(config.refreshIntervalMs, 1000));
      break;
  }
}

export function stopDataSync(): void {
  stopActiveSync();
}

export function initSyncCoordinator(): () => void {
  appLogger.gui.debug("Initializing sync coordinator");

  const unsubConnection = useConnectionStore.subscribe((state, prev) => {
    if (state.connection === "connected" && prev.connection !== "connected") {
      appLogger.gui.info("Connection established — starting sync");
      restartDataSync();
    }
    if (state.connection !== "connected" && prev.connection === "connected") {
      appLogger.gui.warn("Connection lost — stopping sync");
      stopDataSync();
    }
  });

  const unsubConfig = useConfigStore.subscribe((state, prev) => {
    const channelChanged = state.config.syncChannel !== prev.config.syncChannel;
    const intervalChanged =
      state.config.refreshIntervalMs !== prev.config.refreshIntervalMs;
    if (
      useConnectionStore.getState().connection === "connected" &&
      (channelChanged || intervalChanged)
    ) {
      appLogger.gui.info("Sync config changed — restarting sync");
      restartDataSync();
    }
  });

  void ensureElectronSession().then(() => {
    useConnectionStore.getState().reconnectClient();
    useConnectionStore.getState().startPingMonitor();

    if (useConnectionStore.getState().connection === "connected") {
      restartDataSync();
    }
  });

  return () => {
    unsubConnection();
    unsubConfig();
    stopDataSync();
    useConnectionStore.getState().stopPingMonitor();
  };
}
