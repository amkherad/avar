export type ProxyType = "none" | "http" | "https" | "socks5";

export interface ProxySettings {
  enabled: boolean;
  type: ProxyType;
  host: string;
  port: string;
  username: string;
  password: string;
  noProxy?: string;
}

export const defaultProxySettings = (): ProxySettings => ({
  enabled: false,
  type: "http",
  host: "",
  port: "",
  username: "",
  password: "",
});

export function proxySettingsToRpcParams(
  proxy: ProxySettings,
): Record<string, unknown> | undefined {
  if (!proxy.enabled || !proxy.host.trim()) {
    return undefined;
  }
  return {
    type: proxy.type,
    host: proxy.host.trim(),
    port: proxy.port.trim() ? Number(proxy.port) : undefined,
    username: proxy.username.trim() || undefined,
    password: proxy.password || undefined,
  };
}

/** Maps a stored proxy URL from download items back to form fields. */
export function proxySettingsFromUrl(url: string | null | undefined): ProxySettings {
  if (!url) {
    return defaultProxySettings();
  }
  try {
    const parsed = new URL(url);
    return {
      enabled: true,
      type: (parsed.protocol.replace(":", "") as ProxyType) || "http",
      host: parsed.hostname,
      port: parsed.port,
      username: decodeURIComponent(parsed.username),
      password: decodeURIComponent(parsed.password),
    };
  } catch {
    return defaultProxySettings();
  }
}
