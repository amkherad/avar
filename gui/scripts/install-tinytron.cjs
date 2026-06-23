/**
 * Install tinytron and fail if the native addon was not built.
 * Kept separate from package.json optionalDependencies so npm does not
 * swallow native build failures.
 */

const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const guiRoot = path.join(__dirname, "..");
const tinytronVersion = "1.0.3";
const addonPath = path.join(
  guiRoot,
  "node_modules",
  "tinytron",
  "build",
  "Release",
  "addon.node",
);

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: guiRoot,
    stdio: "inherit",
    env: process.env,
    shell: process.platform === "win32",
  });
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

run("npm", [
  "install",
  `tinytron@${tinytronVersion}`,
  "--no-save",
  "--foreground-scripts",
]);

if (!fs.existsSync(addonPath)) {
  console.error(`tinytron native addon missing after install: ${addonPath}`);
  process.exit(1);
}

console.log(`tinytron ready: ${addonPath}`);
