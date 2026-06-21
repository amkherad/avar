export type TableSortDirection = "asc" | "desc";

export interface TableSort {
  key: string | null;
  direction: TableSortDirection;
}

export function toggleTableSort(current: TableSort, key: string): TableSort {
  if (current.key !== key) {
    return { key, direction: "asc" };
  }
  if (current.direction === "asc") {
    return { key, direction: "desc" };
  }
  return { key: null, direction: "asc" };
}

export function tableSortIndicator(sort: TableSort, key: string): string {
  if (sort.key !== key) {
    return "";
  }
  return sort.direction === "asc" ? " ▲" : " ▼";
}
