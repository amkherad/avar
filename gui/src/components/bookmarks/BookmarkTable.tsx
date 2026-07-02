import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import type { BookmarkInfo } from "@/api/types";
import { FontAwesomeIcon } from "@/icons";
import { faExternalLinkAlt, faTrash } from "@fortawesome/free-solid-svg-icons";
import { Button } from "@/components/ui/Button";
import { CopyButton } from "@/components/ui/CopyButton";
import { DataTable, type DataTableColumn } from "@/components/ui/DataTable";
import { TruncateWithTooltip } from "@/components/ui/TruncateWithTooltip";

export interface BookmarkTableProps {
  bookmarks: BookmarkInfo[];
  selectedIds: string[];
  loading?: boolean;
  emptyMessage?: string;
  busyId: string | null;
  batchBusy?: boolean;
  onToggleSelect: (id: string, event?: MouseEvent) => void;
  onSelectAll: (checked: boolean) => void;
  onOpen: (bookmark: BookmarkInfo) => void;
  onRemove: (bookmark: BookmarkInfo) => void;
}

export function BookmarkTable({
  bookmarks,
  selectedIds,
  loading = false,
  emptyMessage,
  busyId,
  batchBusy = false,
  onToggleSelect,
  onSelectAll,
  onOpen,
  onRemove,
}: BookmarkTableProps) {
  const { t } = useTranslation();

  const columns = useMemo((): DataTableColumn<BookmarkInfo>[] => {
    return [
      {
        id: "title",
        header: t("bookmark.pageTitle"),
        width: 220,
        minWidth: 120,
        maxWidth: 480,
        render: (bookmark) => (
          <TruncateWithTooltip
            text={bookmark.title || bookmark.url}
            className="avar-list__title"
          />
        ),
      },
      {
        id: "url",
        header: t("bookmark.url"),
        width: 320,
        minWidth: 160,
        maxWidth: 720,
        render: (bookmark) => (
          <TruncateWithTooltip text={bookmark.url} className="avar-list__meta" />
        ),
      },
      {
        id: "linkCount",
        header: t("bookmark.linkCountField"),
        width: 120,
        minWidth: 90,
        maxWidth: 180,
        align: "end",
        render: (bookmark) => (
          <span className="avar-list__meta">
            {t("bookmark.linkCount", { count: bookmark.linkCount })}
          </span>
        ),
      },
    ];
  }, [t]);

  function renderActions(bookmark: BookmarkInfo) {
    const busy = busyId === bookmark.id || batchBusy;
    return (
      <div className="avar-bookmark-table__actions">
        <Button
          size="sm"
          variant="ghost"
          className="avar-btn--icon-only"
          title={t("bookmark.openLink")}
          aria-label={t("bookmark.openLink")}
          disabled={busy}
          onClick={() => onOpen(bookmark)}
        >
          <FontAwesomeIcon icon={faExternalLinkAlt} />
        </Button>
        <CopyButton text={bookmark.url} label={t("bookmark.copyLink")} />
        <Button
          size="sm"
          variant="ghost"
          className="avar-btn--icon-only"
          title={t("bookmark.remove")}
          aria-label={t("bookmark.remove")}
          loading={busy}
          onClick={() => onRemove(bookmark)}
        >
          <FontAwesomeIcon icon={faTrash} />
        </Button>
      </div>
    );
  }

  return (
    <DataTable
      className="avar-bookmark-table"
      rows={bookmarks}
      columns={columns}
      getRowId={(bookmark) => bookmark.id}
      selectedIds={selectedIds}
      showCheckboxes
      selectAllLabel={t("bookmark.selectAll")}
      getCheckboxLabel={(bookmark) => bookmark.title || bookmark.url}
      onToggleSelect={onToggleSelect}
      onSelectAll={onSelectAll}
      onRowClick={(bookmark, event) => onToggleSelect(bookmark.id, event.nativeEvent)}
      loading={loading}
      emptyMessage={emptyMessage ?? t("bookmark.empty")}
      trailing={{
        width: 132,
        variant: "actions",
        render: renderActions,
      }}
      variant="bordered"
      interactive
    />
  );
}
