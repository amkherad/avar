const { nativeImage } = require("electron");

/** @type {Record<string, string>} */
const TRAY_MENU_SVGS = {
  show:
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="2" y="3" width="12" height="9" rx="1" fill="none" stroke="#111" stroke-width="1.5"/><path d="M5 13h6" stroke="#111" stroke-width="1.5" stroke-linecap="round"/></svg>',
  play: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4 2.5v11l9-5.5z" fill="#111"/></svg>',
  pause:
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="4" y="3" width="3" height="10" rx="0.5" fill="#111"/><rect x="9" y="3" width="3" height="10" rx="0.5" fill="#111"/></svg>',
  resume:
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M4 2.5v11l9-5.5z" fill="#111"/></svg>',
  stop: '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><rect x="4" y="4" width="8" height="8" rx="1" fill="#111"/></svg>',
  exit:
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M3 8h7M8 5l3 3-3 3" fill="none" stroke="#111" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/><path d="M12 3v10" stroke="#111" stroke-width="1.5" stroke-linecap="round"/></svg>',
  download:
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16"><path d="M8 2v7M5 7l3 3 3-3" fill="none" stroke="#111" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/><path d="M3 12h10" stroke="#111" stroke-width="1.5" stroke-linecap="round"/></svg>',
};

/**
 * @param {string} key
 * @returns {import('electron').NativeImage | undefined}
 */
function trayMenuIcon(key) {
  const svg = TRAY_MENU_SVGS[key];
  if (!svg) {
    return undefined;
  }

  const image = nativeImage.createFromDataURL(
    `data:image/svg+xml;base64,${Buffer.from(svg).toString("base64")}`,
  );
  if (image.isEmpty()) {
    return undefined;
  }

  return image.resize({ width: 16, height: 16 });
}

module.exports = { trayMenuIcon };
