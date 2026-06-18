export function isPaused(status: string): boolean {
  return status === "paused";
}

export function isActive(status: string): boolean {
  return status === "downloading";
}

export function canStart(status: string): boolean {
  return (
    status === "paused" ||
    status === "error" ||
    status === "failed" ||
    status === "cancelled"
  );
}

export function canStop(status: string): boolean {
  return status === "downloading";
}

export function canPause(status: string): boolean {
  return status === "downloading";
}

export function canResume(status: string): boolean {
  return isPaused(status);
}
