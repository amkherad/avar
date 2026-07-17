/**
 * Locate and start the avar daemon bundled next to the Electron desktop app.
 */

const { spawn } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");
const { app } = require("electron");

/** @type {import("node:child_process").ChildProcess | null} */
let daemonChild = null;

function bundledAvarPath() {
  if (!app.isPackaged) {
    return null;
  }

  const fileName = process.platform === "win32" ? "avar.exe" : "avar";
  const candidate = path.join(process.resourcesPath, "avar", fileName);
  return fs.existsSync(candidate) ? candidate : null;
}

function parseDaemonTarget(target) {
  try {
    const url = new URL(target);
    return {
      port: url.port ? Number(url.port) : 8000,
    };
  } catch {
    return { port: 8000 };
  }
}

async function isDaemonReachable(target) {
  try {
    const response = await fetch(`${target.replace(/\/+$/, "")}/api/ping`);
    return response.ok;
  } catch {
    return false;
  }
}

function waitForDaemon(target, attempts = 40, intervalMs = 250) {
  return new Promise((resolve) => {
    let remaining = attempts;

    const tick = async () => {
      if (await isDaemonReachable(target)) {
        resolve(true);
        return;
      }

      remaining -= 1;
      if (remaining <= 0) {
        resolve(false);
        return;
      }

      setTimeout(() => {
        void tick();
      }, intervalMs);
    };

    void tick();
  });
}

async function ensureBundledDaemon(daemonTarget) {
  const avarPath = bundledAvarPath();
  if (!avarPath) {
    return true;
  }

  if (await isDaemonReachable(daemonTarget)) {
    return true;
  }

  const { port } = parseDaemonTarget(daemonTarget);
  const args = ["daemon", "start", "--http", "--port", String(port), "--attached", "--no-detach"];

  daemonChild = spawn(avarPath, args, {
    detached: false,
    stdio: "ignore",
    windowsHide: true,
  });

  daemonChild.on("error", () => {});
  return waitForDaemon(daemonTarget);
}

function stopBundledDaemon() {
  if (!daemonChild) {
    return;
  }

  const child = daemonChild;
  daemonChild = null;

  try {
    child.kill();
  } catch {
    // ignore shutdown errors
  }
}

module.exports = {
  bundledAvarPath,
  ensureBundledDaemon,
  stopBundledDaemon,
};
