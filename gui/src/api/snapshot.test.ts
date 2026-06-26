import { describe, expect, it } from "vitest";
import { isUnchangedStreamPayload, parseDownloadItem, parseSnapshotPayload } from "./snapshot";

describe("isUnchangedStreamPayload", () => {
  it("returns true for unchanged stream payloads", () => {
    expect(isUnchangedStreamPayload({ type: "unchanged" })).toBe(true);
  });

  it("returns false for snapshots and other payloads", () => {
    expect(isUnchangedStreamPayload({ type: "snapshot" })).toBe(false);
    expect(isUnchangedStreamPayload({ type: "stats" })).toBe(false);
    expect(isUnchangedStreamPayload(null)).toBe(false);
  });
});

describe("parseDownloadItem", () => {
  it("parses referer when present", () => {
    expect(
      parseDownloadItem({
        id: "dl-1",
        filename: "video.mp4",
        url: "https://cdn.example.com/video.mp4",
        referer: "https://example.com/watch?v=1",
        status: "queued",
        bytesDownloaded: 0,
        totalBytes: 0,
      }).referer,
    ).toBe("https://example.com/watch?v=1");
  });

  it("maps legacy originalPage to referer", () => {
    expect(
      parseDownloadItem({
        id: "dl-1",
        filename: "video.mp4",
        url: "https://cdn.example.com/video.mp4",
        originalPage: "https://example.com/watch?v=1",
        status: "queued",
        bytesDownloaded: 0,
        totalBytes: 0,
      }).referer,
    ).toBe("https://example.com/watch?v=1");
  });
});

describe("parseSnapshotPayload", () => {
  it("parses snapshot payloads", () => {
    const parsed = parseSnapshotPayload({
      type: "snapshot",
      downloads: [],
      queues: [],
    });
    expect(parsed?.type).toBe("snapshot");
  });
});
