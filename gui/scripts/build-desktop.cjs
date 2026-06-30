"use strict";

/**
 * Build Electron desktop installers for one or more targets.
 * macOS packages require macOS; Windows and Linux can be cross-built per
 * https://www.electron.build/multi-platform-build
 *
 * Usage:
 *   node scripts/build-desktop.cjs            # all targets supported on this host
 *   node scripts/build-desktop.cjs mac        # macOS only
 *   node scripts/build-desktop.cjs win linux  # Windows + Linux
 */

const { spawnSync } = require("node:child_process");
const path = require("node:path");

const guiRoot = path.join(__dirname, "..");
const docsUrl = "https://www.electron.build/multi-platform-build";

/** @type {{ id: string; flag: string; hosts: NodeJS.Platform[]; label: string }[]} */
const DESKTOP_TARGETS = [
  { id: "mac", flag: "--mac", hosts: ["darwin"], label: "macOS" },
  { id: "win", flag: "--win", hosts: ["darwin", "linux", "win32"], label: "Windows" },
  { id: "linux", flag: "--linux", hosts: ["darwin", "linux", "win32"], label: "Linux" },
];

/** @type {Record<string, string>} */
const PLATFORM_ALIASES = {
  all: "all",
  mac: "mac",
  macos: "mac",
  darwin: "mac",
  win: "win",
  windows: "win",
  linux: "linux",
};

function resolveElectronBuilder() {
  const name = process.platform === "win32" ? "electron-builder.cmd" : "electron-builder";
  return path.join(guiRoot, "node_modules", ".bin", name);
}

function runElectronBuilder(args) {
  return spawnSync(resolveElectronBuilder(), args, {
    cwd: guiRoot,
    stdio: "inherit",
    env: process.env,
    shell: process.platform === "win32",
  });
}

function resolveTargetIds(argv) {
  if (argv.length === 0) {
    return DESKTOP_TARGETS.map((target) => target.id);
  }

  const ids = [];
  for (const arg of argv) {
    if (arg.startsWith("-")) {
      continue;
    }

    const id = PLATFORM_ALIASES[arg.toLowerCase()];
    if (!id) {
      console.error(`Unknown platform: ${arg}`);
      console.error(`Supported platforms: ${Object.keys(PLATFORM_ALIASES).join(", ")}`);
      process.exit(1);
    }

    if (id === "all") {
      return DESKTOP_TARGETS.map((target) => target.id);
    }

    if (!ids.includes(id)) {
      ids.push(id);
    }
  }

  return ids;
}

function electronBuilderArgs(argv) {
  return argv.filter((arg) => arg.startsWith("-"));
}

function targetById(id) {
  const target = DESKTOP_TARGETS.find((entry) => entry.id === id);
  if (!target) {
    throw new Error(`Unknown target id: ${id}`);
  }
  return target;
}

function main() {
  const argv = process.argv.slice(2);
  const requestedIds = resolveTargetIds(argv);
  const extraArgs = electronBuilderArgs(argv);
  const explicitRequest = argv.some((arg) => !arg.startsWith("-"));

  const selected = [];
  const skipped = [];

  for (const id of requestedIds) {
    const target = targetById(id);
    if (target.hosts.includes(process.platform)) {
      selected.push(target.flag);
      continue;
    }
    skipped.push(target.label);
  }

  if (skipped.length > 0) {
    console.log(
      `Skipping ${skipped.join(", ")} build — not available on ${process.platform}. See ${docsUrl}`,
    );
  }

  if (selected.length === 0) {
    if (explicitRequest) {
      return;
    }
    console.error("No desktop targets can be built on this platform.");
    process.exit(1);
  }

  const result = runElectronBuilder([...selected, ...extraArgs]);
  process.exit(result.status ?? 1);
}

main();
