/**
 * Build the vendored tinytron native addon from gui/third_party/tiny.
 *
 * On Windows, fetches the WebView2 SDK headers first (see fetch-webview2.cjs).
 * Uses the gui's node-gyp (not tinytron's bundled copy) for Node 22+ compatibility.
 */

const { spawnSync } = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");

const guiRoot = path.join(__dirname, "..");
const tinytronSourceDir = path.join(guiRoot, "third_party", "tiny");
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

function copyTinytronSource() {
  if (!fs.existsSync(path.join(tinytronSourceDir, "package.json"))) {
    console.error(`Vendored tinytron not found: ${tinytronSourceDir}`);
    process.exit(1);
  }

  fs.mkdirSync(path.join(guiRoot, "node_modules"), { recursive: true });
  if (fs.existsSync(tinytronDir)) {
    fs.rmSync(tinytronDir, { recursive: true, force: true });
  }

  fs.cpSync(tinytronSourceDir, tinytronDir, {
    recursive: true,
    filter: (src) => {
      const base = path.basename(src);
      return base !== "node_modules" && base !== ".git";
    },
  });
}

if (process.platform === "win32") {
  run(process.execPath, [path.join(__dirname, "fetch-webview2.cjs")], {
    shell: false,
  });
}

copyTinytronSource();

run("npm", ["install", "--no-save", "--ignore-scripts"], {
  cwd: tinytronDir,
  shell: process.platform === "win32",
});

const nodeGyp = resolveNodeGyp();
run(process.execPath, [nodeGyp, "rebuild"], { cwd: tinytronDir, shell: false });

if (!fs.existsSync(addonPath)) {
  console.error(`tinytron native addon missing after rebuild: ${addonPath}`);
  process.exit(1);
}

console.log(`tinytron ready: ${addonPath}`);
