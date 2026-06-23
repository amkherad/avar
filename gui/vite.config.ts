import { createRequire } from "node:module";
import { defineConfig, loadEnv } from "vite";
import react from "@vitejs/plugin-react";
import fs from "node:fs";
import path from "node:path";
import { extensionsPlugin } from "./vite-extensions";
import { extensionBridgePlugin } from "./vite-extension-bridge";
import { serviceWorkerCacheVersionPlugin } from "./vite-sw-cache";

const require = createRequire(import.meta.url);
const { DEV_SERVER_PORT } = require("./dev-server.cjs") as { DEV_SERVER_PORT: number };

function loadAvarBuildEnv(root: string): Record<string, string> {
  const envPath = path.join(root, ".avar-build.env");
  if (!fs.existsSync(envPath)) {
    return {};
  }
  const vars: Record<string, string> = {};
  for (const line of fs.readFileSync(envPath, "utf8").split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) {
      continue;
    }
    const eq = trimmed.indexOf("=");
    if (eq <= 0) {
      continue;
    }
    vars[trimmed.slice(0, eq)] = trimmed.slice(eq + 1);
  }
  return vars;
}

export default defineConfig(({ mode }) => {
  const env = { ...loadEnv(mode, process.cwd(), ""), ...loadAvarBuildEnv(process.cwd()) };
  const buildDate = env.VITE_BUILD_DATE || new Date().toISOString();
  const buildVersion =
    env.VITE_BUILD_VERSION || process.env.npm_package_version || "dev";
  const buildCommit = env.VITE_BUILD_COMMIT?.slice(0, 7) ?? "";
  const cacheVersion = buildCommit ? `${buildVersion}-${buildCommit}` : buildVersion;

  return {
    plugins: [
      react(),
      extensionBridgePlugin(),
      extensionsPlugin(),
      serviceWorkerCacheVersionPlugin(cacheVersion),
    ],
    resolve: {
      alias: {
        "@": path.resolve(__dirname, "src"),
      },
    },
    base: "./",
    define: {
      "import.meta.env.VITE_BUILD_VERSION": JSON.stringify(
        env.VITE_BUILD_VERSION || process.env.npm_package_version || "dev",
      ),
      "import.meta.env.VITE_BUILD_DATE": JSON.stringify(buildDate),
      "import.meta.env.VITE_BUILD_COMMIT": JSON.stringify(env.VITE_BUILD_COMMIT || ""),
      "import.meta.env.VITE_GITHUB_REPO": JSON.stringify(
        env.VITE_GITHUB_REPO || "https://github.com/amkherad/avar",
      ),
      "import.meta.env.VITE_GITHUB_AUTHOR": JSON.stringify(
        env.VITE_GITHUB_AUTHOR || "https://github.com/amkherad",
      ),
      "import.meta.env.VITE_GITHUB_SPONSORS": JSON.stringify(
        env.VITE_GITHUB_SPONSORS || "https://github.com/sponsors/amkherad",
      ),
    },
    server: {
      port: DEV_SERVER_PORT,
      strictPort: true,
      proxy: {
        "/api": {
          target: "http://127.0.0.1:8000",
          changeOrigin: true,
          ws: true,
        },
      },
    },
    preview: {
      proxy: {
        "/api": {
          target: "http://127.0.0.1:8000",
          changeOrigin: true,
          ws: true,
        },
      },
    },
  };
});
