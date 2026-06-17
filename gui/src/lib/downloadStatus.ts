export function isPaused(status: string): boolean {
  return status === "paused";
}

export function isActive(status: string): boolean {
  return status === "downloading" || status === "queued";
}

export function canStart(status: string): boolean {
  return (
    status === "paused" ||
    status === "queued" ||
    status === "error" ||
    status === "cancelled"
  );
}

export function canStop(status: string): boolean {
  return status === "downloading" || status === "queued";
}

export function canPause(status: string): boolean {
  return isActive(status);
}

export function canResume(status: string): boolean {
  return isPaused(status);
}
