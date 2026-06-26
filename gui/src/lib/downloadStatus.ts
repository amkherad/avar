export function isPaused(status: string): boolean {
  return status === "paused";
}

export function isActive(status: string): boolean {
  return status === "downloading";
}

export function isError(status: string): boolean {
  return status === "error" || status === "failed";
}

export function canStart(status: string): boolean {
  return (
    status === "queued" ||
    status === "paused" ||
    isError(status) ||
    status === "stopped"
  );
}

export function canStop(status: string): boolean {
  return status === "downloading" || status === "paused";
}

export function canPause(status: string): boolean {
  return status === "downloading";
}

export function canResume(status: string): boolean {
  return isPaused(status);
}

export function isCompleted(status: string): boolean {
  return status === "completed";
}

export function canRedownload(status: string): boolean {
  return isCompleted(status);
}
