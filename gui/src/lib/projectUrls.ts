export interface ProjectUrls {
  repo: string;
  issues: string;
  license: string;
  author: string;
  sponsors: string;
}

export function getProjectUrls(): ProjectUrls {
  const repo =
    import.meta.env.VITE_GITHUB_REPO ?? "https://github.com/amkherad/avar";
  const author =
    import.meta.env.VITE_GITHUB_AUTHOR ?? "https://github.com/amkherad";
  const sponsors =
    import.meta.env.VITE_GITHUB_SPONSORS ?? "https://github.com/sponsors/amkherad";

  return {
    repo,
    issues: `${repo}/issues/new`,
    license: `${repo}#license`,
    author,
    sponsors,
  };
}
