export type DirectoryPathValidationResult =
  | { ok: true; normalized: string }
  | { ok: false; message: string };

const WINDOWS_DRIVE_RE = /^[A-Za-z]:[\\/]/;
const WINDOWS_UNC_RE = /^\\\\[^\\]+\\[^\\]+/;
const UNIX_ABSOLUTE_RE = /^\//;

const INVALID_PATH_CHARS_RE = /[\0<>"|?*]/;

export function normalizeDirectoryPathInput(value: string): string {
  return value.trim().replace(/[\\/]+/g, "/").replace(/\/+$/, "");
}

export function validateDirectoryPath(value: string): DirectoryPathValidationResult {
  const trimmed = value.trim();
  if (!trimmed) {
    return { ok: true, normalized: "" };
  }

  if (INVALID_PATH_CHARS_RE.test(trimmed)) {
    return { ok: false, message: "Path contains invalid characters." };
  }

  const normalized = normalizeDirectoryPathInput(trimmed);
  const isWindowsStyle = WINDOWS_DRIVE_RE.test(trimmed) || WINDOWS_UNC_RE.test(trimmed);
  const isUnixStyle = UNIX_ABSOLUTE_RE.test(trimmed);

  if (!isWindowsStyle && !isUnixStyle) {
    return { ok: false, message: "Use an absolute path (e.g. /home/user or C:\\Users)." };
  }

  if (/\/{2,}/.test(normalized.replace(/^\/\//, ""))) {
    return { ok: false, message: "Path contains empty segments." };
  }

  const segments = normalized.split("/").filter(Boolean);
  if (segments.some((segment) => segment === "..")) {
    return { ok: false, message: "Parent directory segments (..) are not allowed." };
  }

  return { ok: true, normalized: trimmed };
}

export function joinDirectoryPath(base: string, name: string): string {
  const trimmedBase = base.trim().replace(/[\\/]+$/, "");
  const trimmedName = name.trim();
  if (!trimmedBase) {
    return trimmedName;
  }
  const separator = WINDOWS_DRIVE_RE.test(trimmedBase) || WINDOWS_UNC_RE.test(trimmedBase) ? "\\" : "/";
  return `${trimmedBase}${separator}${trimmedName}`;
}
