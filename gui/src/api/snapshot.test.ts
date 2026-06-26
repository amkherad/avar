import { describe, expect, it } from "vitest";
import { isUnchangedStreamPayload, parseSnapshotPayload } from "./snapshot";

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
