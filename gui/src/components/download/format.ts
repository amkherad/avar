import {
  defaultUnitFormatPreferences,
  formatBytePair as formatBytePairWithPrefs,
  formatBytes as formatBytesWithPrefs,
  formatTransferRate as formatTransferRateWithPrefs,
  type UnitFormatPreferences,
} from "@/lib/units";

export function formatBytes(
  value: number,
  prefs: UnitFormatPreferences = defaultUnitFormatPreferences(),
): string {
  return formatBytesWithPrefs(value, prefs);
}

export function formatBytePair(
  done: number,
  total: number,
  prefs: UnitFormatPreferences = defaultUnitFormatPreferences(),
): string {
  return formatBytePairWithPrefs(done, total, prefs);
}

export function progressPercent(done: number, total: number): number {
  if (total <= 0) {
    return 0;
  }
  return Math.min(100, Math.round((done / total) * 100));
}

export function formatTransferRate(
  bytesPerSecond: number,
  prefs: UnitFormatPreferences = defaultUnitFormatPreferences(),
): string {
  return formatTransferRateWithPrefs(bytesPerSecond, prefs);
}
