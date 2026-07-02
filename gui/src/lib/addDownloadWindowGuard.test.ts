import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import {
  EXTENSION_GRAB_WINDOW_MS,
  MAX_EXTENSION_GRABS_IN_WINDOW,
  MAX_OPEN_ADD_DOWNLOAD_WINDOWS,
  countOpenAddDownloadWindows,
  countRecentExtensionGrabs,
  releaseAddDownloadSlot,
  resetAddDownloadWindowGuardForTests,
  tryAcquireAddDownloadSlot,
} from "@/lib/addDownloadWindowGuard";

describe("addDownloadWindowGuard", () => {
  beforeEach(() => {
    resetAddDownloadWindowGuardForTests();
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("allows manual opens until the concurrent window limit is reached", () => {
    for (let i = 0; i < MAX_OPEN_ADD_DOWNLOAD_WINDOWS; i += 1) {
      expect(tryAcquireAddDownloadSlot()).toEqual({ allowed: true });
    }

    expect(tryAcquireAddDownloadSlot()).toEqual({
      allowed: false,
      reason: "tooManyOpen",
    });
    expect(countOpenAddDownloadWindows()).toBe(MAX_OPEN_ADD_DOWNLOAD_WINDOWS);
  });

  it("releases slots when an add-download window closes", () => {
    for (let i = 0; i < MAX_OPEN_ADD_DOWNLOAD_WINDOWS; i += 1) {
      tryAcquireAddDownloadSlot();
    }
    releaseAddDownloadSlot();

    expect(tryAcquireAddDownloadSlot()).toEqual({ allowed: true });
  });

  it("rate-limits extension grabs within the sliding window", () => {
    for (let i = 0; i < MAX_EXTENSION_GRABS_IN_WINDOW; i += 1) {
      expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({ allowed: true });
      releaseAddDownloadSlot();
    }

    expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({
      allowed: false,
      reason: "grabRateLimited",
    });
    expect(countRecentExtensionGrabs()).toBe(MAX_EXTENSION_GRABS_IN_WINDOW);
  });

  it("allows extension grabs again after the sliding window expires", () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date("2026-01-01T00:00:00.000Z"));

    for (let i = 0; i < MAX_EXTENSION_GRABS_IN_WINDOW; i += 1) {
      tryAcquireAddDownloadSlot({ fromExtensionGrab: true });
      releaseAddDownloadSlot();
    }

    expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({
      allowed: false,
      reason: "grabRateLimited",
    });

    vi.advanceTimersByTime(EXTENSION_GRAB_WINDOW_MS + 1);

    expect(countRecentExtensionGrabs()).toBe(0);
    expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({ allowed: true });
  });
});
