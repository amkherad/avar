export type ProxyType = "none" | "http" | "https" | "socks5";

export interface ProxySettings {
  enabled: boolean;
  type: ProxyType;
  host: string;
  port: string;
  username: string;
  password: string;
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
