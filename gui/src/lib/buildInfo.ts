export interface BuildInfo {
  version: string;
  date: string;
  commit: string;
}

export function getBuildInfo(): BuildInfo {
  return {
    version: import.meta.env.VITE_BUILD_VERSION ?? "dev",
    date: import.meta.env.VITE_BUILD_DATE ?? "",
    commit: import.meta.env.VITE_BUILD_COMMIT ?? "",
  };
}
