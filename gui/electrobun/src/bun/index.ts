import { BrowserWindow, Updater, Utils } from "electrobun/bun";
import {
  APP_TITLE,
  DEFAULT_WINDOW_HEIGHT,
  DEFAULT_WINDOW_WIDTH,
  resolveShellGuiUrl,
  startDaemonProxy,
  stopDaemonProxy,
} from "./desktop-shell";

const SHELL_ELECTROBUN = "electrobun";

function shutdown(): void {
  stopDaemonProxy();
}

console.log(`Avar desktop shell: ${SHELL_ELECTROBUN}`);

startDaemonProxy();

let channel = "dev";
try {
  channel = await Updater.localInfo.channel();
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  console.warn(`Could not read Electrobun channel (${message}); assuming dev.`);
}

const guiUrl = await resolveShellGuiUrl(channel);
console.log(`Loading GUI: ${guiUrl}`);

const mainWindow = new BrowserWindow({
  title: APP_TITLE,
  url: guiUrl,
  frame: {
    width: DEFAULT_WINDOW_WIDTH,
    height: DEFAULT_WINDOW_HEIGHT,
    x: 100,
    y: 100,
  },
});

mainWindow.on("close", () => {
  shutdown();
  Utils.quit();
});
