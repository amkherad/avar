import { describe, expect, it, vi } from "vitest";
import type { DownloadInfo } from "@/api/types";
import {
  collectDownloadUrls,
  exportDownloadUrlsToTextFile,
  formatDownloadUrlsAsText,
} from "@/lib/downloadListTextIo";

function download(url: string, id = url): DownloadInfo {
  return {
    id,
    filename: "file.bin",
    url,
    status: "queued",
    bytesDownloaded: 0,
    totalBytes: 0,
  };
}

describe("downloadListTextIo", () => {
  it("collects unique trimmed URLs in list order", () => {
    const urls = collectDownloadUrls([
      download("https://example.com/a"),
      download("https://example.com/b"),
      download(" https://example.com/a "),
      download("", "empty"),
      download("https://example.com/c"),
    ]);

    expect(urls).toEqual([
      "https://example.com/a",
      "https://example.com/b",
      "https://example.com/c",
    ]);
  });

  it("formats URLs as one line per entry", () => {
    expect(formatDownloadUrlsAsText(["https://a.test", "https://b.test"])).toBe(
      "https://a.test\nhttps://b.test\n",
    );
    expect(formatDownloadUrlsAsText([])).toBe("");
  });

  it("triggers a text file download", async () => {
    vi.useFakeTimers();
    const clicks: HTMLAnchorElement[] = [];
    const originalCreateElement = document.createElement.bind(document);
    const originalCreateObjectURL = URL.createObjectURL;
    const originalRevokeObjectURL = URL.revokeObjectURL;

    URL.createObjectURL = vi.fn(() => "blob:test");
    URL.revokeObjectURL = vi.fn();

    const createElementSpy = vi
      .spyOn(document, "createElement")
      .mockImplementation((tagName: string, options?: ElementCreationOptions) => {
        const element = originalCreateElement(tagName, options);
        if (tagName === "a") {
          element.click = () => {
            clicks.push(element as HTMLAnchorElement);
          };
        }
        return element;
      });

    exportDownloadUrlsToTextFile(["https://example.com/file.zip"], "links.txt");

    expect(URL.createObjectURL).toHaveBeenCalled();
    expect(clicks).toHaveLength(1);
    expect(clicks[0]?.download).toBe("links.txt");
    expect(clicks[0]?.href).toBe("blob:test");

    await vi.runAllTimersAsync();

    createElementSpy.mockRestore();
    URL.createObjectURL = originalCreateObjectURL;
    URL.revokeObjectURL = originalRevokeObjectURL;
    vi.useRealTimers();
  });
});
