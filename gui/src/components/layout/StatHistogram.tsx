export interface StatHistogramProps {
  values: number[];
  max?: number;
  label: string;
  textValue: string;
  className?: string;
}

export function StatHistogram({ values, max, label, textValue, className }: StatHistogramProps) {
  const peak = max ?? Math.max(1, ...values, 1);

  return (
    <div
      className={`avar-stat-histogram ${className ?? ""}`}
      role="img"
      aria-label={`${label}: ${textValue}`}
    >
      <div className="avar-stat-histogram__bars" aria-hidden="true">
        {values.map((value, index) => {
          const height = Math.max(2, Math.round((value / peak) * 100));
          return (
            <span
              key={`${index}-${value}`}
              className="avar-stat-histogram__bar"
              style={{ height: `${height}%` }}
            />
          );
        })}
      </div>
      <span className="avar-stat-histogram__text">{textValue}</span>
    </div>
  );
}
