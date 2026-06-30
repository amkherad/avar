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
  if (done >= total) {
    return 100;
  }
  return Math.floor((done * 100) / total);
}

export function formatTransferRate(
  bytesPerSecond: number,
  prefs: UnitFormatPreferences = defaultUnitFormatPreferences(),
): string {
  return formatTransferRateWithPrefs(bytesPerSecond, prefs);
}
