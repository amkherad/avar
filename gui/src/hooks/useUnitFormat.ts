import { useMemo } from "react";
import { useConfigStore } from "@/stores/configStore";
import {
  formatBytePair as formatBytePairWithPrefs,
  formatBytes as formatBytesWithPrefs,
  formatTransferRate as formatTransferRateWithPrefs,
  type UnitFormatPreferences,
} from "@/lib/units";

export function useUnitFormatPreferences(): UnitFormatPreferences {
  const byteDisplayUnit = useConfigStore((s) => s.config.byteDisplayUnit);
  const transferRateDisplayUnit = useConfigStore((s) => s.config.transferRateDisplayUnit);
  return useMemo(
    () => ({ byteDisplayUnit, transferRateDisplayUnit }),
    [byteDisplayUnit, transferRateDisplayUnit],
  );
}

export function useUnitFormat() {
  const prefs = useUnitFormatPreferences();
  return useMemo(
    () => ({
      formatBytes: (value: number) => formatBytesWithPrefs(value, prefs),
      formatBytePair: (done: number, total: number) => formatBytePairWithPrefs(done, total, prefs),
      formatTransferRate: (bytesPerSecond: number) =>
        formatTransferRateWithPrefs(bytesPerSecond, prefs),
    }),
    [prefs],
  );
}
