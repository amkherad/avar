import { useMemo, useRef } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faStop, faTableCells, faTableList } from "@fortawesome/free-solid-svg-icons";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import { Select } from "@/components/ui/Select";
import type { DownloadInfo } from "@/api/types";
import type { DownloadViewMode } from "@/stores/layoutStore";
import { formatDownloadStatus } from "@/lib/downloadStatusLabel";
import type { DownloadStatusFilter } from "@/lib/downloadFilterSort";
import { canStop } from "@/lib/downloadStatus";
import { useDownloadActions } from "@/hooks/useDownloadActions";
import { useDataStore } from "@/stores/dataStore";
import { DownloadControls } from "./DownloadControls";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";

export interface DownloadToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  statusFilter: DownloadStatusFilter;
  onStatusFilterChange: (filter: DownloadStatusFilter) => void;
  availableStatuses: string[];
  showStatusFilter?: boolean;
  viewMode: DownloadViewMode;
  onViewModeChange: (mode: DownloadViewMode) => void;
  selectedDownloads: DownloadInfo[];
}

export function DownloadToolbar({
  searchQuery,
  onSearchChange,
  statusFilter,
  onStatusFilterChange,
  availableStatuses,
  showStatusFilter = false,
  viewMode,
  onViewModeChange,
  selectedDownloads,
}: DownloadToolbarProps) {
  const { t } = useTranslation();
  const searchRef = useRef<HTMLInputElement>(null);
  const allDownloads = useDataStore((s) => s.downloads);
  const { busy, stop } = useDownloadActions();

  const stoppableIds = useMemo(
    () => allDownloads.filter((item) => canStop(item.status)).map((item) => item.id),
    [allDownloads],
  );

  useShortcutAction("download.search", () => searchRef.current?.focus());

  return (
    <div className="avar-download-toolbar">
      <div className="avar-download-toolbar__start">
        {stoppableIds.length > 0 ? (
          <div className="avar-download-toolbar__group">
            <Button
              size="sm"
              variant="secondary"
              loading={busy}
              title={t("download.stopAll")}
              aria-label={t("download.stopAll")}
              onClick={() => void stop(stoppableIds)}
            >
              <FontAwesomeIcon icon={faStop} />
              {t("download.stopAll")}
            </Button>
          </div>
        ) : null}

        {selectedDownloads.length > 0 ? (
          <div className="avar-download-toolbar__group">
            <span className="avar-download-toolbar__selection">
              {t("download.selectedCount", { count: selectedDownloads.length })}
            </span>
            <DownloadControls downloads={selectedDownloads} />
          </div>
        ) : null}

        {viewMode === "grid" ? (
          <div
            className="avar-download-toolbar__group"
            role="group"
            aria-label={t("download.viewMode")}
          >
            <Button
              size="sm"
              variant={viewMode === "grid" ? "secondary" : "ghost"}
              aria-pressed={viewMode === "grid"}
              title={t("download.viewGrid")}
              onClick={() => onViewModeChange("grid")}
            >
              <FontAwesomeIcon icon={faTableCells} />
            </Button>
            <Button
              size="sm"
              variant={viewMode === "compact" ? "secondary" : "ghost"}
              aria-pressed={viewMode === "compact"}
              title={t("download.viewCompact")}
              onClick={() => onViewModeChange("compact")}
            >
              <FontAwesomeIcon icon={faTableList} />
            </Button>
          </div>
        ) : null}
      </div>

      <Input
        ref={searchRef}
        className="avar-download-toolbar__search avar-download-toolbar__search--compact"
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
        placeholder={t("download.searchPlaceholder")}
        aria-label={t("download.searchPlaceholder")}
      />

      {showStatusFilter ? (
        <Select
          compact
          className="avar-download-toolbar__status-filter"
          label={t("download.statusFilter")}
          value={statusFilter}
          onChange={(e) => onStatusFilterChange(e.target.value as DownloadStatusFilter)}
        >
          <option value="all">{t("download.statusFilterAll")}</option>
          {availableStatuses.map((status) => (
            <option key={status} value={status}>
              {formatDownloadStatus(status, t)}
            </option>
          ))}
        </Select>
      ) : null}
    </div>
  );
}
