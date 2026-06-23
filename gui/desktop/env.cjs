/**
 * Shared desktop-shell environment (Electron, Tiny, future shells).
 */

const path = require("node:path");

const guiRoot = path.join(__dirname, "..");

module.exports = {
  guiRoot,
  distDir: path.join(guiRoot, "dist"),
  electronDir: path.join(guiRoot, "electron"),

  DAEMON_TARGET: process.env.AVAR_DAEMON_URL || "http://127.0.0.1:8000",
  BUNDLED_GUI_URL: process.env.AVAR_GUI_URL || "",
  IS_BUNDLED: process.env.AVAR_BUNDLED === "1",

  PROXY_HOST: "127.0.0.1",
  PROXY_PORT: Number(process.env.AVAR_ELECTRON_PROXY_PORT || 18765),

  VITE_DEV_SERVER_URL: process.env.VITE_DEV_SERVER_URL || "",

  DEFAULT_WINDOW_WIDTH: 1280,
  DEFAULT_WINDOW_HEIGHT: 800,
  DEFAULT_WINDOW_MIN_WIDTH: 1024,
  DEFAULT_WINDOW_MIN_HEIGHT: 640,

  APP_TITLE: "Avar",
};
