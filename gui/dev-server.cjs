"use strict";

/**
 * Vite / Electron development server port (web + desktop).
 * Keep outside Windows Hyper-V excluded ranges (often ~56417–57216 on Win10/11).
 */
module.exports = require("./electron/ports.cjs");
