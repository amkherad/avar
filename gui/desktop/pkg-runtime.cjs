/**
 * Helpers for running the GUI inside a @yao-pkg/pkg executable.
 */

const fs = require("node:fs");
const path = require("node:path");

function isPkg() {
  return Boolean(process.pkg);
}

function resolveGuiRoot() {
  if (isPkg()) {
    return path.join(__dirname, "..");
  }
  return path.join(__dirname, "..");
}

function resolveTinytronAddonPath() {
  if (!isPkg()) {
    return null;
  }

  const candidates = [
    path.join(path.dirname(process.execPath), "addon.node"),
    path.join(
      resolveGuiRoot(),
      "node_modules",
      "tinytron",
      "build",
      "Release",
      "addon.node",
    ),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return null;
}

module.exports = {
  isPkg,
  resolveGuiRoot,
  resolveTinytronAddonPath,
};
