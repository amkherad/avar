import {
  defaultUnitFormatPreferences,
  formatBytes as formatBytesWithPrefs,
  type UnitFormatPreferences,
} from "@/lib/units";

export function formatBytes(
  value: number,
  prefs: UnitFormatPreferences = defaultUnitFormatPreferences(),
): string {
  return formatBytesWithPrefs(value, prefs);
}

export function formatPercent(value: number): string {
  if (!Number.isFinite(value)) {
    return "—";
  }
  return `${Math.round(value)}%`;
}
