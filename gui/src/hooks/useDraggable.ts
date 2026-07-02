import { useCallback, useEffect, useRef, useState } from "react";

const VIEWPORT_MARGIN = 16;

export interface UseDraggableOptions {
  enabled?: boolean;
  resetKey?: unknown;
}

interface Point {
  x: number;
  y: number;
}

function clampBetween(value: number, min: number, max: number): number {
  if (min > max) {
    return min;
  }
  return Math.min(Math.max(value, min), max);
}

export function clampDialogOffset(
  element: HTMLElement,
  currentOffset: Point,
  nextOffset: Point,
  margin = VIEWPORT_MARGIN,
): Point {
  const rect = element.getBoundingClientRect();
  const baseLeft = rect.left - currentOffset.x;
  const baseTop = rect.top - currentOffset.y;
  const { width, height } = rect;

  const minX = margin - baseLeft;
  const maxX = window.innerWidth - margin - width - baseLeft;
  const minY = margin - baseTop;
  const maxY = window.innerHeight - margin - height - baseTop;

  return {
    x: clampBetween(nextOffset.x, minX, maxX),
    y: clampBetween(nextOffset.y, minY, maxY),
  };
}

export function useDraggable({ enabled = true, resetKey }: UseDraggableOptions = {}) {
  const [offset, setOffset] = useState<Point>({ x: 0, y: 0 });
  const offsetRef = useRef(offset);
  offsetRef.current = offset;
  const dialogRef = useRef<HTMLDivElement>(null);
  const dragRef = useRef<{
    pointerId: number;
    startX: number;
    startY: number;
    baseX: number;
    baseY: number;
  } | null>(null);

  const applyOffset = useCallback((next: Point) => {
    const dialog = dialogRef.current;
    if (!dialog) {
      setOffset(next);
      return;
    }
    setOffset(clampDialogOffset(dialog, offsetRef.current, next));
  }, []);

  useEffect(() => {
    setOffset({ x: 0, y: 0 });
  }, [resetKey]);

  useEffect(() => {
    function handleResize() {
      const dialog = dialogRef.current;
      if (!dialog) {
        return;
      }
      setOffset((current) => {
        const clamped = clampDialogOffset(dialog, current, current);
        if (clamped.x === current.x && clamped.y === current.y) {
          return current;
        }
        return clamped;
      });
    }

    window.addEventListener("resize", handleResize);
    return () => window.removeEventListener("resize", handleResize);
  }, []);

  const onPointerDown = useCallback(
    (event: React.PointerEvent<HTMLElement>) => {
      if (!enabled || event.button !== 0) {
        return;
      }
      if ((event.target as HTMLElement).closest("button, a, input, label, select, textarea")) {
        return;
      }
      event.preventDefault();
      event.currentTarget.setPointerCapture(event.pointerId);
      dragRef.current = {
        pointerId: event.pointerId,
        startX: event.clientX,
        startY: event.clientY,
        baseX: offsetRef.current.x,
        baseY: offsetRef.current.y,
      };
    },
    [enabled],
  );

  const onPointerMove = useCallback(
    (event: React.PointerEvent<HTMLElement>) => {
      const drag = dragRef.current;
      if (!drag || drag.pointerId !== event.pointerId) {
        return;
      }
      applyOffset({
        x: drag.baseX + event.clientX - drag.startX,
        y: drag.baseY + event.clientY - drag.startY,
      });
    },
    [applyOffset],
  );

  const endDrag = useCallback((event: React.PointerEvent<HTMLElement>) => {
    const drag = dragRef.current;
    if (!drag || drag.pointerId !== event.pointerId) {
      return;
    }
    dragRef.current = null;
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId);
    }
  }, []);

  return {
    dialogRef,
    dragHandleProps: {
      onPointerDown,
      onPointerMove,
      onPointerUp: endDrag,
      onPointerCancel: endDrag,
      style: {
        cursor: enabled ? ("grab" as const) : undefined,
        touchAction: "none" as const,
      },
    },
    dialogStyle: {
      transform:
        offset.x !== 0 || offset.y !== 0
          ? `translate(${offset.x}px, ${offset.y}px)`
          : undefined,
    },
  };
}
