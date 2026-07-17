/**
 * Package a standalone Tiny desktop executable with embedded GUI assets.
 *
 * Uses @yao-pkg/pkg with Node 22 runtime binaries (published on pkg-fetch v3.6).
 * Node 18/20 targets are not shipped in the remote cache and would require
 * building from source plus the `patch` utility on Windows.
 */

const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");

const guiRoot = path.join(__dirname, "..");
const launcherPath = path.join(guiRoot, "tiny", "launcher.cjs");
const distDir = path.join(guiRoot, "dist");
const addonPath = path.join(
  guiRoot,
  "node_modules",
  "tinytron",
  "build",
  "Release",
  "addon.node",
);
const outputDir = path.join(guiRoot, "release", "tiny");
const tinytronDir = path.join(guiRoot, "node_modules", "tinytron");

/** pkg-fetch v3.6 publishes 22.x / 24.x / 26.x — not 18.x or 20.x. */
const PKG_NODE_MAJOR = "22";
const PKG_NODE_VERSION = "22.23.1";

function run(command, args, options = {}) {
  const useShell =
    options.shell ?? (process.platform === "win32" && command.endsWith(".cmd"));
  const result = spawnSync(command, args, {
    cwd: guiRoot,
    stdio: "inherit",
    env: options.env ?? process.env,
    shell: useShell,
    ...options,
    shell: useShell,
  });
  if (result.error) {
    console.error(result.error.message);
    process.exit(1);
  }
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function resolveNodeGyp() {
  return require.resolve("node-gyp/bin/node-gyp.js", { paths: [guiRoot] });
}

function rebuildTinytronForPkg() {
  const nodeGyp = resolveNodeGyp();
  console.log(`Rebuilding tinytron for pkg Node ${PKG_NODE_VERSION}...`);
  run(
    process.execPath,
    [
      nodeGyp,
      "rebuild",
      `--target=${PKG_NODE_VERSION}`,
      "--dist-url=https://nodejs.org/dist",
    ],
    { cwd: tinytronDir, shell: false },
  );
}

function resolvePkgEntry() {
  return require.resolve("@yao-pkg/pkg/lib-es5/bin.js");
}

function pkgTarget() {
  if (process.platform === "win32") {
    return `node${PKG_NODE_MAJOR}-win-x64`;
  }
  if (process.platform === "darwin") {
    return `node${PKG_NODE_MAJOR}-macos-x64`;
  }
  return `node${PKG_NODE_MAJOR}-linux-x64`;
}

function findWindowsPatchDir() {
  const candidates = [
    process.env.PATCH_HOME,
    "C:\\Program Files\\Git\\usr\\bin",
    "C:\\Program Files (x86)\\Git\\usr\\bin",
  ].filter(Boolean);

  for (const dir of candidates) {
    const patchExe = path.join(dir, "patch.exe");
    if (fs.existsSync(patchExe)) {
      return dir;
    }
  }
  return null;
}

function packagingEnv() {
  const env = { ...process.env };
  const patchDir = process.platform === "win32" ? findWindowsPatchDir() : null;
  if (patchDir) {
    env.PATH = `${patchDir}${path.delimiter}${env.PATH ?? ""}`;
  }
  return env;
}

async function prefetchPkgRuntime(target) {
  const pkgFetch = require("@yao-pkg/pkg-fetch");
  const match = /^node(\d+)-(win|macos|linux)-(?:x64|arm64)$/i.exec(target);
  if (!match) {
    throw new Error(`Unsupported pkg target: ${target}`);
  }

  const [, major, platform] = match;
  const nodeRange = `node${major}`;
  const fancyPlatform = platform === "win" ? "win" : platform;

  console.log(`Prefetching pkg runtime ${target}...`);
  const runtimePath = await pkgFetch.need({
    nodeRange,
    platform: fancyPlatform,
    arch: "x64",
  });
  console.log(`pkg runtime ready: ${runtimePath}`);
}

function copyAddonBesideExe(outputPath) {
  const destination = path.join(path.dirname(outputPath), "addon.node");
  fs.copyFileSync(addonPath, destination);
  console.log(`Copied native addon: ${destination}`);
}

function restoreTinytronForDev() {
  run(process.execPath, [path.join(__dirname, "install-tinytron.cjs")], {
    shell: false,
  });
}

async function main() {
  if (!fs.existsSync(distDir)) {
    console.error("GUI dist/ missing. Run: npm run build");
    process.exit(1);
  }
  if (!fs.existsSync(addonPath)) {
    console.error("tinytron addon missing. Run: node scripts/install-tinytron.cjs");
    process.exit(1);
  }
  if (!fs.existsSync(launcherPath)) {
    console.error(`Launcher missing: ${launcherPath}`);
    process.exit(1);
  }

  fs.mkdirSync(outputDir, { recursive: true });

  const target = pkgTarget();
  const outputName =
    process.platform === "win32" ? "avar-tiny.exe" : "avar-tiny";
  const outputPath = path.join(outputDir, outputName);

  try {
    await prefetchPkgRuntime(target);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`Failed to prefetch pkg runtime (${target}): ${message}`);
    if (process.platform === "win32" && !findWindowsPatchDir()) {
      console.error(
        [
          "Install Git for Windows (includes patch.exe) or add patch to PATH",
          "if pkg must build Node from source.",
        ].join("\n"),
      );
    }
    process.exit(1);
  }

  rebuildTinytronForPkg();

  run(
    process.execPath,
    [
      resolvePkgEntry(),
      launcherPath,
      "--targets",
      target,
      "--output",
      outputPath,
      "--config",
      path.join(guiRoot, "tiny", "pkg.json"),
    ],
    { env: packagingEnv(), shell: false },
  );

  copyAddonBesideExe(outputPath);
  restoreTinytronForDev();

  console.log(`Created ${outputPath}`);
  console.log(`  pkg target: ${target}`);
  console.log(`  pkg cache: ${process.env.PKG_CACHE_PATH || path.join(os.homedir(), ".pkg-cache")}`);
}

void main().catch((error) => {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
});
