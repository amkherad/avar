import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { execSync } from "node:child_process";
import type { Plugin, Connect } from "vite";

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const extensionsRoot = path.join(repoRoot, "extensions");

const MIME: Record<string, string> = {
  ".js": "application/javascript",
  ".json": "application/json",
  ".html": "text/html",
  ".svg": "image/svg+xml",
  ".zip": "application/zip",
  ".md": "text/markdown",
};

function syncSharedAssets(): void {
  const sharedDir = path.join(extensionsRoot, "shared");
  for (const file of ["media.js", "protocol.js", "capture.js", "hls.js", "context-menu.js", "download-intercept.js", "badge.js", "popup.js", "popup.html", "page-response-hook.js", "media-hook.js", "content.js"]) {
    const source = path.join(sharedDir, file);
    if (!fs.existsSync(source)) {
      continue;
    }
    for (const target of ["chromium", "firefox"]) {
      fs.copyFileSync(source, path.join(extensionsRoot, target, file));
    }
  }
}

function packageExtensions(): void {
  syncSharedAssets();
  const packagesDir = path.join(extensionsRoot, "packages");
  fs.mkdirSync(packagesDir, { recursive: true });

  for (const variant of ["chromium", "firefox"]) {
    const sourceDir = path.join(extensionsRoot, variant);
    const zipPath = path.join(packagesDir, `avar-${variant}.zip`);
    try {
      if (process.platform === "win32") {
        const escapedSource = sourceDir.replace(/'/g, "''");
        const escapedZip = zipPath.replace(/'/g, "''");
        execSync(
          `powershell.exe -NoProfile -Command "Compress-Archive -Path '${escapedSource}\\*' -DestinationPath '${escapedZip}' -Force"`,
          { stdio: "inherit" },
        );
      } else {
        execSync(`cd "${sourceDir}" && zip -qr "${zipPath}" .`, { stdio: "inherit" });
      }
    } catch {
      // Unpacked folders remain available under /extensions/{variant}/ when zipping fails.
    }
  }
}

function serveExtensions(
  req: Connect.IncomingMessage,
  res: Connect.ServerResponse,
  next: Connect.NextFunction,
): void {
  const urlPath = decodeURIComponent((req.url ?? "/").split("?")[0]);
  const rel = urlPath.replace(/^\/+/, "");
  const filePath = path.normalize(path.join(extensionsRoot, rel));

  if (!filePath.startsWith(extensionsRoot) || !fs.existsSync(filePath)) {
    next();
    return;
  }

  const stat = fs.statSync(filePath);
  if (stat.isDirectory()) {
    next();
    return;
  }

  const ext = path.extname(filePath);
  res.setHeader("Content-Type", MIME[ext] ?? "application/octet-stream");
  fs.createReadStream(filePath).pipe(res);
}

export function extensionsPlugin(): Plugin {
  return {
    name: "avar-extensions",
    configureServer(server) {
      server.middlewares.use("/extensions", serveExtensions);
    },
    closeBundle() {
      syncSharedAssets();
      packageExtensions();
      const dest = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "dist/extensions");
      fs.cpSync(extensionsRoot, dest, { recursive: true });
    },
  };
}

export function prepareExtensions(): void {
  syncSharedAssets();
  packageExtensions();
}
