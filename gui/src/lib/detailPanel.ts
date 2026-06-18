import { useDataStore } from "@/stores/dataStore";
import { useLayoutStore } from "@/stores/layoutStore";

export function toggleDetailPanelWithSelection(): void {
  const { selectedDownloadId, visibleDownloadOrder, setSelectedDownloadId } =
    useDataStore.getState();
  const { setDetailPanelOpen, toggleDetailPanel } = useLayoutStore.getState();

  if (visibleDownloadOrder.length === 0) {
    return;
  }

  if (!selectedDownloadId) {
    setSelectedDownloadId(visibleDownloadOrder[0]);
    setDetailPanelOpen(true);
    return;
  }

  toggleDetailPanel();
}
