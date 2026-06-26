/** Must match `gui/dev-server.cjs`. */
export const DEV_SERVER_PORT = 56000;

export const DEFAULT_EXTENSION_GUI_URL = `http://127.0.0.1:${DEV_SERVER_PORT}`;

export const ELECTRON_EXTENSION_BRIDGE_URL = "http://127.0.0.1:18766";

/** @deprecated Use ELECTRON_EXTENSION_BRIDGE_URL */

export const ELECTRON_EXTENSION_GUI_URL = ELECTRON_EXTENSION_BRIDGE_URL;

