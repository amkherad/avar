import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { FontAwesomeIcon } from "@/icons";
import { faBookmark, faPlus } from "@fortawesome/free-solid-svg-icons";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Spinner } from "@/components/ui/Spinner";
import { ErrorBoundary } from "@/components/ui/ErrorBoundary";
import { Footer } from "@/components/layout/Footer";
import { ConsolePanel } from "@/components/console/ConsolePanel";
import { AddBookmarkModal } from "@/components/bookmarks/AddBookmarkModal";
import { BookmarkTable } from "@/components/bookmarks/BookmarkTable";
import { BookmarkToolbar } from "@/components/bookmarks/BookmarkToolbar";
import { useConnectionStore } from "@/stores/connectionStore";
import { appLogger } from "@/lib/appLogger";
import { filterBookmarksBySearch } from "@/lib/bookmarkSearch";
import { showConfirmDialog } from "@/lib/popup";
import { applyTableSelection } from "@/lib/tableSelection";
import { openExternalUrl } from "@/lib/openExternalUrl";
import type { BookmarkInfo } from "@/api/types";
import { parseBookmarkRecord } from "@/api/snapshot";

const BOOKMARKS_EXPORT_VERSION = 1;

interface BookmarksExportFile {
  version: number;
  bookmarks: BookmarkInfo[];
}

function parseImportPayload(raw: unknown): BookmarkInfo[] {
  if (Array.isArray(raw)) {
    return raw.map((item) => parseBookmarkRecord(item));
  }
  if (raw && typeof raw === "object") {
    const record = raw as Record<string, unknown>;
    if (Array.isArray(record.bookmarks)) {
      return record.bookmarks.map((item) => parseBookmarkRecord(item));
    }
  }
  throw new Error("Invalid bookmarks file");
}

interface BookmarksPanelProps {
  staleBanner?: React.ReactNode;
  errorBanner?: React.ReactNode;
}

export function BookmarksPanel({ staleBanner, errorBanner }: BookmarksPanelProps) {
  const { t } = useTranslation();
  const client = useConnectionStore((s) => s.client);
  const [bookmarks, setBookmarks] = useState<BookmarkInfo[]>([]);
  const [status, setStatus] = useState<"idle" | "loading" | "error">("idle");
  const [error, setError] = useState<string | null>(null);
  const [busyId, setBusyId] = useState<string | null>(null);
  const [batchBusy, setBatchBusy] = useState(false);
  const [importBusy, setImportBusy] = useState(false);
  const [addModalOpen, setAddModalOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");
  const [selectedIds, setSelectedIds] = useState<string[]>([]);
  const selectionAnchorRef = useRef<string | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);

  const filteredBookmarks = useMemo(
    () => filterBookmarksBySearch(bookmarks, searchQuery),
    [bookmarks, searchQuery],
  );
  const visibleIds = useMemo(
    () => filteredBookmarks.map((bookmark) => bookmark.id),
    [filteredBookmarks],
  );

  const refresh = useCallback(async () => {
    if (!client) {
      return;
    }
    setStatus("loading");
    setError(null);
    try {
      const items = await client.listBookmarks();
      setBookmarks(items);
      setStatus("idle");
    } catch (err) {
      setStatus("error");
      setError(err instanceof Error ? err.message : t("bookmark.loadFailed"));
    }
  }, [client, t]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    setSelectedIds((current) => current.filter((id) => visibleIds.includes(id)));
  }, [visibleIds]);

  function applySelection(id: string, event?: MouseEvent) {
    const additive = Boolean(event?.ctrlKey || event?.metaKey);
    const range = Boolean(event?.shiftKey);
    const next = applyTableSelection({
      id,
      orderedIds: visibleIds,
      selectedIds,
      anchorId: selectionAnchorRef.current,
      additive,
      range,
    });
    setSelectedIds(next.selectedIds);
    selectionAnchorRef.current = next.anchorId;
  }

  function handleToggleSelect(id: string, event?: MouseEvent) {
    if (event?.shiftKey || event?.ctrlKey || event?.metaKey) {
      applySelection(id, event);
      return;
    }
    setSelectedIds((current) => {
      const next = current.includes(id)
        ? current.filter((entry) => entry !== id)
        : [...current, id];
      selectionAnchorRef.current = next.includes(id) ? id : (next[next.length - 1] ?? null);
      return next;
    });
  }

  function handleSelectAll(checked: boolean) {
    if (!checked) {
      setSelectedIds([]);
      selectionAnchorRef.current = null;
      return;
    }
    setSelectedIds(visibleIds);
    selectionAnchorRef.current = visibleIds[visibleIds.length - 1] ?? null;
  }

  async function handleRemove(item: BookmarkInfo) {
    if (!client) {
      return;
    }
    const result = await showConfirmDialog({
      title: t("bookmark.removeConfirmTitle"),
      message: t("bookmark.removeConfirmMessage", { title: item.title || item.url }),
      confirmLabel: t("bookmark.remove"),
    });
    if (!result.confirmed) {
      return;
    }

    setBusyId(item.id);
    try {
      await client.removeBookmark({ id: item.id });
      setSelectedIds((current) => current.filter((id) => id !== item.id));
      await refresh();
    } catch (err) {
      setError(err instanceof Error ? err.message : t("bookmark.removeFailed"));
    } finally {
      setBusyId(null);
    }
  }

  async function handleBatchRemove() {
    if (!client || selectedIds.length === 0) {
      return;
    }
    const result = await showConfirmDialog({
      title: t("bookmark.removeConfirmTitle"),
      message: t("bookmark.removeConfirmBatch", { count: selectedIds.length }),
      confirmLabel: t("bookmark.removeBatch"),
    });
    if (!result.confirmed) {
      return;
    }

    setBatchBusy(true);
    setError(null);
    try {
      for (const id of selectedIds) {
        await client.removeBookmark({ id });
      }
      setSelectedIds([]);
      selectionAnchorRef.current = null;
      await refresh();
    } catch (err) {
      setError(err instanceof Error ? err.message : t("bookmark.removeFailed"));
    } finally {
      setBatchBusy(false);
    }
  }

  function handleExport() {
    const payload: BookmarksExportFile = {
      version: BOOKMARKS_EXPORT_VERSION,
      bookmarks,
    };
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    anchor.href = url;
    anchor.download = "bookmarks.json";
    anchor.click();
    URL.revokeObjectURL(url);
    appLogger.gui.debug("Bookmarks exported", bookmarks.length);
  }

  async function handleImportFile(file: File) {
    if (!client) {
      return;
    }
    setImportBusy(true);
    setError(null);
    try {
      const text = await file.text();
      const parsed = parseImportPayload(JSON.parse(text) as unknown);
      for (const item of parsed) {
        if (!item.url.trim()) {
          continue;
        }
        await client.addBookmark({
          url: item.url.trim(),
          title: item.title,
          linkCount: item.linkCount,
        });
      }
      await refresh();
    } catch (err) {
      setError(err instanceof Error ? err.message : t("bookmark.importFailed"));
    } finally {
      setImportBusy(false);
      if (fileInputRef.current) {
        fileInputRef.current.value = "";
      }
    }
  }

  const emptyMessage =
    searchQuery.trim() && filteredBookmarks.length === 0
      ? t("bookmark.searchEmpty")
      : t("bookmark.empty");

  return (
    <div className="avar-dashboard">
      <div className="avar-dashboard__workspace">
        <div className="avar-dashboard__main">
          <Card
            title={t("bookmark.title")}
            actions={
              <>
                <Button
                  size="sm"
                  aria-label={t("bookmark.add")}
                  title={t("bookmark.add")}
                  onClick={() => setAddModalOpen(true)}
                >
                  <FontAwesomeIcon icon={faPlus} />
                </Button>
                <Button
                  size="sm"
                  variant="secondary"
                  disabled={importBusy}
                  onClick={() => fileInputRef.current?.click()}
                >
                  {t("bookmark.import")}
                </Button>
                <Button
                  size="sm"
                  variant="secondary"
                  disabled={bookmarks.length === 0}
                  onClick={() => handleExport()}
                >
                  {t("bookmark.export")}
                </Button>
              </>
            }
          >
            <input
              ref={fileInputRef}
              type="file"
              accept="application/json,.json"
              hidden
              onChange={(event) => {
                const file = event.target.files?.[0];
                if (file) {
                  void handleImportFile(file);
                }
              }}
            />

            {staleBanner}
            {errorBanner}
            {error ? <p className="avar-field__error">{error}</p> : null}

            {status === "loading" && bookmarks.length === 0 ? <Spinner /> : null}

            {status !== "loading" || bookmarks.length > 0 ? (
              <>
                <BookmarkToolbar
                  searchQuery={searchQuery}
                  onSearchChange={setSearchQuery}
                  selectedCount={selectedIds.length}
                  batchBusy={batchBusy}
                  onBatchDelete={() => void handleBatchRemove()}
                />
                <div className="avar-bookmark-table-wrap">
                  <BookmarkTable
                    bookmarks={filteredBookmarks}
                    selectedIds={selectedIds}
                    loading={status === "loading" && bookmarks.length > 0}
                    emptyMessage={emptyMessage}
                    busyId={busyId}
                    batchBusy={batchBusy}
                    onToggleSelect={handleToggleSelect}
                    onSelectAll={handleSelectAll}
                    onOpen={(bookmark) => void openExternalUrl(bookmark.url)}
                    onRemove={(bookmark) => void handleRemove(bookmark)}
                  />
                </div>
              </>
            ) : null}
          </Card>
        </div>
      </div>

      <AddBookmarkModal
        open={addModalOpen}
        onClose={() => setAddModalOpen(false)}
        onCreated={() => void refresh()}
      />

      <Footer />
      <ConsolePanel />
    </div>
  );
}

interface BookmarksPageProps {
  staleBanner?: React.ReactNode;
  errorBanner?: React.ReactNode;
}

export function BookmarksPage({ staleBanner, errorBanner }: BookmarksPageProps) {
  const { t } = useTranslation();

  return (
    <ErrorBoundary name={t("bookmark.title")} resetLabel={t("common.tryAgain")}>
      <BookmarksPanel staleBanner={staleBanner} errorBanner={errorBanner} />
    </ErrorBoundary>
  );
}

export interface BookmarksSidebarItemProps {
  selected: boolean;
  onSelect: () => void;
}

export function BookmarksSidebarItem({ selected, onSelect }: BookmarksSidebarItemProps) {
  const { t } = useTranslation();

  return (
    <ul className="avar-list avar-striped-list avar-queue-sidebar-list avar-bookmarks-sidebar-entry">
      <li>
        <button
          type="button"
          className={`avar-queue-sidebar-list__item ${selected ? "avar-queue-sidebar-list__item--active" : ""}`}
          onClick={onSelect}
        >
          <span className="avar-queue-sidebar-list__title">
            <FontAwesomeIcon icon={faBookmark} className="avar-bookmarks-sidebar-entry__icon" />
            {t("bookmark.sidebarLabel")}
          </span>
          <span className="avar-queue-sidebar-list__description">{t("bookmark.sidebarHint")}</span>
        </button>
      </li>
    </ul>
  );
}
