/** Sentinel sidebar id for the bookmarks view (not a download queue). */
export const BOOKMARKS_VIEW_ID = "__bookmarks__";

export function isBookmarksView(id: string | null | undefined): boolean {
  return id === BOOKMARKS_VIEW_ID;
}
