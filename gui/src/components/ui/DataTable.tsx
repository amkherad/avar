import {
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
  type MouseEvent,
  type KeyboardEvent,
} from "react";
import { FontAwesomeIcon } from "@/icons";
import { faGear } from "@fortawesome/free-solid-svg-icons";
import type { IconDefinition } from "@fortawesome/fontawesome-svg-core";
import { Button } from "@/components/ui/Button";
import { ContextMenu } from "@/components/ui/ContextMenu";
import { ResizeHandle } from "@/components/ui/ResizeHandle";
import { Select } from "@/components/ui/Select";
import { Spinner } from "@/components/ui/Spinner";
import { tableSortIndicator, toggleTableSort, type TableSort } from "@/lib/tableSort";

export interface DataTableViewButton {
  id: string;
  label: string;
  icon: IconDefinition;
  active?: boolean;
  onClick: () => void;
}

export interface DataTableChromeConfig {
  viewButtons?: DataTableViewButton[];
  viewModeLabel?: string;
  showCheckboxes?: boolean;
  onToggleCheckboxes?: () => void;
  toggleCheckboxesLabel?: string;
  settingsLabel?: string;
}

export interface DataTableColumn<T> {
  id: string;
  header: ReactNode;
  sortKey?: string;
  width: number;
  minWidth?: number;
  maxWidth?: number;
  onResize?: (width: number) => void;
  headerAddon?: ReactNode;
  /** `stacked` places the addon below the header (downloads status filter). */
  headerLayout?: "inline" | "stacked";
  align?: "start" | "end";
  cellClassName?: string;
  render: (row: T, index: number) => ReactNode;
}

export interface DataTablePagination {
  page: number;
  pageSize: number;
  totalItems: number;
  pageSizes: readonly number[];
  pageSizeLabel: string;
  prevLabel: string;
  nextLabel: string;
  pageLabel: string;
  onPageChange: (page: number) => void;
  onPageSizeChange: (size: number) => void;
}

export interface DataTableProps<T> {
  rows: T[];
  columns: DataTableColumn<T>[];
  getRowId: (row: T) => string;
  selectedIds?: string[];
  showCheckboxes?: boolean;
  selectAllLabel?: string;
  isRowSelectable?: (row: T) => boolean;
  getCheckboxLabel?: (row: T) => string;
  onToggleSelect?: (id: string, event?: MouseEvent) => void;
  onSelectAll?: (checked: boolean) => void;
  sort?: TableSort;
  onSortChange?: (sort: TableSort) => void;
  loading?: boolean;
  emptyMessage?: string;
  trailing?: {
    width: number;
    header?: ReactNode;
    render?: (row: T, index: number) => ReactNode;
    variant?: "actions" | "fill";
  };
  onRowClick?: (row: T, event: MouseEvent) => void;
  onRowDoubleClick?: (row: T) => void;
  onRowContextMenu?: (row: T, event: MouseEvent) => void;
  pagination?: DataTablePagination;
  /** `flex` fills parent height (downloads). `bordered` adds card-like frame (queues). */
  variant?: "default" | "flex" | "bordered";
  /** Highlights selected rows with a border (downloads). */
  interactive?: boolean;
  className?: string;
  chrome?: DataTableChromeConfig;
}

const CHECKBOX_WIDTH = 36;
const CHECKBOX_COL = "2.25rem ";
const DEFAULT_COL_MIN = 60;
/** Matches `gap: 0.5rem` on `.avar-data-table__header` / `__row`. */
const GRID_GAP_PX = 8;

interface TableLayout {
  gridTemplate: string;
  innerMinWidth: number;
  /** Stored-width units per displayed pixel while columns are expanded to fill. */
  fillScale: number;
}

function gridTrackCount(
  showCheckboxes: boolean,
  columnCount: number,
  hasTrailing: boolean,
): number {
  let tracks = columnCount;
  if (showCheckboxes) {
    tracks += 1;
  }
  if (hasTrailing) {
    tracks += 1;
  }
  return tracks;
}

function gridGapTotal(
  showCheckboxes: boolean,
  columnCount: number,
  hasTrailing: boolean,
): number {
  const tracks = gridTrackCount(showCheckboxes, columnCount, hasTrailing);
  return Math.max(0, tracks - 1) * GRID_GAP_PX;
}

function fixedTrackWidth(
  showCheckboxes: boolean,
  trailing?: DataTableProps<unknown>["trailing"],
): number {
  return (showCheckboxes ? CHECKBOX_WIDTH : 0) + (trailing?.width ?? 0);
}

function computeTableLayout(
  scrollWidth: number,
  showCheckboxes: boolean,
  columns: DataTableColumn<unknown>[],
  trailing?: DataTableProps<unknown>["trailing"],
): TableLayout {
  const hasTrailing = trailing !== undefined;
  const fixedWidth = fixedTrackWidth(showCheckboxes, trailing);
  const dataTotal = columns.reduce((sum, column) => sum + column.width, 0);
  const minDataTotal = columns.reduce(
    (sum, column) => sum + (column.minWidth ?? DEFAULT_COL_MIN),
    0,
  );
  const gaps = gridGapTotal(showCheckboxes, columns.length, hasTrailing);
  const totalPreferred = fixedWidth + dataTotal + gaps;
  const totalMin = fixedWidth + minDataTotal + gaps;

  const parts: string[] = [];
  if (showCheckboxes) {
    parts.push(CHECKBOX_COL);
  }

  const available = scrollWidth > 0 ? scrollWidth : totalPreferred;
  const useFill = totalPreferred <= available;

  if (useFill) {
    for (const column of columns) {
      const minW = column.minWidth ?? DEFAULT_COL_MIN;
      const weight = Math.max(column.width, minW);
      parts.push(`minmax(${minW}px, ${weight}fr)`);
    }
    if (trailing) {
      parts.push(`${trailing.width}px`);
    }

    const flexibleWidth = Math.max(0, available - fixedWidth - gaps);
    const fillScale = dataTotal > 0 ? flexibleWidth / dataTotal : 1;

    return {
      gridTemplate: parts.join(" "),
      innerMinWidth: Math.max(totalMin, available),
      fillScale,
    };
  }

  for (const column of columns) {
    parts.push(`${column.width}px`);
  }
  if (trailing) {
    parts.push(`${trailing.width}px`);
  }

  return {
    gridTemplate: parts.join(" "),
    innerMinWidth: totalPreferred,
    fillScale: 1,
  };
}

function DataTableChrome({ chrome }: { chrome: DataTableChromeConfig }) {
  const settingsRef = useRef<HTMLButtonElement>(null);
  const [settingsMenu, setSettingsMenu] = useState<{ x: number; y: number } | null>(null);
  const viewButtons = chrome.viewButtons ?? [];
  const hasViewButtons = viewButtons.length > 0;
  const hasSettings = Boolean(chrome.onToggleCheckboxes);

  const settingsItems = useMemo(() => {
    if (!chrome.onToggleCheckboxes || !chrome.toggleCheckboxesLabel) {
      return [];
    }
    return [
      {
        id: "toggle-checkboxes",
        label: chrome.toggleCheckboxesLabel,
        checked: chrome.showCheckboxes,
        onClick: chrome.onToggleCheckboxes,
      },
    ];
  }, [chrome.onToggleCheckboxes, chrome.showCheckboxes, chrome.toggleCheckboxesLabel]);

  if (!hasViewButtons && !hasSettings) {
    return null;
  }

  return (
    <>
      <div className="avar-data-table__chrome">
        <div
          className="avar-data-table__chrome-controls"
          role={hasViewButtons ? "group" : undefined}
          aria-label={chrome.viewModeLabel}
        >
          {viewButtons.map((button) => (
            <Button
              key={button.id}
              size="sm"
              variant={button.active ? "secondary" : "ghost"}
              className="avar-btn--icon-only"
              aria-pressed={button.active}
              aria-label={button.label}
              title={button.label}
              onClick={button.onClick}
            >
              <FontAwesomeIcon icon={button.icon} />
            </Button>
          ))}
          {hasViewButtons && hasSettings ? (
            <span className="avar-data-table__chrome-separator" aria-hidden="true" />
          ) : null}
          {hasSettings ? (
            <Button
              ref={settingsRef}
              size="sm"
              variant="ghost"
              className="avar-btn--icon-only"
              aria-label={chrome.settingsLabel ?? "Table settings"}
              title={chrome.settingsLabel ?? "Table settings"}
              aria-haspopup="menu"
              aria-expanded={settingsMenu !== null}
              onClick={() => {
                const rect = settingsRef.current?.getBoundingClientRect();
                if (!rect) {
                  return;
                }
                setSettingsMenu({ x: rect.left, y: rect.bottom + 4 });
              }}
            >
              <FontAwesomeIcon icon={faGear} />
            </Button>
          ) : null}
        </div>
      </div>

      {settingsMenu && settingsItems.length > 0 ? (
        <ContextMenu
          x={settingsMenu.x}
          y={settingsMenu.y}
          items={settingsItems}
          onClose={() => setSettingsMenu(null)}
        />
      ) : null}
    </>
  );
}

export function DataTable<T>({
  rows,
  columns,
  getRowId,
  selectedIds = [],
  showCheckboxes = false,
  selectAllLabel = "Select all",
  isRowSelectable,
  getCheckboxLabel,
  onToggleSelect,
  onSelectAll,
  sort,
  onSortChange,
  loading = false,
  emptyMessage,
  trailing,
  onRowClick,
  onRowDoubleClick,
  onRowContextMenu,
  pagination,
  variant = "default",
  interactive = false,
  className = "",
  chrome,
}: DataTableProps<T>) {
  const scrollRef = useRef<HTMLDivElement>(null);
  const [scrollWidth, setScrollWidth] = useState(0);

  useEffect(() => {
    const element = scrollRef.current;
    if (!element) {
      return;
    }

    const observer = new ResizeObserver((entries) => {
      const width = entries[0]?.contentRect.width ?? 0;
      setScrollWidth(width);
    });
    observer.observe(element);
    return () => observer.disconnect();
  }, []);

  const layout = useMemo(
    () =>
      computeTableLayout(
        scrollWidth,
        showCheckboxes,
        columns as DataTableColumn<unknown>[],
        trailing as DataTableProps<unknown>["trailing"],
      ),
    [scrollWidth, showCheckboxes, columns, trailing],
  );
  const allSelected =
    rows.length > 0 && rows.every((row) => selectedIds.includes(getRowId(row)));
  const totalPages = pagination
    ? Math.max(1, Math.ceil(pagination.totalItems / pagination.pageSize))
    : 1;
  const showPaging = pagination !== undefined && totalPages > 1;

  const rootClass = [
    "avar-data-table",
    variant === "flex" ? "avar-data-table--flex" : "",
    variant === "bordered" ? "avar-data-table--bordered" : "",
    interactive ? "avar-data-table--interactive" : "",
    className,
  ]
    .filter(Boolean)
    .join(" ");

  function handleSort(key: string) {
    if (!sort || !onSortChange) {
      return;
    }
    onSortChange(toggleTableSort(sort, key));
  }

  function renderHeaderCell(column: DataTableColumn<T>) {
    const sortable = Boolean(column.sortKey && onSortChange);
    const headerContent = sortable ? (
      <button
        type="button"
        className="avar-data-table__sort-btn"
        onClick={() => handleSort(column.sortKey!)}
      >
        {column.header}
        {sort ? tableSortIndicator(sort, column.sortKey!) : null}
      </button>
    ) : (
      column.header
    );

    const thClass = [
      "avar-data-table__th",
      sortable ? "avar-data-table__th--sortable" : "",
      column.onResize ? "avar-data-table__th--resizable" : "",
      column.align === "end" ? "avar-data-table__th--end" : "",
    ]
      .filter(Boolean)
      .join(" ");

    return (
      <div key={column.id} className={thClass} role="columnheader">
        {column.headerAddon ? (
          <div
            className={`avar-data-table__th-main ${column.headerLayout === "stacked" ? "avar-data-table__th-main--stacked" : ""}`}
          >
            {headerContent}
            {column.headerAddon}
          </div>
        ) : (
          headerContent
        )}
        {column.onResize ? (
          <ResizeHandle
            axis="horizontal"
            min={column.minWidth ?? 60}
            max={column.maxWidth ?? 800}
            className="avar-data-table__col-resize"
            onResize={(delta) => {
              const storedDelta = layout.fillScale > 0 ? delta / layout.fillScale : delta;
              column.onResize!(column.width + storedDelta);
            }}
          />
        ) : null}
      </div>
    );
  }

  return (
    <div className={rootClass}>
      {chrome ? <DataTableChrome chrome={chrome} /> : null}
      <div className="avar-data-table__scroll" ref={scrollRef}>
        <div className="avar-data-table__inner" style={{ minWidth: layout.innerMinWidth }}>
          <div
            className="avar-data-table__header"
            style={{ gridTemplateColumns: layout.gridTemplate }}
            role="row"
          >
            {showCheckboxes ? (
              <div
                className="avar-data-table__th avar-data-table__th--checkbox"
                role="columnheader"
              >
                <input
                  type="checkbox"
                  aria-label={selectAllLabel}
                  checked={allSelected}
                  onChange={(e) => onSelectAll?.(e.target.checked)}
                />
              </div>
            ) : null}
            {columns.map((column) => renderHeaderCell(column))}
            {trailing ? (
              <div
                className={`avar-data-table__th ${trailing.variant === "actions" ? "avar-data-table__th--actions" : "avar-data-table__th--fill"}`}
                role="columnheader"
              >
                {trailing.header ?? null}
              </div>
            ) : null}
          </div>

          <div className="avar-data-table__body" role="rowgroup">
            {loading ? (
              <div className="avar-data-table__empty">
                <Spinner />
              </div>
            ) : rows.length === 0 ? (
              <div className="avar-data-table__empty">
                {emptyMessage ? <p className="avar-empty">{emptyMessage}</p> : null}
              </div>
            ) : (
              rows.map((row, index) => {
                const rowId = getRowId(row);
                const selected = selectedIds.includes(rowId);
                const selectable = isRowSelectable ? isRowSelectable(row) : true;
                const rowClass = [
                  "avar-data-table__row",
                  selected ? "avar-data-table__row--selected" : "",
                  onRowClick || interactive ? "avar-data-table__row--clickable" : "",
                ]
                  .filter(Boolean)
                  .join(" ");

                return (
                  <div
                    key={rowId}
                    className={rowClass}
                    style={{ gridTemplateColumns: layout.gridTemplate }}
                    role="row"
                    tabIndex={onRowClick ? 0 : undefined}
                    onClick={onRowClick ? (event) => onRowClick(row, event) : undefined}
                    onDoubleClick={onRowDoubleClick ? () => onRowDoubleClick(row) : undefined}
                    onContextMenu={
                      onRowContextMenu
                        ? (event) => {
                            event.preventDefault();
                            onRowContextMenu(row, event);
                          }
                        : undefined
                    }
                    onKeyDown={
                      onRowClick
                        ? (event: KeyboardEvent) => {
                            if (event.key === "Enter") {
                              onRowClick(row, event as unknown as MouseEvent);
                            }
                          }
                        : undefined
                    }
                  >
                    {showCheckboxes ? (
                      <div
                        className="avar-data-table__cell avar-data-table__cell--checkbox"
                        role="cell"
                        onClick={(event) => event.stopPropagation()}
                      >
                        <input
                          type="checkbox"
                          aria-label={getCheckboxLabel?.(row) ?? rowId}
                          checked={selected}
                          disabled={!selectable}
                          onClick={(event) => {
                            event.stopPropagation();
                            onToggleSelect?.(rowId, event.nativeEvent);
                          }}
                        />
                      </div>
                    ) : null}
                    {columns.map((column) => (
                      <div
                        key={column.id}
                        className={[
                          "avar-data-table__cell",
                          column.align === "end" ? "avar-data-table__cell--end" : "",
                          column.cellClassName ?? "",
                        ]
                          .filter(Boolean)
                          .join(" ")}
                        role="cell"
                      >
                        {column.render(row, index)}
                      </div>
                    ))}
                    {trailing?.render ? (
                      <div
                        className={`avar-data-table__cell ${trailing.variant === "actions" ? "avar-data-table__cell--actions" : "avar-data-table__cell--fill"}`}
                        role="cell"
                      >
                        {trailing.render(row, index)}
                      </div>
                    ) : trailing ? (
                      <div
                        className={`avar-data-table__cell ${trailing.variant === "fill" ? "avar-data-table__cell--fill" : ""}`}
                        role="cell"
                      />
                    ) : null}
                  </div>
                );
              })
            )}
          </div>
        </div>
      </div>

      {showPaging && pagination ? (
        <div className="avar-data-table__pager">
          <label className="avar-data-table__page-size">
            <Select
              label={pagination.pageSizeLabel}
              value={String(pagination.pageSize)}
              onChange={(e) => pagination.onPageSizeChange(Number(e.target.value))}
            >
              {pagination.pageSizes.map((size) => (
                <option key={size} value={size}>
                  {size}
                </option>
              ))}
            </Select>
          </label>
          <div className="avar-data-table__page-nav">
            <button
              type="button"
              className="avar-btn avar-btn--sm avar-btn--ghost"
              disabled={pagination.page <= 1}
              onClick={() => pagination.onPageChange(pagination.page - 1)}
            >
              {pagination.prevLabel}
            </button>
            <span className="avar-data-table__page-label">{pagination.pageLabel}</span>
            <button
              type="button"
              className="avar-btn avar-btn--sm avar-btn--ghost"
              disabled={pagination.page >= totalPages}
              onClick={() => pagination.onPageChange(pagination.page + 1)}
            >
              {pagination.nextLabel}
            </button>
          </div>
        </div>
      ) : null}
    </div>
  );
}
