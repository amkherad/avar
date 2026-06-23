import { parseSnapshotPayload, parseStreamStatsPayload } from "@/api/snapshot";
import type { SnapshotPayload, SystemStatsInfo } from "@/api/types";
import type { DaemonClient } from "@/api/daemon";
import { isPushSyncChannel, resolveSyncChannel } from "@/lib/syncChannel";
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

function applyStreamPayload(payload: SnapshotPayload, stats?: SystemStatsInfo): void {
  appLogger.gui.debug("Stream snapshot received");
  useDataStore.getState().applySnapshot(payload);
  useConnectionStore.getState().noteStreamActivity(stats ?? payload.stats);
}

function startSseSync(client: DaemonClient): SyncStopFn {
  const url = client.getEventsUrl();
  appLogger.gui.info("SSE sync connecting", url);
  const source = new EventSource(url, { withCredentials: false });

  source.addEventListener("snapshot", (event) => {
    try {
      const parsed = parseSnapshotPayload(
        JSON.parse((event as MessageEvent<string>).data),
      );
      if (parsed) {
        applyStreamPayload(parsed);
      }
    } catch {
      appLogger.gui.warn("Malformed SSE snapshot event");
    }
  });

  source.onopen = () => {
    appLogger.gui.info("SSE connection opened");
    useConnectionStore.getState().noteStreamActivity();
  };

  source.onerror = () => {
    if (source.readyState === EventSource.CLOSED) {
      appLogger.gui.warn("SSE connection closed (daemon may still be running)");
    } else {
      appLogger.gui.debug("SSE connection error (reconnecting)");
    }
  };

  return () => source.close();
}

function startWebSocketSync(client: DaemonClient): SyncStopFn {
  let ws: WebSocket | null = null;
  let reconnectTimer: number | null = null;
  let closed = false;

  const connect = () => {
    const wsUrl = client.getWebSocketUrl();
    appLogger.gui.info("WebSocket sync connecting", wsUrl);
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      appLogger.gui.info("WebSocket connection opened");
      useConnectionStore.getState().noteStreamActivity();
    };

    ws.onmessage = (event) => {
      try {
        const raw = JSON.parse(String(event.data));
        const stats = parseStreamStatsPayload(raw);
        if (stats) {
          appLogger.gui.debug("WebSocket stats received");
          useConnectionStore.getState().noteStreamActivity(stats);
          return;
        }

        const parsed = parseSnapshotPayload(raw);
        if (parsed?.type === "snapshot") {
          applyStreamPayload(parsed);
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

  const { config } = useConfigStore.getState();
  const keepaliveMs = Math.max(config.pingIntervalMs, 500);
  const keepalive = window.setInterval(() => {
    if (ws?.readyState === WebSocket.OPEN) {
      appLogger.gui.debug("WebSocket keepalive ping");
      ws.send("ping");
    }
  }, keepaliveMs);

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
  if (!client) {
    appLogger.gui.debug("Data sync not started (no client)");
    return;
  }

  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0];
  const channel = resolveSyncChannel(config.syncChannel, session);

  if (channel === "poll") {
    if (connection !== "connected") {
      appLogger.gui.debug("Data sync not started (not connected)");
      return;
    }
    appLogger.gui.info("Starting data sync (poll)");
    activeStop = startPollSync(Math.max(config.refreshIntervalMs, 1000));
    return;
  }

  appLogger.gui.info(`Starting data sync (${channel})`);
  activeStop =
    channel === "websocket" ? startWebSocketSync(client) : startSseSync(client);
}

export function stopDataSync(): void {
  stopActiveSync();
}

export function initSyncCoordinator(): () => void {
  appLogger.gui.debug("Initializing sync coordinator");

  const unsubConnection = useConnectionStore.subscribe((state, prev) => {
    const { config } = useConfigStore.getState();
    const session =
      config.sessions.find((s) => s.id === config.activeSessionId) ??
      config.sessions[0];
    const channel = resolveSyncChannel(config.syncChannel, session);

    if (
      isPushSyncChannel(channel) &&
      state.client &&
      (state.client !== prev.client || state.activeSession !== prev.activeSession)
    ) {
      restartDataSync();
    }

    if (state.connection === "connected" && prev.connection !== "connected") {
      appLogger.gui.info("Connection established — starting sync");
      if (!isPushSyncChannel(channel)) {
        void useDataStore.getState().refresh();
      }
      restartDataSync();
    }
    if (state.connection !== "connected" && prev.connection === "connected") {
      const session =
        config.sessions.find((s) => s.id === config.activeSessionId) ??
        config.sessions[0];
      const channel = resolveSyncChannel(config.syncChannel, session);
      if (!isPushSyncChannel(channel)) {
        appLogger.gui.warn("Connection lost — stopping sync");
        stopDataSync();
      }
    }
  });

  const unsubConfig = useConfigStore.subscribe((state, prev) => {
    const channelChanged = state.config.syncChannel !== prev.config.syncChannel;
    const intervalChanged =
      state.config.refreshIntervalMs !== prev.config.refreshIntervalMs;
    if (channelChanged || intervalChanged) {
      appLogger.gui.info("Sync config changed — restarting sync");
      restartDataSync();
    }
  });

  void ensureElectronSession().then(() => {
    useConnectionStore.getState().reconnectClient();
    useConnectionStore.getState().startPingMonitor();
    restartDataSync();
  });

  return () => {
    unsubConnection();
    unsubConfig();
    stopDataSync();
    useConnectionStore.getState().stopPingMonitor();
  };
}
