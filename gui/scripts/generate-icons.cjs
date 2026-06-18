/**
 * Generate PNG icons from gui/icon.svg for extensions, tray, and electron-builder.
 * Run: node scripts/generate-icons.cjs
 */
const fs = require("node:fs");
const path = require("node:path");
const { execSync } = require("node:child_process");

const guiRoot = path.join(__dirname, "..");
const svgPath = path.join(guiRoot, "icon.svg");

const targets = [
  { dir: path.join(guiRoot, "public"), sizes: [16, 48, 128, 256] },
  { dir: path.join(guiRoot, "electron"), sizes: [16, 32] },
  { dir: path.join(guiRoot, "..", "extensions", "chromium", "icons"), sizes: [16, 48, 128] },
  { dir: path.join(guiRoot, "..", "extensions", "firefox", "icons"), sizes: [16, 48, 128] },
];

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function generateWithSharp(size, outPath) {
  const sharp = require("sharp");
  return sharp(svgPath).resize(size, size).png().toFile(outPath);
}

async function main() {
  if (!fs.existsSync(svgPath)) {
    throw new Error(`Missing ${svgPath}`);
  }

  let sharp;
  try {
    sharp = require("sharp");
  } catch {
    console.log("Installing sharp for icon generation…");
    execSync("npm install --no-save sharp", { cwd: guiRoot, stdio: "inherit" });
    sharp = require("sharp");
  }

  for (const target of targets) {
    ensureDir(target.dir);
    for (const size of target.sizes) {
      const filename = size === 256 ? "icon-256.png" : `icon-${size}.png`;
      const outPath = path.join(target.dir, filename);
      await generateWithSharp(size, outPath);
      console.log(`Wrote ${outPath}`);
    }
  }

  const builderIcon = path.join(guiRoot, "build", "icon.png");
  ensureDir(path.dirname(builderIcon));
  await generateWithSharp(512, builderIcon);
  console.log(`Wrote ${builderIcon}`);
}

void main().catch((error) => {
  console.error(error);
  process.exit(1);
});
