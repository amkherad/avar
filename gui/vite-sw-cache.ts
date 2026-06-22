import fs from "node:fs";
import path from "node:path";
import type { Plugin } from "vite";

export function serviceWorkerCacheVersionPlugin(buildVersion: string): Plugin {
  return {
    name: "avar-sw-cache-version",
    apply: "build",
    closeBundle() {
      const outDir = path.resolve(process.cwd(), "dist");
      const swPath = path.join(outDir, "sw.js");
      if (!fs.existsSync(swPath)) {
        return;
      }

      const content = fs
        .readFileSync(swPath, "utf8")
        .replaceAll("__CACHE_VERSION__", buildVersion);
      fs.writeFileSync(swPath, content);
    },
  };
}
