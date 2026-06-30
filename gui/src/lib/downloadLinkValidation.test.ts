import { describe, expect, it } from "vitest";
import * as linkValidation from "@/lib/downloadLinkValidation";

describe("downloadLinkValidation", () => {
  it("compares referer hosts", () => {
    expect(linkValidation.refererHost("https://cdn.example.com/file")).toBe("cdn.example.com");
    expect(
      linkValidation.referrersLikelyMatch("https://a.example/page", "https://b.example/other"),
    ).toBe(false);
    expect(
      linkValidation.referrersLikelyMatch("https://a.example/page", "https://a.example/other"),
    ).toBe(true);
  });

  it("detects file size mismatches only when both sizes are known", () => {
    expect(linkValidation.fileSizesMismatch(100, 100)).toBe(false);
    expect(linkValidation.fileSizesMismatch(100, 200)).toBe(true);
    expect(linkValidation.fileSizesMismatch(0, 200)).toBe(false);
    expect(linkValidation.fileSizesMismatch(100, 0)).toBe(false);
  });

  it("resolves download referer from details or download info", () => {
    expect(
      linkValidation.downloadRefererUrl(
        { referer: "https://example.com/a" },
        { referer: "https://example.com/b" },
      ),
    ).toBe("https://example.com/a");
    expect(
      linkValidation.downloadRefererUrl(
        { originalPage: "https://example.com/page" },
        undefined,
      ),
    ).toBe("https://example.com/page");
    expect(linkValidation.downloadRefererUrl(undefined, { referer: "https://example.com/c" })).toBe(
      "https://example.com/c",
    );
  });
});
