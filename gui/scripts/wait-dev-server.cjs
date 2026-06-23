"use strict";

const { spawn } = require("node:child_process");
const path = require("node:path");
const waitOn = require("wait-on");
const { DEV_SERVER_URL } = require("../dev-server.cjs");

const guiRoot = path.join(__dirname, "..");
const command = process.argv[2];
const args = process.argv.slice(3);

if (!command) {
  console.error("Usage: node wait-dev-server.cjs <command> [args...]");
  process.exit(1);
}

waitOn({ resources: [DEV_SERVER_URL], timeout: 120000 })
  .then(() => {
    const env = { ...process.env, VITE_DEV_SERVER_URL: DEV_SERVER_URL };
    const child = spawn(command, args, {
      cwd: guiRoot,
      env,
      stdio: "inherit",
      shell: true,
    });
    child.on("exit", (code, signal) => {
      if (signal) {
        process.kill(process.pid, signal);
        return;
      }
      process.exit(code ?? 0);
    });
  })
  .catch((err) => {
    console.error(err);
    process.exit(1);
  });
