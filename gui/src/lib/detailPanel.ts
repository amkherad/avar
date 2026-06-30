import { useDataStore } from "@/stores/dataStore";
import { useLayoutStore } from "@/stores/layoutStore";
import { appLogger } from "@/lib/appLogger";

export function toggleDetailPanelWithSelection(): void {
  const { selectedDownloadId, visibleDownloadOrder, setSelectedDownloadId } =
    useDataStore.getState();
  const { setDetailPanelOpen, toggleDetailPanel } = useLayoutStore.getState();

  if (visibleDownloadOrder.length === 0) {
    appLogger.gui.debug("Detail panel toggle skipped (no downloads)");
    return;
  }

  if (!selectedDownloadId) {
    appLogger.gui.debug("Detail panel opened with first download");
    setSelectedDownloadId(visibleDownloadOrder[0]);
    setDetailPanelOpen(true);
    return;
  }

  toggleDetailPanel();
}
