export interface TableSelectionState {
  selectedIds: string[];
  anchorId: string | null;
}

export interface TableSelectionInput {
  id: string;
  orderedIds: string[];
  selectedIds: string[];
  anchorId: string | null;
  additive?: boolean;
  range?: boolean;
}

export function applyTableSelection({
  id,
  orderedIds,
  selectedIds,
  anchorId,
  additive = false,
  range = false,
}: TableSelectionInput): TableSelectionState {
  if (range && anchorId && orderedIds.length > 0) {
    const anchorIndex = orderedIds.indexOf(anchorId);
    const clickIndex = orderedIds.indexOf(id);
    if (anchorIndex >= 0 && clickIndex >= 0) {
      const start = Math.min(anchorIndex, clickIndex);
      const end = Math.max(anchorIndex, clickIndex);
      return {
        selectedIds: orderedIds.slice(start, end + 1),
        anchorId: id,
      };
    }
  }

  if (additive) {
    const exists = selectedIds.includes(id);
    return {
      selectedIds: exists ? selectedIds.filter((item) => item !== id) : [...selectedIds, id],
      anchorId: id,
    };
  }

  return { selectedIds: [id], anchorId: id };
}
