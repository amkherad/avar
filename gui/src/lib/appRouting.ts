import type { AppPage } from "@/components/layout/Sidebar";
import type { SettingsCategory } from "@/pages/SettingsPage";
import { HELP_TOPICS } from "@/lib/helpDocs";

export type DashboardAction = "add-download";

export interface AppLocation {
  page: AppPage;
  settingsCategory: SettingsCategory;
  helpTopicId: string;
  dashboardAction: DashboardAction | null;
}

const SETTINGS_CATEGORIES = new Set<SettingsCategory>([
  "general",
  "downloads",
  "queues",
  "daemon",
  "browser",
  "shortcuts",
]);

const HELP_TOPIC_IDS = new Set(HELP_TOPICS.map((topic) => topic.id));

const DEFAULT_APP_LOCATION: AppLocation = {
  page: "dashboard",
  settingsCategory: "general",
  helpTopicId: HELP_TOPICS[0].id,
  dashboardAction: null,
};

export function defaultAppLocation(): AppLocation {
  return DEFAULT_APP_LOCATION;
}

let cachedLocationHash: string | null = null;
let cachedLocation: AppLocation = DEFAULT_APP_LOCATION;

function parseSettingsCategory(value: string | undefined): SettingsCategory {
  if (value && SETTINGS_CATEGORIES.has(value as SettingsCategory)) {
    return value as SettingsCategory;
  }
  return "general";
}

function parseHelpTopicId(value: string | undefined): string {
  if (value && HELP_TOPIC_IDS.has(value)) {
    return value;
  }
  return HELP_TOPICS[0].id;
}

export function parseAppHash(hash: string): AppLocation | null {
  const trimmed = hash.replace(/^#/, "").replace(/^\//, "");
  if (!trimmed || trimmed.startsWith("popup/")) {
    return null;
  }

  const segments = trimmed.split("/").filter(Boolean);
  const root = segments[0] ?? "dashboard";

  if (root === "settings") {
    return {
      page: "settings",
      settingsCategory: parseSettingsCategory(segments[1]),
      helpTopicId: HELP_TOPICS[0].id,
      dashboardAction: null,
    };
  }

  if (root === "help") {
    return {
      page: "help",
      settingsCategory: "general",
      helpTopicId: parseHelpTopicId(segments[1]),
      dashboardAction: null,
    };
  }

  if (root === "dashboard") {
    const action = segments[1] === "add-download" ? "add-download" : null;
    return {
      page: "dashboard",
      settingsCategory: "general",
      helpTopicId: HELP_TOPICS[0].id,
      dashboardAction: action,
    };
  }

  return DEFAULT_APP_LOCATION;
}

export function readAppLocation(): AppLocation {
  const hash = window.location.hash;
  if (hash === cachedLocationHash) {
    return cachedLocation;
  }

  cachedLocationHash = hash;
  cachedLocation = parseAppHash(hash) ?? DEFAULT_APP_LOCATION;
  return cachedLocation;
}

export function buildAppHash(location: AppLocation): string {
  switch (location.page) {
    case "settings":
      return location.settingsCategory === "general"
        ? "#/settings"
        : `#/settings/${location.settingsCategory}`;
    case "help":
      return location.helpTopicId === HELP_TOPICS[0].id
        ? "#/help"
        : `#/help/${location.helpTopicId}`;
    default:
      return location.dashboardAction === "add-download"
        ? "#/dashboard/add-download"
        : "#/dashboard";
  }
}

export function navigateAppLocation(location: AppLocation, replace = false): void {
  const hash = buildAppHash(location);
  if (window.location.hash === hash) {
    return;
  }

  const url = `${window.location.pathname}${window.location.search}${hash}`;
  if (replace) {
    window.history.replaceState(window.history.state, "", url);
    window.dispatchEvent(new HashChangeEvent("hashchange"));
    return;
  }

  window.location.hash = hash;
}

export function subscribeAppLocation(listener: () => void): () => void {
  window.addEventListener("hashchange", listener);
  window.addEventListener("popstate", listener);
  return () => {
    window.removeEventListener("hashchange", listener);
    window.removeEventListener("popstate", listener);
  };
}
