import { useCallback, useEffect, useRef } from "react";

export type ResizeAxis = "horizontal" | "vertical";

export interface UseResizeOptions {
  axis: ResizeAxis;
  min: number;
  max: number;
  /** Positive delta grows the panel (sidebar/console/detail). */
  invert?: boolean;
  onResize: (delta: number) => void;
  onResizeEnd?: () => void;
}

export function useResize({
  axis,
  invert = false,
  onResize,
  onResizeEnd,
}: UseResizeOptions) {
  const startPos = useRef(0);
  const active = useRef(false);

  const onPointerDown = useCallback(
    (e: React.PointerEvent) => {
      e.preventDefault();
      active.current = true;
      startPos.current = axis === "horizontal" ? e.clientX : e.clientY;
      (e.target as HTMLElement).setPointerCapture(e.pointerId);
    },
    [axis],
  );

  useEffect(() => {
    function onPointerMove(e: PointerEvent) {
      if (!active.current) {
        return;
      }
      const current = axis === "horizontal" ? e.clientX : e.clientY;
      let delta = current - startPos.current;
      if (invert) {
        delta = -delta;
      }
      startPos.current = current;
      onResize(delta);
    }

    function onPointerUp() {
      if (!active.current) {
        return;
      }
      active.current = false;
      onResizeEnd?.();
    }

    window.addEventListener("pointermove", onPointerMove);
    window.addEventListener("pointerup", onPointerUp);
    return () => {
      window.removeEventListener("pointermove", onPointerMove);
      window.removeEventListener("pointerup", onPointerUp);
    };
  }, [axis, invert, onResize, onResizeEnd]);

  return { onPointerDown };
}

export function clampSize(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}
