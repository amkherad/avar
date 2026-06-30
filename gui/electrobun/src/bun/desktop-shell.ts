/**
 * Self-contained desktop helpers for the Electrobun shell.
 *
 * Electrobun bundles the Bun entrypoint, so we cannot require gui/desktop/*.cjs
 * via relative paths from the bundle output directory.
 */

const DAEMON_TARGET = process.env.AVAR_DAEMON_URL || "http://127.0.0.1:8000";
const PROXY_HOST = "127.0.0.1";
const PROXY_PORT = Number(process.env.AVAR_ELECTRON_PROXY_PORT || 18765);
/** Matches gui/dev-server.cjs — Vite binds to localhost (IPv6 on Windows). */
const DEV_SERVER_FALLBACK = "http://localhost:56000";

export const APP_TITLE = "Avar";
export const DEFAULT_WINDOW_WIDTH = 1280;
export const DEFAULT_WINDOW_HEIGHT = 800;

let proxyServer: ReturnType<typeof Bun.serve> | null = null;

export function startDaemonProxy(): void {
  if (proxyServer) {
    return;
  }

  try {
    proxyServer = Bun.serve({
      port: PROXY_PORT,
      hostname: PROXY_HOST,
      async fetch(request) {
        const incoming = new URL(request.url);
        const target = new URL(
          `${incoming.pathname}${incoming.search}`,
          DAEMON_TARGET,
        );

        const headers = new Headers(request.headers);
        headers.set("host", target.host);

        return fetch(target, {
          method: request.method,
          headers,
          body:
            request.method === "GET" || request.method === "HEAD"
              ? undefined
              : request.body,
        });
      },
    });
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (!message.includes("EADDRINUSE")) {
      throw error;
    }
    console.warn(`Daemon proxy port ${PROXY_PORT} already in use — reusing it.`);
  }
}

export function stopDaemonProxy(): void {
  if (!proxyServer) {
    return;
  }
  proxyServer.stop();
  proxyServer = null;
}

export async function waitForUrl(url: string, timeoutMs = 60_000): Promise<void> {
  const started = Date.now();

  while (Date.now() - started <= timeoutMs) {
    try {
      const response = await fetch(url, { method: "HEAD" });
      if (response.ok || response.status === 404) {
        return;
      }
    } catch {
      // Server not ready yet.
    }

    await Bun.sleep(250);
  }

  throw new Error(`Timed out waiting for ${url}`);
}

export async function resolveShellGuiUrl(channel: string): Promise<string> {
  const bundledGuiUrl = process.env.AVAR_GUI_URL?.trim() || "";
  const viteDevServerUrl =
    process.env.VITE_DEV_SERVER_URL?.trim() ||
    (channel === "dev" ? DEV_SERVER_FALLBACK : "");

  if (bundledGuiUrl) {
    await waitForUrl(bundledGuiUrl);
    return bundledGuiUrl;
  }

  if (viteDevServerUrl) {
    await waitForUrl(viteDevServerUrl);
    return viteDevServerUrl;
  }

  return "views://mainview/index.html";
}
