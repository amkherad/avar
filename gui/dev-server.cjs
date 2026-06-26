"use strict";

/**
 * Vite / Electron development server port (web + desktop).
 * Keep outside Windows Hyper-V excluded ranges (often ~56417–57216 on Win10/11).
 */
const DEV_SERVER_PORT = 56000;

module.exports = {
  DEV_SERVER_PORT,
  DEV_SERVER_URL: `http://localhost:${DEV_SERVER_PORT}`,
  EXTENSION_GUI_URL: `http://127.0.0.1:${DEV_SERVER_PORT}`,
};
