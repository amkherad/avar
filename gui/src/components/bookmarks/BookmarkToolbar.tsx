import { useRef } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faTrash } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { Input } from "@/components/ui/Input";

export interface BookmarkToolbarProps {
  searchQuery: string;
  onSearchChange: (query: string) => void;
  selectedCount: number;
  batchBusy?: boolean;
  onBatchDelete: () => void;
}

export function BookmarkToolbar({
  searchQuery,
  onSearchChange,
  selectedCount,
  batchBusy = false,
  onBatchDelete,
}: BookmarkToolbarProps) {
  const { t } = useTranslation();
  const searchRef = useRef<HTMLInputElement>(null);

  return (
    <div className="avar-download-toolbar avar-bookmark-toolbar">
      <div className="avar-download-toolbar__start">
        {selectedCount > 0 ? (
          <div className="avar-download-toolbar__group">
            <span className="avar-download-toolbar__selection">
              {t("bookmark.selectedCount", { count: selectedCount })}
            </span>
            <Button
              size="sm"
              variant="danger"
              loading={batchBusy}
              onClick={() => onBatchDelete()}
            >
              <FontAwesomeIcon icon={faTrash} />
              {t("bookmark.removeBatch")}
            </Button>
          </div>
        ) : null}
      </div>

      <Input
        ref={searchRef}
        className="avar-download-toolbar__search avar-download-toolbar__search--compact"
        value={searchQuery}
        onChange={(e) => onSearchChange(e.target.value)}
        placeholder={t("bookmark.searchPlaceholder")}
        aria-label={t("bookmark.searchPlaceholder")}
      />
    </div>
  );
}
