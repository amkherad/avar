import { beforeEach, describe, expect, it, vi } from "vitest";

import { readStashedAddDownloadPrefill } from "@/lib/addDownloadPrefill";
import { openAddDownloadDialog } from "@/lib/openAddDownloadDialog";
import { parsePopupHash } from "@/lib/popup";

describe("openAddDownloadDialog", () => {
  beforeEach(() => {
    window.avar = {
      isElectron: true,
      openPopup: vi.fn().mockResolvedValue(undefined),
    };
  });

  it("stashes manual add prefill before opening the Electron popup", () => {
    openAddDownloadDialog("Add download", "queue-1");

    expect(window.avar?.openPopup).toHaveBeenCalledOnce();
    const popupOptions = vi.mocked(window.avar!.openPopup).mock.calls[0]?.[0];
    expect(popupOptions?.title).toBe("Add download");

    const route = parsePopupHash(popupOptions?.hash ?? "");
    expect(route).toEqual({
      type: "add-download",
      id: expect.stringMatching(/^popup-/),
    });

    const prefill = readStashedAddDownloadPrefill(route!.id);
    expect(prefill).toEqual({
      url: "",
      defaultQueueId: "queue-1",
    });
  });
});
