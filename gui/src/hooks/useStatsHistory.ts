import { useEffect, useRef, useState } from "react";
import type { SystemStatsInfo } from "@/api/types";

const HISTORY_LENGTH = 40;

export interface StatsHistory {
  memory: number[];
  cpu: number[];
  network: number[];
}

function pushValue(series: number[], value: number): number[] {
  const next = [...series, value];
  if (next.length > HISTORY_LENGTH) {
    return next.slice(next.length - HISTORY_LENGTH);
  }
  return next;
}

export function useStatsHistory(
  stats: SystemStatsInfo | null,
  enabled: boolean,
): StatsHistory {
  const [history, setHistory] = useState<StatsHistory>({
    memory: [],
    cpu: [],
    network: [],
  });
  const lastSampleRef = useRef<string | null>(null);

  useEffect(() => {
    if (!enabled || !stats) {
      return;
    }

    const fingerprint = [
      stats.memoryUsedPercent,
      stats.cpuUsagePercent,
      stats.networkRxBytesPerSec,
    ].join(":");

    if (fingerprint === lastSampleRef.current) {
      return;
    }

    lastSampleRef.current = fingerprint;
    setHistory((prev) => ({
      memory: pushValue(prev.memory, stats.memoryUsedPercent),
      cpu: pushValue(prev.cpu, stats.cpuUsagePercent),
      network: pushValue(prev.network, stats.networkRxBytesPerSec),
    }));
  }, [enabled, stats]);

  useEffect(() => {
    if (!enabled) {
      lastSampleRef.current = null;
      setHistory({ memory: [], cpu: [], network: [] });
    }
  }, [enabled]);

  return history;
}
