/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_BUILD_VERSION: string;
  readonly VITE_BUILD_DATE: string;
  readonly VITE_BUILD_COMMIT: string;
  readonly VITE_GITHUB_REPO: string;
  readonly VITE_GITHUB_AUTHOR: string;
  readonly VITE_GITHUB_SPONSORS: string;
}

interface ImportMeta {
  readonly env: ImportMetaEnv;
}
