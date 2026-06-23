/**
 * Experimental Tiny desktop shell for Avar GUI.
 *
 * Uses tinytron (https://github.com/Rafi993/tiny) — a minimal webview wrapper.
 * Runs the same React SPA as Electron in plain web mode (no preload bridge).
 *
 * Prerequisites:
 * - tinytron built successfully (`npm install` with C++ toolchain, or see tiny/README.md)
 * - Avar daemon on port 8000 (or set AVAR_DAEMON_URL)
 *
 * Dev: enable "Use dev proxy" in session settings (same as `npm run dev`).
 */

const {
  APP_TITLE,
  DEFAULT_WINDOW_HEIGHT,
  DEFAULT_WINDOW_WIDTH,
  VITE_DEV_SERVER_URL,
  BUNDLED_GUI_URL,
} = require("../desktop/env.cjs");
const { startDaemonProxy, stopDaemonProxy } = require("../desktop/daemon-proxy.cjs");
const { resolveGuiUrl } = require("../desktop/resolve-gui-url.cjs");
const {
  startStaticServer,
  stopStaticServer,
} = require("../desktop/static-server.cjs");
const { SHELL_TINY } = require("../desktop/shells.cjs");

function loadTiny() {
  try {
    return require("tinytron");
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(
      [
        "Failed to load tinytron.",
        "Install build tools (Visual Studio C++ on Windows, Xcode CLT on macOS, build-essential on Linux)",
        "then run: node scripts/install-tinytron.cjs",
        "",
        `Details: ${message}`,
      ].join("\n"),
    );
    process.exit(1);
  }
}

function waitForUrl(url, timeoutMs = 60_000) {
  return new Promise((resolve, reject) => {
    const started = Date.now();

    const poll = async () => {
      try {
        const response = await fetch(url, { method: "HEAD" });
        if (response.ok || response.status === 404) {
          resolve();
          return;
        }
      } catch {
        // Server not ready yet.
      }

      if (Date.now() - started > timeoutMs) {
        reject(new Error(`Timed out waiting for ${url}`));
        return;
      }

      setTimeout(() => {
        void poll();
      }, 250);
    };

    void poll();
  });
}

async function resolveShellGuiUrl() {
  if (BUNDLED_GUI_URL || VITE_DEV_SERVER_URL) {
    const url = resolveGuiUrl({ isPackaged: false });
    await waitForUrl(url);
    return url;
  }

  const staticServer = startStaticServer();
  await new Promise((resolve) => {
    const check = () => {
      if (staticServer.url) {
        resolve();
        return;
      }
      setTimeout(check, 50);
    };
    check();
  });

  return staticServer.url;
}

function shutdown() {
  stopStaticServer();
  stopDaemonProxy();
}

async function main() {
  console.log(`Avar desktop shell: ${SHELL_TINY}`);

  const Tiny = loadTiny();

  startDaemonProxy();

  const guiUrl = await resolveShellGuiUrl();
  console.log(`Loading GUI: ${guiUrl}`);

  const window = new Tiny();
  window.setSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
  window.setTitle(APP_TITLE);
  window.navigate(guiUrl);

  process.on("SIGINT", () => {
    shutdown();
    window.destroy();
    process.exit(0);
  });

  process.on("SIGTERM", () => {
    shutdown();
    window.destroy();
    process.exit(0);
  });

  window.run();
  shutdown();
}

void main();
