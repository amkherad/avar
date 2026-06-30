"use strict";

const { spawnSync } = require("node:child_process");
const path = require("node:path");

const electrobunRoot = __dirname;
const guiRoot = path.join(electrobunRoot, "..");
const electrobunCli = path.join(guiRoot, "node_modules", "electrobun", "bin", "electrobun.cjs");

const args = ["build", "--env=stable", ...process.argv.slice(2)];
const result = spawnSync(process.execPath, [electrobunCli, ...args], {
  cwd: electrobunRoot,
  stdio: "inherit",
  env: { ...process.env, AVAR_ELECTROBUN_COPY_DIST: "1" },
});

process.exit(result.status ?? 1);
