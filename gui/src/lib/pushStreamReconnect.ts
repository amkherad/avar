let handler: (() => void) | null = null;

export function setPushStreamReconnectHandler(fn: (() => void) | null): void {
  handler = fn;
}

export function requestPushStreamReconnect(): void {
  handler?.();
}
