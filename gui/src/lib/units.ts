export type ByteDisplayUnit = "binary" | "decimal";

export type TransferRateDisplayUnit = "binary-bytes" | "binary-bits";

export interface UnitFormatPreferences {
  byteDisplayUnit: ByteDisplayUnit;
  transferRateDisplayUnit: TransferRateDisplayUnit;
}

export const defaultUnitFormatPreferences = (): UnitFormatPreferences => ({
  byteDisplayUnit: "binary",
  transferRateDisplayUnit: "binary-bytes",
});

const BINARY_BYTE_UNITS = ["B", "KiB", "MiB", "GiB", "TiB"] as const;
const DECIMAL_BYTE_UNITS = ["B", "KB", "MB", "GB", "TB"] as const;
const BINARY_BYTE_RATE_UNITS = ["B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"] as const;
const BINARY_BIT_RATE_UNITS = ["bit/s", "Kib/s", "Mib/s", "Gib/s", "Tib/s"] as const;

function formatScaledValue(
  value: number,
  units: readonly string[],
  divisor: number,
): string {
  let size = value;
  let unitIndex = 0;
  while (size >= divisor && unitIndex < units.length - 1) {
    size /= divisor;
    unitIndex += 1;
  }

  const digits = size >= 100 || unitIndex === 0 ? 0 : size >= 10 ? 1 : 2;
  return `${size.toFixed(digits)} ${units[unitIndex]}`;
}

export function formatBytes(value: number, prefs: UnitFormatPreferences): string {
  if (!Number.isFinite(value) || value < 0) {
    return "—";
  }
  if (value === 0) {
    return "0 B";
  }

  if (prefs.byteDisplayUnit === "decimal") {
    return formatScaledValue(value, DECIMAL_BYTE_UNITS, 1000);
  }
  return formatScaledValue(value, BINARY_BYTE_UNITS, 1024);
}

export function formatBytePair(
  done: number,
  total: number,
  prefs: UnitFormatPreferences,
): string {
  return `${formatBytes(done, prefs)} / ${formatBytes(total, prefs)}`;
}

export function formatTransferRate(
  bytesPerSecond: number,
  prefs: UnitFormatPreferences,
): string {
  if (!Number.isFinite(bytesPerSecond) || bytesPerSecond <= 0) {
    return "—";
  }

  if (prefs.transferRateDisplayUnit === "binary-bits") {
    const bitsPerSecond = bytesPerSecond * 8;
    return formatScaledValue(bitsPerSecond, BINARY_BIT_RATE_UNITS, 1000);
  }

  return formatScaledValue(bytesPerSecond, BINARY_BYTE_RATE_UNITS, 1024);
}
