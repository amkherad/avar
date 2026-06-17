import { useResize, type ResizeAxis } from "@/hooks/useResize";

export interface ResizeHandleProps {
  axis: ResizeAxis;
  min: number;
  max: number;
  invert?: boolean;
  onResize: (delta: number) => void;
  onResizeEnd?: () => void;
  className?: string;
  label?: string;
}

export function ResizeHandle({
  axis,
  min,
  max,
  invert,
  onResize,
  onResizeEnd,
  className = "",
  label,
}: ResizeHandleProps) {
  const { onPointerDown } = useResize({ axis, min, max, invert, onResize, onResizeEnd });

  return (
    <div
      className={`avar-resize-handle avar-resize-handle--${axis} ${className}`.trim()}
      role="separator"
      aria-orientation={axis === "horizontal" ? "vertical" : "horizontal"}
      aria-label={label}
      onPointerDown={onPointerDown}
    />
  );
}
