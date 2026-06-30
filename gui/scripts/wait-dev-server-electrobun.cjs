"use strict";

const { spawn } = require("node:child_process");
const path = require("node:path");
const waitOn = require("wait-on");
const { DEV_SERVER_URL } = require("../dev-server.cjs");

const guiRoot = path.join(__dirname, "..");
const electrobunRoot = path.join(guiRoot, "electrobun");
const electrobunCli = path.join(guiRoot, "node_modules", "electrobun", "bin", "electrobun.cjs");

waitOn({ resources: [DEV_SERVER_URL], timeout: 120000 })
  .then(() => {
    const env = { ...process.env, VITE_DEV_SERVER_URL: DEV_SERVER_URL };
    const child = spawn(process.execPath, [electrobunCli, "dev"], {
      cwd: electrobunRoot,
      env,
      stdio: "inherit",
    });

    child.on("exit", (code, signal) => {
      if (signal) {
        process.kill(process.pid, signal);
        return;
      }
      process.exit(code ?? 0);
    });
  })
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
