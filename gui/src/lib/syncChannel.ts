import type { SyncChannelId } from "@/config/defaults";
import { appLogger } from "@/lib/appLogger";

export function resolveSyncChannel(
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

export function isPushSyncChannel(channel: SyncChannelId): boolean {
  return channel === "sse" || channel === "websocket";
}
