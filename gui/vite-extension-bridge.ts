import type { Plugin, Connect } from "vite";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const {
  handleExtensionBridgeRequest,
  setExtensionBridgeEnabled,
  setExtensionBridgeConfig,
} = require("./electron/extension-bridge.cjs");

export function extensionBridgePlugin(): Plugin {
  return {
    name: "avar-extension-bridge",
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        void handleExtensionBridgeRequest(req, res).then((handled) => {
          if (!handled) {
            next();
          }
        });
      });
    },
  };
}

export { setExtensionBridgeEnabled, setExtensionBridgeConfig };
