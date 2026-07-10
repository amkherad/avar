import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import {
  ADD_DOWNLOAD_OPEN_WINDOW_MS,
  MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW,
  countRecentAddDownloadOpens,
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

  it("allows opens until the sliding-window limit is reached", () => {
    for (let i = 0; i < MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW; i += 1) {
      expect(tryAcquireAddDownloadSlot()).toEqual({ allowed: true });
    }

    expect(tryAcquireAddDownloadSlot()).toEqual({
      allowed: false,
      reason: "rateLimited",
    });
    expect(countRecentAddDownloadOpens()).toBe(MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW);
  });

  it("does not free slots when an add-download window closes", () => {
    for (let i = 0; i < MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW; i += 1) {
      tryAcquireAddDownloadSlot();
    }

    expect(tryAcquireAddDownloadSlot()).toEqual({
      allowed: false,
      reason: "rateLimited",
    });
  });

  it("rate-limits manual and extension opens with the same sliding window", () => {
    for (let i = 0; i < MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW; i += 1) {
      expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({ allowed: true });
    }

    expect(tryAcquireAddDownloadSlot()).toEqual({
      allowed: false,
      reason: "rateLimited",
    });
    expect(tryAcquireAddDownloadSlot({ fromExtensionGrab: true })).toEqual({
      allowed: false,
      reason: "rateLimited",
    });
    expect(countRecentAddDownloadOpens()).toBe(MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW);
  });

  it("allows opens again after the sliding window expires", () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date("2026-01-01T00:00:00.000Z"));

    for (let i = 0; i < MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW; i += 1) {
      tryAcquireAddDownloadSlot();
    }

    expect(tryAcquireAddDownloadSlot()).toEqual({
      allowed: false,
      reason: "rateLimited",
    });

    vi.advanceTimersByTime(ADD_DOWNLOAD_OPEN_WINDOW_MS + 1);

    expect(countRecentAddDownloadOpens()).toBe(0);
    expect(tryAcquireAddDownloadSlot()).toEqual({ allowed: true });
  });
});
