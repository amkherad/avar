import { beforeEach, describe, expect, it, vi } from "vitest";

import { readStashedAddDownloadPrefill } from "@/lib/addDownloadPrefill";
import {
  MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW,
  resetAddDownloadWindowGuardForTests,
  tryAcquireAddDownloadSlot,
} from "@/lib/addDownloadWindowGuard";
import { openAddDownloadDialog } from "@/lib/openAddDownloadDialog";
import { parsePopupHash } from "@/lib/popup";

function listAddDownloadPrefillKeys(): string[] {
  return Object.keys(localStorage).filter((key) => key.startsWith("avar.popup.add-download."));
}

describe("openAddDownloadDialog", () => {
  beforeEach(() => {
    resetAddDownloadWindowGuardForTests();
    localStorage.clear();
    window.avar = {
      isElectron: true,
      openPopup: vi.fn().mockResolvedValue(1),
    };
  });

  it("stashes manual add prefill before opening the Electron popup", () => {
    openAddDownloadDialog("Add download", "queue-1");

    expect(window.avar?.openPopup).toHaveBeenCalledOnce();
    const popupOptions = vi.mocked(window.avar!.openPopup).mock.calls[0]?.[0];
    expect(popupOptions?.title).toBe("Add download");
    expect(popupOptions?.alwaysOnTop).toBe(true);

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

  it("does not open another popup when the web guard limit is reached", () => {
    window.avar = undefined;

    for (let i = 0; i < MAX_ADD_DOWNLOAD_OPENS_IN_WINDOW; i += 1) {
      tryAcquireAddDownloadSlot();
    }

    const keysBefore = listAddDownloadPrefillKeys();
    openAddDownloadDialog("Add download", "queue-1");
    expect(listAddDownloadPrefillKeys()).toEqual(keysBefore);
  });
});
