/**
 * Desktop host metadata exposed to the GUI About page.
 */

const fs = require("node:fs");
const path = require("node:path");
const {
  SHELL_ELECTRON,
  SHELL_ELECTROBUN,
  SHELL_TINY,
  SHELLS,
} = require("./shells.cjs");

const guiRoot = path.join(__dirname, "..");

function readPackageVersion(relativePath) {
  try {
    const packagePath = path.join(guiRoot, relativePath);
    const raw = fs.readFileSync(packagePath, "utf8");
    const parsed = JSON.parse(raw);
    return typeof parsed.version === "string" ? parsed.version : null;
  } catch {
    return null;
  }
}

function compactVersions(entries) {
  /** @type {Record<string, string>} */
  const versions = {};
  for (const [key, value] of entries) {
    if (typeof value === "string" && value.trim() !== "") {
      versions[key] = value;
    }
  }
  return versions;
}

function electronHostInfo() {
  return {
    shell: SHELL_ELECTRON,
    shellLabel: SHELLS[SHELL_ELECTRON].label,
    platform: process.platform,
    versions: compactVersions([
      ["electron", process.versions.electron],
      ["chromium", process.versions.chrome],
      ["node", process.versions.node],
    ]),
  };
}

function tinyHostInfo() {
  return {
    shell: SHELL_TINY,
    shellLabel: SHELLS[SHELL_TINY].label,
    platform: process.platform,
    versions: compactVersions([
      ["tinytron", readPackageVersion("third_party/tiny/package.json")],
      ["node", process.versions.node],
    ]),
  };
}

function electrobunHostInfo(runtime = {}) {
  const bunVersion =
    typeof runtime.bunVersion === "string"
      ? runtime.bunVersion
      : typeof process.versions?.bun === "string"
        ? process.versions.bun
        : null;

  return {
    shell: SHELL_ELECTROBUN,
    shellLabel: SHELLS[SHELL_ELECTROBUN].label,
    platform: process.platform,
    versions: compactVersions([
      ["electrobun", readPackageVersion("node_modules/electrobun/package.json")],
      ["bun", bunVersion],
    ]),
  };
}

module.exports = {
  electronHostInfo,
  tinyHostInfo,
  electrobunHostInfo,
};
