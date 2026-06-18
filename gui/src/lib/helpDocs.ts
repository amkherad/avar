import type { LocaleId } from "@/config/defaults";

export interface HelpTopic {
  id: string;
  titleKey: string;
  file: string;
}

export const HELP_TOPICS: HelpTopic[] = [
  { id: "index", titleKey: "help.topics.index", file: "index.md" },
  { id: "downloads", titleKey: "help.topics.downloads", file: "downloads.md" },
  { id: "queues", titleKey: "help.topics.queues", file: "queues.md" },
  { id: "sessions", titleKey: "help.topics.sessions", file: "sessions.md" },
  { id: "settings", titleKey: "help.topics.settings", file: "settings.md" },
  { id: "shortcuts", titleKey: "help.topics.shortcuts", file: "shortcuts.md" },
  { id: "console", titleKey: "help.topics.console", file: "console.md" },
  { id: "layout", titleKey: "help.topics.layout", file: "layout.md" },
];

const docModules = import.meta.glob("../../docs/**/*.md", {
  query: "?raw",
  import: "default",
  eager: true,
}) as Record<string, string>;

export function getHelpContent(locale: LocaleId, file: string): string | null {
  const path = `../../docs/${locale}/${file}`;
  return docModules[path] ?? docModules[`../../docs/en/${file}`] ?? null;
}
