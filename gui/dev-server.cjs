"use strict";

/** Vite / Electron development server port (web + desktop). */
const DEV_SERVER_PORT = 56821;

module.exports = {
  DEV_SERVER_PORT,
  DEV_SERVER_URL: `http://localhost:${DEV_SERVER_PORT}`,
  EXTENSION_GUI_URL: `http://127.0.0.1:${DEV_SERVER_PORT}`,
};
