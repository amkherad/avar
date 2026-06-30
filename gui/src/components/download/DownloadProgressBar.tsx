import type { DownloadInfo } from "@/api/types";
import { progressPercent } from "./format";

/** Above this count, section dividers are too dense to see. */
const MAX_SECTION_DIVIDERS = 48;

export interface DownloadProgressBarProps {
  download: Pick<
    DownloadInfo,
    "bytesDownloaded" | "totalBytes" | "chunkSize" | "doneRanges" | "activeRanges"
  >;
  className?: string;
}

function rangeStyle(
  rangeStart: number,
  rangeEnd: number,
  totalBytes: number,
): { left: string; width: string } | null {
  if (totalBytes <= 0 || rangeEnd < rangeStart) {
    return null;
  }

  const clampedStart = Math.max(0, rangeStart);
  const clampedEnd = Math.min(totalBytes - 1, rangeEnd);
  if (clampedEnd < clampedStart) {
    return null;
  }

  const left = (clampedStart / totalBytes) * 100;
  const width = ((clampedEnd - clampedStart + 1) / totalBytes) * 100;
  return { left: `${left}%`, width: `${width}%` };
}

function sectionDividerPercents(
  totalBytes: number,
  chunkSize: number,
): number[] {
  const sectionCount = Math.max(1, Math.ceil(totalBytes / chunkSize));
  if (sectionCount <= 1) {
    return [];
  }

  const dividerCount = Math.min(sectionCount - 1, MAX_SECTION_DIVIDERS);
  const step = (sectionCount - 1) / dividerCount;

  const percents: number[] = [];
  for (let index = 1; index <= dividerCount; index += 1) {
    const sectionIndex = Math.round(index * step);
    const byteOffset = Math.min(totalBytes, sectionIndex * chunkSize);
    percents.push((byteOffset / totalBytes) * 100);
  }

  return percents;
}

export function DownloadProgressBar({ download, className }: DownloadProgressBarProps) {
  const percent = progressPercent(download.bytesDownloaded, download.totalBytes);
  const totalBytes = download.totalBytes;
  const chunkSize = download.chunkSize ?? 0;
  const doneRanges = download.doneRanges ?? [];
  const activeRanges = download.activeRanges ?? [];
  const showRanges =
    totalBytes > 0 && (chunkSize > 0 || doneRanges.length > 0 || activeRanges.length > 0);

  if (!showRanges) {
    return (
      <div className={className ?? "avar-progress"} aria-hidden="true">
        <div className="avar-progress__bar" style={{ width: `${percent}%` }} />
      </div>
    );
  }

  const dividerPercents =
    chunkSize > 0 ? sectionDividerPercents(totalBytes, chunkSize) : [];

  return (
    <div
      className={`${className ?? "avar-progress"} avar-progress--ranges`.trim()}
      aria-hidden="true"
    >
      {dividerPercents.map((leftPercent, index) => (
        <div
          key={`divider-${index}`}
          className="avar-progress__section-divider"
          style={{ left: `${leftPercent}%` }}
        />
      ))}
      {doneRanges.map(([start, end], index) => {
        const style = rangeStyle(start, end, totalBytes);
        if (!style) {
          return null;
        }

        return (
          <div
            key={`done-${start}-${end}-${index}`}
            className="avar-progress__range"
            style={style}
          />
        );
      })}
      {activeRanges.map(([start, end], index) => {
        const style = rangeStyle(start, end, totalBytes);
        if (!style) {
          return null;
        }

        return (
          <div
            key={`active-${start}-${end}-${index}`}
            className="avar-progress__range avar-progress__range--active"
            style={style}
          />
        );
      })}
    </div>
  );
}
