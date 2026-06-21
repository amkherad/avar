import type { GuiSession } from "@/config/defaults";

const LOCAL_HOSTNAMES = new Set([
  "localhost",
  "127.0.0.1",
  "::1",
  "[::1]",
  "0.0.0.0",
]);

function normalizeHostname(hostname: string): string {
  const trimmed = hostname.trim().toLowerCase();
  if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
    return trimmed.slice(1, -1);
  }
  return trimmed;
}

export function isLocalHostAddress(hostname: string): boolean {
  const normalized = normalizeHostname(hostname);
  if (LOCAL_HOSTNAMES.has(normalized)) {
    return true;
  }
  if (normalized.startsWith("127.")) {
    return true;
  }
  return false;
}

export function isLocalSessionUrl(baseUrl: string): boolean {
  const trimmed = baseUrl.trim();
  if (!trimmed) {
    return true;
  }

  try {
    const parsed = new URL(trimmed);
    if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
      return false;
    }
    return isLocalHostAddress(parsed.hostname);
  } catch {
    return false;
  }
}

export function isRemoteSession(session: GuiSession): boolean {
  if (session.forceRemote) {
    return true;
  }
  if (session.useElectronProxy) {
    return false;
  }
  if (session.useRelativeApi) {
    return false;
  }
  return !isLocalSessionUrl(session.baseUrl);
}
