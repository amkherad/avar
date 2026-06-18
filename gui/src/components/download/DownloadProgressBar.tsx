import type { DownloadInfo } from "@/api/types";
import { progressPercent } from "./format";

export interface DownloadProgressBarProps {
  download: Pick<DownloadInfo, "bytesDownloaded" | "totalBytes" | "chunkSize" | "doneRanges">;
  className?: string;
}

function segmentFillPercent(
  segmentIndex: number,
  chunkSize: number,
  totalBytes: number,
  doneRanges: Array<[number, number]>,
): number {
  const segmentStart = segmentIndex * chunkSize;
  const segmentEnd = Math.min(totalBytes, segmentStart + chunkSize);
  const segmentWidth = segmentEnd - segmentStart;
  if (segmentWidth <= 0) {
    return 0;
  }

  let doneInSegment = 0;
  for (const [rangeStart, rangeEnd] of doneRanges) {
    const overlapStart = Math.max(rangeStart, segmentStart);
    const overlapEnd = Math.min(rangeEnd, segmentEnd - 1);
    if (overlapEnd >= overlapStart) {
      doneInSegment += overlapEnd - overlapStart + 1;
    }
  }

  return Math.min(100, Math.round((doneInSegment / segmentWidth) * 100));
}

export function DownloadProgressBar({ download, className }: DownloadProgressBarProps) {
  const percent = progressPercent(download.bytesDownloaded, download.totalBytes);
  const totalBytes = download.totalBytes;
  const chunkSize = download.chunkSize ?? 0;
  const doneRanges = download.doneRanges ?? [];
  const showSegments = totalBytes > 0 && chunkSize > 0 && doneRanges.length > 0;

  if (!showSegments) {
    return (
      <div className={className ?? "avar-progress"} aria-hidden="true">
        <div className="avar-progress__bar" style={{ width: `${percent}%` }} />
      </div>
    );
  }

  const segmentCount = Math.max(1, Math.ceil(totalBytes / chunkSize));

  return (
    <div
      className={`${className ?? "avar-progress"} avar-progress--segmented`.trim()}
      aria-hidden="true"
    >
      {Array.from({ length: segmentCount }, (_, index) => {
        const fillPercent =
          doneRanges.length > 0
            ? segmentFillPercent(index, chunkSize, totalBytes, doneRanges)
            : index === 0
              ? percent
              : 0;

        return (
          <div key={index} className="avar-progress__segment">
            <div className="avar-progress__segment-fill" style={{ width: `${fillPercent}%` }} />
          </div>
        );
      })}
    </div>
  );
}
