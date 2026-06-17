import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faTableCells, faTableList } from "@fortawesome/free-solid-svg-icons";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import type { DownloadInfo } from "@/api/types";
import type { DownloadViewMode } from "@/stores/layoutStore";
import { DownloadControls } from "./DownloadControls";

export interface DownloadToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  viewMode: DownloadViewMode;
  onViewModeChange: (mode: DownloadViewMode) => void;
  selectedDownload: DownloadInfo | null;
}

export function DownloadToolbar({
  searchQuery,
  onSearchChange,
  viewMode,
  onViewModeChange,
  selectedDownload,
}: DownloadToolbarProps) {
  const { t } = useTranslation();

  return (
    <div className="avar-download-toolbar">
      <Input
        className="avar-download-toolbar__search"
        label={t("download.search")}
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
        placeholder={t("download.searchPlaceholder")}
      />

      <div className="avar-download-toolbar__group" role="group" aria-label={t("download.viewMode")}>
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

      {selectedDownload ? (
        <div className="avar-download-toolbar__group">
          <DownloadControls download={selectedDownload} />
        </div>
      ) : null}
    </div>
  );
}
