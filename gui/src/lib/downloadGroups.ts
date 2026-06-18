export const DOWNLOAD_GROUPS = [
  "default",
  "video",
  "audio",
  "document",
  "archive",
  "image",
  "software",
  "other",
] as const;

export type DownloadGroupId = (typeof DOWNLOAD_GROUPS)[number];
