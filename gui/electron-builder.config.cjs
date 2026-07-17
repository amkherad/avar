"use strict";

const fs = require("node:fs");
const path = require("node:path");

/**
 * electron-builder configuration — optimized for minimal distributable size.
 *
 * Renderer dependencies are bundled into dist/ by Vite; keep package.json
 * `dependencies` empty so electron-builder does not ship node_modules.
 *
 * When `gui/build/bundle/avar/` exists (via scripts/stage_desktop_bundle.py),
 * the avar backend is copied into resources/avar/ for the desktop shell.
 *
 * @see https://www.electron.build/configuration
 * @type {import("electron-builder").Configuration}
 */
const bundleAvarDir = path.join(__dirname, "build", "bundle", "avar");
const hasBundledBackend = fs.existsSync(bundleAvarDir);

module.exports = {
  appId: "io.avar.gui",
  productName: "Avar",
  copyright: "Copyright © Ali Kherad",
  artifactName: "${productName}-${version}-${os}-${arch}.${ext}",

  directories: {
    output: "release",
    buildResources: "build",
  },

  files: [
    "dist/**/*",
    "electron/**/*",
    "desktop/env.cjs",
    "desktop/daemon-proxy.cjs",
    "desktop/resolve-gui-url.cjs",
    "desktop/bundled-daemon.cjs",
    "icon.svg",
    "public/icon-128.png",
    "package.json",
  ],

  extraResources: hasBundledBackend
    ? [
        {
          from: bundleAvarDir,
          to: "avar",
          filter: ["**/*"],
        },
      ]
    : [],

  asar: true,
  asarUnpack: ["electron/preload.cjs"],
  npmRebuild: false,
  buildDependenciesFromSource: false,
  removePackageScripts: true,

  // Chromium ships ~50 locale packs; the GUI uses its own i18n files in dist/.
  electronLanguages: ["en-US"],

  // Installer/archive compression (slightly slower builds, smaller artifacts).
  compression: "maximum",

  protocols: [
    {
      name: "Avar",
      schemes: ["avar"],
    },
  ],

  mac: {
    category: "public.app-category.utilities",
    target: ["dmg"],
    icon: "build/icon.png",
    artifactName: "${productName}-${version}-${os}-${arch}.${ext}",
  },

  win: {
    target: ["nsis", "portable"],
    icon: "build/icon.png",
  },

  nsis: {
    oneClick: false,
    allowToChangeInstallationDirectory: true,
    deleteAppDataOnUninstall: false,
    artifactName: "${productName}-${version}-${os}-${arch}-setup.${ext}",
  },

  portable: {
    artifactName: "${productName}-${version}-${os}-${arch}-portable.${ext}",
  },

  linux: {
    maintainer: "Ali Kherad <alimousavikherad@gmail.com>",
    target: ["deb"],
    icon: "build/icon.png",
    category: "Network;FileTransfer",
    synopsis: "Avar Download Manager",
    description: "Cross-platform download manager with segmented transfers, queues, and browser integration.",
    artifactName: "${productName}-${version}-${os}-${arch}.${ext}",
  },
};
