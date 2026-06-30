/**
 * Resolve shared CommonJS modules copied into the Electrobun app bundle.
 */
import { createRequire } from "node:module";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const require = createRequire(import.meta.url);
const bunDir = dirname(fileURLToPath(import.meta.url));

function resolveAppRoot(): string {
  const candidates = [
    join(bunDir, ".."),
    join(bunDir, "..", "..", ".."),
    join(bunDir, "..", "..", "..", ".."),
  ];

  for (const candidate of candidates) {
    if (existsSync(join(candidate, "electron", "extension-bridge.cjs"))) {
      return candidate;
    }
    if (existsSync(join(candidate, "..", "electron", "extension-bridge.cjs"))) {
      return join(candidate, "..");
    }
  }

  throw new Error(
    "Could not locate bundled desktop modules (electron/extension-bridge.cjs).",
  );
}

const appRoot = resolveAppRoot();

export function loadExtensionBridge(): typeof import("../../../electron/extension-bridge.cjs") {
  return require(join(appRoot, "electron", "extension-bridge.cjs"));
}

export function loadAvarProtocol(): typeof import("../../../electron/avar-protocol.cjs") {
  return require(join(appRoot, "electron", "avar-protocol.cjs"));
}

export function loadResolveGuiUrl(): typeof import("../../../desktop/resolve-gui-url.cjs") {
  const bundled = join(appRoot, "desktop", "resolve-gui-url.cjs");
  if (existsSync(bundled)) {
    return require(bundled);
  }
  return require(join(appRoot, "..", "desktop", "resolve-gui-url.cjs"));
}

export { appRoot };
