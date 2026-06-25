import { cleanup, render, screen, waitFor, within } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { loadAddDownloadPrefill } from "@/lib/addDownloadPrefill";
import { AddDownloadPopupPage } from "@/pages/AddDownloadPopupPage";

vi.mock("@/lib/addDownloadPrefill", async (importOriginal) => {
  const actual = await importOriginal<typeof import("@/lib/addDownloadPrefill")>();
  return {
    ...actual,
    loadAddDownloadPrefill: vi.fn(),
  };
});

vi.mock("@/hooks/useDirectoryPathMode", () => ({
  useDaemonDirectoryPathMode: () => "text-only",
}));

vi.mock("@/components/settings/ProxySettingsFields", () => ({
  ProxySettingsFields: () => <div data-testid="proxy-fields" />,
}));

vi.mock("@/stores/connectionStore", () => ({
  useConnectionStore: (selector: (state: { client: object | null; connection: string }) => unknown) =>
    selector({
      client: {},
      connection: "connected",
    }),
}));

vi.mock("@/stores/dataStore", () => {
  const state = {
    queues: [] as [],
    selectedQueueId: "queue-1" as string | null,
    refresh: vi.fn().mockResolvedValue(undefined),
  };

  return {
    useDataStore: Object.assign((selector: (s: typeof state) => unknown) => selector(state), {
      getState: () => state,
    }),
    selectEffectiveQueueId: (s: { selectedQueueId: string | null }) => s.selectedQueueId,
    selectSelectedQueue: () => null,
  };
});

const loadAddDownloadPrefillMock = vi.mocked(loadAddDownloadPrefill);

describe("AddDownloadPopupPage", () => {
  afterEach(() => {
    cleanup();
  });

  beforeEach(() => {
    loadAddDownloadPrefillMock.mockReset();
  });

  it("renders the add form for manual popups with an empty-url prefill", async () => {
    loadAddDownloadPrefillMock.mockResolvedValue({
      url: "",
      defaultQueueId: "queue-1",
    });

    render(<AddDownloadPopupPage addId="popup-manual-1" />);

    await waitFor(() => {
      expect(screen.getByRole("heading", { name: "Add download" })).toBeInTheDocument();
    });
    expect(screen.getByLabelText("URL")).toHaveValue("");
    expect(screen.queryByText("Download details not found")).not.toBeInTheDocument();
  });

  it("renders the add form when a manual popup has no stashed prefill", async () => {
    loadAddDownloadPrefillMock.mockResolvedValue(null);

    render(<AddDownloadPopupPage addId="popup-manual-2" />);

    await waitFor(() => {
      expect(screen.getByRole("heading", { name: "Add download" })).toBeInTheDocument();
    });
    expect(screen.queryByText("Download details not found")).not.toBeInTheDocument();
  });

  it("shows a not-found message for extension popups without bridge prefill", async () => {
    loadAddDownloadPrefillMock.mockResolvedValue(null);

    const { container } = render(<AddDownloadPopupPage addId="add-extension-1" />);

    await waitFor(() => {
      expect(within(container).getByText("Download details not found")).toBeInTheDocument();
    });
    expect(within(container).queryByRole("heading", { name: "Add download" })).not.toBeInTheDocument();
  });
});
