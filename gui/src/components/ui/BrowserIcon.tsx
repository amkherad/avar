import type { BrowserId } from "@/lib/browserExtensionInstall";

export interface BrowserIconProps {
  browser: BrowserId;
  className?: string;
}

const BROWSER_LOGOS: Record<BrowserId, string> = {
  chrome: "chrome_128x128.png",
  firefox: "firefox_128x128.png",
  edge: "edge_128x128.png",
  opera: "opera_128x128.png",
};

export function BrowserIcon({ browser, className }: BrowserIconProps) {
  const base = import.meta.env.BASE_URL.replace(/\/?$/, "/");
  const src = `${base}browser_logos/${BROWSER_LOGOS[browser]}`;

  return <img className={className} src={src} alt="" aria-hidden="true" />;
}
