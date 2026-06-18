import type { BrowserId } from "@/lib/browserExtensionInstall";

export interface BrowserIconProps {
  browser: BrowserId;
  className?: string;
}

export function BrowserIcon({ browser, className }: BrowserIconProps) {
  const shared = className ?? "";

  switch (browser) {
    case "firefox":
      return (
        <svg className={shared} viewBox="0 0 24 24" aria-hidden="true">
          <circle cx="12" cy="12" r="11" fill="#ff7139" />
          <path
            fill="#fff"
            d="M12 4.5c-3.2 0-5.8 2.1-6.7 5 1.4-.8 3-1.2 4.7-1.1 1.1.1 2.1.4 3 .9.8.5 1.5 1.1 2 1.9.6-.9.9-2 1-3.1.1-1.2 0-2.3-.3-3.4C14.8 5.2 13.5 4.5 12 4.5z"
          />
        </svg>
      );
    case "edge":
      return (
        <svg className={shared} viewBox="0 0 24 24" aria-hidden="true">
          <path fill="#0078d4" d="M20.5 8.2A7.8 7.8 0 0 0 12 3.5 8 8 0 0 0 4.2 9.5 6.5 6.5 0 0 0 3 13c0 3.6 2.9 6.5 6.5 6.5h8.8A4.7 4.7 0 0 0 23 14.8a4.8 4.8 0 0 0-2.5-6.6z" />
          <path fill="#50e6ff" d="M6 14.5c0-2.5 2-4.5 4.5-4.5.8 0 1.5.2 2.1.6-1.2 1.5-1.9 3.4-1.9 5.4H6z" />
        </svg>
      );
    case "opera":
      return (
        <svg className={shared} viewBox="0 0 24 24" aria-hidden="true">
          <ellipse cx="12" cy="12" rx="10" ry="11" fill="#ff1b2d" />
          <ellipse cx="12" cy="12" rx="4.5" ry="7.5" fill="#fff" />
        </svg>
      );
    default:
      return (
        <svg className={shared} viewBox="0 0 24 24" aria-hidden="true">
          <circle cx="12" cy="12" r="10" fill="#4285f4" />
          <circle cx="12" cy="12" r="4" fill="#fff" />
          <path
            fill="#34a853"
            d="M12 3a9 9 0 0 1 7.8 4.5H12V3z"
          />
          <path fill="#fbbc04" d="M3.5 8.5A9 9 0 0 0 3 12c0 1.2.2 2.3.6 3.4L12 12V3.5A8.5 8.5 0 0 0 12 3z" />
          <path fill="#ea4335" d="M12 21a9 9 0 0 0 7.4-3.9L15.8 13 12 21z" />
        </svg>
      );
  }
}
