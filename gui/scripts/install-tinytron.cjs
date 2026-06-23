/**
 * Install tinytron and fail if the native addon was not built.
 *
 * tinytron@1.0.3 bundles node-gyp@7, which breaks on Node 22+. We skip its
 * install script and rebuild with the gui's node-gyp instead.
 */

const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const guiRoot = path.join(__dirname, "..");
const tinytronVersion = "1.0.3";
const tinytronDir = path.join(guiRoot, "node_modules", "tinytron");
const addonPath = path.join(tinytronDir, "build", "Release", "addon.node");

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: guiRoot,
    stdio: "inherit",
    env: process.env,
    shell: process.platform === "win32",
    ...options,
  });
  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }
}

function resolveNodeGyp() {
  try {
    return require.resolve("node-gyp/bin/node-gyp.js", { paths: [guiRoot] });
  } catch {
    console.error("node-gyp is required to build tinytron. Run npm ci in gui/ first.");
    process.exit(1);
  }
}

run("npm", [
  "install",
  `tinytron@${tinytronVersion}`,
  "--no-save",
  "--ignore-scripts",
]);

const nodeGyp = resolveNodeGyp();
run(process.execPath, [nodeGyp, "rebuild"], { cwd: tinytronDir, shell: false });

if (!fs.existsSync(addonPath)) {
  console.error(`tinytron native addon missing after rebuild: ${addonPath}`);
  process.exit(1);
}

console.log(`tinytron ready: ${addonPath}`);
