"use strict";

/**
 * Shared dev-server and extension-bridge ports for Node desktop shells.
 * Must match gui/src/lib/browserExtensionUrls.ts.
 */

const DEV_SERVER_PORT = 56000;

module.exports = {
  DEV_SERVER_PORT,
  DEV_SERVER_URL: `http://localhost:${DEV_SERVER_PORT}`,
  EXTENSION_GUI_URL: `http://127.0.0.1:${DEV_SERVER_PORT}`,
};
