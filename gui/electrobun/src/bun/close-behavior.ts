let keepInTrayOnClose = true;

export function setKeepInTrayOnClose(enabled: boolean): void {
  keepInTrayOnClose = enabled !== false;
}

export function shouldKeepInTrayOnClose(): boolean {
  return keepInTrayOnClose;
}
