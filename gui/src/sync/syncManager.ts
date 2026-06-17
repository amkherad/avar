import { parseSnapshotPayload } from "@/api/snapshot";
import type { SnapshotPayload } from "@/api/types";
import type { DaemonClient } from "@/api/daemon";
import { useConfigStore } from "@/stores/configStore";
import { useConnectionStore } from "@/stores/connectionStore";
import { useDataStore } from "@/stores/dataStore";

type SyncStopFn = () => void;

let activeStop: SyncStopFn | null = null;

function stopActiveSync(): void {
  if (activeStop) {
    activeStop();
    activeStop = null;
  }
}

function startPollSync(intervalMs: number): SyncStopFn {
  void useDataStore.getState().refresh();
  const timerId = window.setInterval(() => {
    if (useConnectionStore.getState().connection === "connected") {
      void useDataStore.getState().refresh();
    }
  }, intervalMs);
  return () => window.clearInterval(timerId);
}

function startSseSync(client: DaemonClient): SyncStopFn {
  const url = client.getEventsUrl();
  const source = new EventSource(url, { withCredentials: false });

  // EventSource cannot set Authorization; append token as query param if needed.
  // For token auth, fall back to poll when token is configured.
  const { config } = useConfigStore.getState();
  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0];
  if (session?.authToken) {
    source.close();
    return startPollSync(config.refreshIntervalMs);
  }

  const apply = (payload: SnapshotPayload) => {
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
      /* ignore malformed events */
    }
  });

  source.onerror = () => {
    // EventSource auto-reconnects; only treat a fully closed source as dead.
    if (source.readyState === EventSource.CLOSED) {
      useConnectionStore.setState({ connection: "disconnected" });
    }
  };

  void useDataStore.getState().refresh();

  return () => source.close();
}

function startWebSocketSync(client: DaemonClient): SyncStopFn {
  const { config } = useConfigStore.getState();
  const session =
    config.sessions.find((s) => s.id === config.activeSessionId) ??
    config.sessions[0];

  if (session?.authToken) {
    return startPollSync(config.refreshIntervalMs);
  }

  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  let closed = false;

  const connect = () => {
    ws = new WebSocket(client.getWebSocketUrl());

    ws.onopen = () => {
      if (useConnectionStore.getState().connection !== "connected") {
        useConnectionStore.setState({ connection: "connected" });
      }
      void useDataStore.getState().refresh();
    };

    ws.onmessage = (event) => {
      try {
        const parsed = parseSnapshotPayload(JSON.parse(String(event.data)));
        if (parsed?.type === "snapshot") {
          useDataStore.getState().applySnapshot(parsed);
        }
      } catch {
        /* ignore */
      }
    };

    ws.onclose = () => {
      if (!closed) {
        reconnectTimer = window.setTimeout(connect, 2000);
      }
    };
  };

  connect();

  const keepalive = window.setInterval(() => {
    if (ws?.readyState === WebSocket.OPEN) {
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
    return;
  }

  switch (config.syncChannel) {
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
  const unsubConnection = useConnectionStore.subscribe((state, prev) => {
    if (state.connection === "connected" && prev.connection !== "connected") {
      restartDataSync();
    }
    if (state.connection !== "connected" && prev.connection === "connected") {
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
      restartDataSync();
    }
  });

  useConnectionStore.getState().reconnectClient();
  useConnectionStore.getState().startPingMonitor();

  if (useConnectionStore.getState().connection === "connected") {
    restartDataSync();
  }

  return () => {
    unsubConnection();
    unsubConfig();
    stopDataSync();
    useConnectionStore.getState().stopPingMonitor();
  };
}
