export type ShortcutActionId =
  | "download.add"
  | "download.search"
  | "download.pause"
  | "download.start"
  | "download.stop"
  | "download.delete"
  | "nav.dashboard"
  | "nav.settings"
  | "nav.help"
  | "console.toggle"
  | "detailPanel.toggle";

export interface ShortcutDefinition {
  id: ShortcutActionId;
  defaultCombo: string;
  labelKey: string;
  categoryKey: string;
}

export const SHORTCUT_DEFINITIONS: ShortcutDefinition[] = [
  {
    id: "download.add",
    defaultCombo: "ctrl+n",
    labelKey: "shortcuts.actions.downloadAdd",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "download.search",
    defaultCombo: "ctrl+f",
    labelKey: "shortcuts.actions.downloadSearch",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "download.pause",
    defaultCombo: "ctrl+p",
    labelKey: "shortcuts.actions.downloadPause",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "download.start",
    defaultCombo: "ctrl+shift+s",
    labelKey: "shortcuts.actions.downloadStart",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "download.stop",
    defaultCombo: "ctrl+shift+x",
    labelKey: "shortcuts.actions.downloadStop",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "download.delete",
    defaultCombo: "delete",
    labelKey: "shortcuts.actions.downloadDelete",
    categoryKey: "shortcuts.category.downloads",
  },
  {
    id: "nav.dashboard",
    defaultCombo: "ctrl+1",
    labelKey: "shortcuts.actions.navDashboard",
    categoryKey: "shortcuts.category.navigation",
  },
  {
    id: "nav.settings",
    defaultCombo: "ctrl+,",
    labelKey: "shortcuts.actions.navSettings",
    categoryKey: "shortcuts.category.navigation",
  },
  {
    id: "nav.help",
    defaultCombo: "f1",
    labelKey: "shortcuts.actions.navHelp",
    categoryKey: "shortcuts.category.navigation",
  },
  {
    id: "console.toggle",
    defaultCombo: "ctrl+`",
    labelKey: "shortcuts.actions.consoleToggle",
    categoryKey: "shortcuts.category.view",
  },
  {
    id: "detailPanel.toggle",
    defaultCombo: "ctrl+d",
    labelKey: "shortcuts.actions.detailPanelToggle",
    categoryKey: "shortcuts.category.view",
  },
];

export function defaultShortcutMap(): Record<ShortcutActionId, string> {
  const map = {} as Record<ShortcutActionId, string>;
  for (const def of SHORTCUT_DEFINITIONS) {
    map[def.id] = def.defaultCombo;
  }
  return map;
}

export function normalizeCombo(event: KeyboardEvent): string | null {
  if (event.key === "Control" || event.key === "Shift" || event.key === "Alt" || event.key === "Meta") {
    return null;
  }

  const parts: string[] = [];
  if (event.ctrlKey || event.metaKey) {
    parts.push("ctrl");
  }
  if (event.shiftKey) {
    parts.push("shift");
  }
  if (event.altKey) {
    parts.push("alt");
  }

  let key = event.key.toLowerCase();
  if (key === " ") {
    key = "space";
  } else if (key.length === 1) {
    key = key;
  } else if (key === "escape") {
    key = "esc";
  }

  parts.push(key);
  return parts.join("+");
}

export function formatCombo(combo: string): string {
  return combo
    .split("+")
    .map((part) => {
      switch (part) {
        case "ctrl":
          return "Ctrl";
        case "shift":
          return "Shift";
        case "alt":
          return "Alt";
        case "meta":
          return "Meta";
        case "delete":
          return "Del";
        case "esc":
          return "Esc";
        default:
          return part.length === 1 ? part.toUpperCase() : part;
      }
    })
    .join(" + ");
}

export function isEditableTarget(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) {
    return false;
  }
  const tag = target.tagName;
  return (
    tag === "INPUT" ||
    tag === "TEXTAREA" ||
    tag === "SELECT" ||
    target.isContentEditable
  );
}
