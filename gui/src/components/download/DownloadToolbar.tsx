import { useRef } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faPlus, faTableCells, faTableList } from "@fortawesome/free-solid-svg-icons";
import { Input } from "@/components/ui/Input";
import { Button } from "@/components/ui/Button";
import { ShortcutButton } from "@/components/ui/ShortcutButton";
import type { DownloadInfo } from "@/api/types";
import type { DownloadViewMode } from "@/stores/layoutStore";
import { DownloadControls } from "./DownloadControls";
import { useShortcutAction } from "@/shortcuts/useShortcutAction";

export interface DownloadToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  viewMode: DownloadViewMode;
  onViewModeChange: (mode: DownloadViewMode) => void;
  selectedDownloads: DownloadInfo[];
  onAddDownload: () => void;
}

export function DownloadToolbar({
  searchQuery,
  onSearchChange,
  viewMode,
  onViewModeChange,
  selectedDownloads,
  onAddDownload,
}: DownloadToolbarProps) {
  const { t } = useTranslation();
  const searchRef = useRef<HTMLInputElement>(null);

  useShortcutAction("download.search", () => searchRef.current?.focus());

  return (
    <div className="avar-download-toolbar">
      <div className="avar-download-toolbar__start">
        <ShortcutButton
          size="sm"
          variant="primary"
          shortcut="download.add"
          onClick={onAddDownload}
        >
          <FontAwesomeIcon icon={faPlus} />
          {t("download.add")}
        </ShortcutButton>

        {selectedDownloads.length > 0 ? (
          <div className="avar-download-toolbar__group">
            <span className="avar-download-toolbar__selection">
              {t("download.selectedCount", { count: selectedDownloads.length })}
            </span>
            <DownloadControls downloads={selectedDownloads} />
          </div>
        ) : null}

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
      </div>

      <Input
        ref={searchRef}
        className="avar-download-toolbar__search"
        label={t("download.search")}
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
        placeholder={t("download.searchPlaceholder")}
      />
    </div>
  );
}
