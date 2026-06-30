import type { AddDownloadPrefill } from "@/lib/addDownloadPrefill";
import { appLogger } from "@/lib/appLogger";
import { getExtensionBridgeUrl } from "@/lib/browserExtensionBridge";

export interface LinkRefreshCapturedPayload extends AddDownloadPrefill {
  url: string;
}

export async function startLinkRefreshSession(downloadId: string): Promise<string> {
  appLogger.gui.debug("Link refresh session starting", downloadId);
  const bridgeUrl = await getExtensionBridgeUrl();
  const response = await fetch(`${bridgeUrl}/extension/link-refresh/start`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ downloadId }),
  });
  if (!response.ok) {
    throw new Error("Failed to start link refresh session");
  }
  const body = (await response.json()) as { ok?: boolean; sessionId?: string };
  if (!body.ok || !body.sessionId) {
    throw new Error("Failed to start link refresh session");
  }
  appLogger.gui.debug("Link refresh session started", body.sessionId);
  return body.sessionId;
}

export async function cancelLinkRefreshSession(sessionId?: string): Promise<void> {
  if (sessionId) {
    appLogger.gui.debug("Link refresh session cancelled", sessionId);
  }
  try {
    const bridgeUrl = await getExtensionBridgeUrl();
    await fetch(`${bridgeUrl}/extension/link-refresh/cancel`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(sessionId ? { sessionId } : {}),
    });
  } catch {
    // Best effort cleanup.
  }
}

export async function pollLinkRefreshSession(
  sessionId: string,
  signal?: AbortSignal,
): Promise<LinkRefreshCapturedPayload | null> {
  const bridgeUrl = await getExtensionBridgeUrl();
  const response = await fetch(
    `${bridgeUrl}/extension/link-refresh/${encodeURIComponent(sessionId)}`,
    { signal },
  );
  if (!response.ok) {
    return null;
  }
  const body = (await response.json()) as {
    ok?: boolean;
    captured?: boolean;
    payload?: LinkRefreshCapturedPayload;
  };
  if (!body.ok || !body.captured || !body.payload?.url) {
    return null;
  }
  appLogger.gui.debug("Link refresh captured", sessionId);
  return body.payload;
}
