"use strict";

/**
 * electron-builder configuration — optimized for minimal distributable size.
 *
 * Renderer dependencies are bundled into dist/ by Vite; keep package.json
 * `dependencies` empty so electron-builder does not ship node_modules.
 *
 * @see https://www.electron.build/configuration
 * @type {import("electron-builder").Configuration}
 */
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
    "icon.svg",
    "public/icon-128.png",
    "package.json",
  ],

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
    target: ["dmg", "zip"],
    icon: "build/icon.png",
  },

  win: {
    target: ["nsis", "portable"],
    icon: "build/icon.png",
  },

  nsis: {
    oneClick: false,
    allowToChangeInstallationDirectory: true,
    deleteAppDataOnUninstall: false,
  },

  linux: {
    maintainer: "Ali Kherad <alimousavikherad@gmail.com>",
    target: ["AppImage", "deb"],
    icon: "build/icon.png",
  },
};
