import { afterEach, describe, expect, it, vi } from "vitest";

import {
  loadAddDownloadPrefill,
  normalizeAddDownloadPrefill,
  readStashedAddDownloadPrefill,
  stashAddDownloadPrefill,
} from "@/lib/addDownloadPrefill";

describe("normalizeAddDownloadPrefill", () => {
  it("keeps an empty URL for manual add-download popups", () => {
    expect(
      normalizeAddDownloadPrefill({
        url: "",
        defaultQueueId: "queue-1",
      }),
    ).toEqual({
      url: "",
      defaultQueueId: "queue-1",
    });
  });

  it("trims populated extension payloads", () => {
    expect(
      normalizeAddDownloadPrefill({
        url: " https://example.com/file.bin ",
        filename: " file.bin ",
        referer: " https://example.com ",
        streamKind: " progressive ",
        defaultQueueId: null,
        pageTitle: " Example page ",
      }),
    ).toEqual({
      url: "https://example.com/file.bin",
      filename: "file.bin",
      referer: "https://example.com",
      streamKind: "progressive",
      defaultQueueId: null,
      pageTitle: "Example page",
    });
  });
});

describe("stashAddDownloadPrefill", () => {
  it("persists manual add payloads with an empty URL", () => {
    stashAddDownloadPrefill("popup-test-1", {
      url: "",
      defaultQueueId: "queue-42",
    });

    expect(readStashedAddDownloadPrefill("popup-test-1")).toEqual({
      url: "",
      defaultQueueId: "queue-42",
    });
    expect(localStorage.getItem("avar.popup.add-download.popup-test-1")).not.toBeNull();
  });
});

describe("loadAddDownloadPrefill", () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it("falls back to the extension bridge when local storage is empty", async () => {
    const fetchMock = vi.fn().mockResolvedValue({
      ok: true,
      json: async () => ({
        ok: true,
        payload: {
          url: "https://cdn.example.com/video.mp4",
          pageTitle: "Captured media",
        },
      }),
    });
    vi.stubGlobal("fetch", fetchMock);

    await expect(loadAddDownloadPrefill("add-extension-1")).resolves.toEqual({
      url: "https://cdn.example.com/video.mp4",
      defaultQueueId: null,
      pageTitle: "Captured media",
    });
    expect(fetchMock).toHaveBeenCalledWith(
      expect.stringContaining("/extension/add-download/add-extension-1"),
    );
  });
});
