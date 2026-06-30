import type { ElectrobunConfig } from "electrobun";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";

const electrobunRoot = import.meta.dirname;
const guiRoot = join(electrobunRoot, "..");
const distDir = join(guiRoot, "dist");
const packageJson = JSON.parse(
  readFileSync(join(guiRoot, "package.json"), "utf8"),
) as { version: string };

function collectDistCopy(
  distRoot: string,
  currentDir: string,
  copy: Record<string, string>,
): void {
  for (const entry of readdirSync(currentDir)) {
    const absolutePath = join(currentDir, entry);
    const relativeToDist = relative(distRoot, absolutePath).replaceAll("\\", "/");
    const sourcePath = `../dist/${relativeToDist}`;
    const targetPath = `views/mainview/${relativeToDist}`;

    if (statSync(absolutePath).isDirectory()) {
      collectDistCopy(distRoot, absolutePath, copy);
      continue;
    }

    copy[sourcePath] = targetPath;
  }
}

function buildDistCopy(): Record<string, string> {
  if (process.env.AVAR_ELECTROBUN_COPY_DIST !== "1" || !existsSync(distDir)) {
    return {};
  }

  const copy: Record<string, string> = {};
  collectDistCopy(distDir, distDir, copy);
  return copy;
}

function buildVendorCopy(): Record<string, string> {
  return {
    "../electron/extension-bridge.cjs": "electron/extension-bridge.cjs",
    "../electron/extension-protocol.cjs": "electron/extension-protocol.cjs",
    "../electron/avar-protocol.cjs": "electron/avar-protocol.cjs",
    "../dev-server.cjs": "dev-server.cjs",
    "../desktop/resolve-gui-url.cjs": "desktop/resolve-gui-url.cjs",
    "../desktop/env.cjs": "desktop/env.cjs",
  };
}

export default {
  app: {
    name: "Avar",
    identifier: "io.avar.gui.electrobun",
    version: packageJson.version,
    urlSchemes: ["avar"],
  },
  runtime: {
    exitOnLastWindowClosed: false,
  },
  build: {
    bun: {
      entrypoint: "src/bun/index.ts",
      packages: "external",
    },
    views: {
      avarbridge: {
        entrypoint: "src/views/avar-preload.ts",
      },
    },
    copy: {
      ...buildVendorCopy(),
      ...buildDistCopy(),
    },
    watch: [
      "../electron/extension-bridge.cjs",
      "../electron/extension-protocol.cjs",
      "../electron/avar-protocol.cjs",
      "../dev-server.cjs",
      "../desktop",
    ],
    mac: {
      bundleCEF: false,
    },
    linux: {
      bundleCEF: false,
    },
    win: {
      bundleCEF: false,
    },
  },
} satisfies ElectrobunConfig;
