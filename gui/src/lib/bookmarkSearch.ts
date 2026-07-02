import type { BookmarkInfo } from "@/api/types";

function normalizeQuery(query: string): string {
  return query.trim().toLowerCase();
}

function bookmarkSearchText(bookmark: BookmarkInfo): string {
  return [bookmark.id, bookmark.title, bookmark.url, String(bookmark.linkCount)]
    .join("\n")
    .toLowerCase();
}

export function filterBookmarksBySearch(
  bookmarks: BookmarkInfo[],
  query: string,
): BookmarkInfo[] {
  const normalized = normalizeQuery(query);
  if (!normalized) {
    return bookmarks;
  }
  return bookmarks.filter((bookmark) => bookmarkSearchText(bookmark).includes(normalized));
}
