import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/ui/Button";
import { useConfigStore } from "@/stores/configStore";
import { appLogger } from "@/lib/appLogger";
import {
  defaultShortcutMap,
  formatCombo,
  SHORTCUT_DEFINITIONS,
  type ShortcutActionId,
} from "@/shortcuts/definitions";

export function ShortcutsSettings() {
  const { t } = useTranslation();
  const shortcuts = useConfigStore((s) => s.config.shortcuts);
  const updateConfig = useConfigStore((s) => s.updateConfig);

  const rows = useMemo(() => {
    const list: Array<
      | { type: "category"; key: string }
      | { type: "shortcut"; id: ShortcutActionId; labelKey: string }
    > = [];

    const categories = new Map<string, typeof SHORTCUT_DEFINITIONS>();
    for (const def of SHORTCUT_DEFINITIONS) {
      const group = categories.get(def.categoryKey) ?? [];
      group.push(def);
      categories.set(def.categoryKey, group);
    }

    for (const [categoryKey, defs] of categories) {
      list.push({ type: "category", key: categoryKey });
      for (const def of defs) {
        list.push({ type: "shortcut", id: def.id, labelKey: def.labelKey });
      }
    }

    return list;
  }, []);

  function handleRecord(id: ShortcutActionId) {
    appLogger.gui.debug("Shortcut rebind started", id);
    function onKeyDown(event: KeyboardEvent) {
      event.preventDefault();
      event.stopPropagation();

      if (event.key === "Escape") {
        appLogger.gui.debug("Shortcut rebind cancelled", id);
        window.removeEventListener("keydown", onKeyDown, true);
        return;
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
      }

      if (key === "control" || key === "shift" || key === "alt" || key === "meta") {
        return;
      }

      parts.push(key);
      const combo = parts.join("+");

      updateConfig({
        shortcuts: { ...shortcuts, [id]: combo },
      });
      appLogger.gui.debug("Shortcut rebound", `${id} -> ${combo}`);
      window.removeEventListener("keydown", onKeyDown, true);
    }

    window.addEventListener("keydown", onKeyDown, true);
  }

  function resetAll() {
    appLogger.gui.debug("Shortcuts reset to defaults");
    updateConfig({ shortcuts: defaultShortcutMap() });
  }

  return (
    <div className="avar-shortcuts-settings">
      <p className="avar-settings-hint">{t("shortcuts.hint")}</p>

      <div className="avar-shortcuts-settings__table-block">
        <div className="avar-data-table-wrap">
          <table className="avar-data-table avar-shortcuts-settings__table">
            <thead>
              <tr>
                <th scope="col">{t("shortcuts.columnAction")}</th>
                <th scope="col">{t("shortcuts.columnKeys")}</th>
              </tr>
            </thead>
            <tbody>
              {rows.map((row) =>
                row.type === "category" ? (
                  <tr key={`cat-${row.key}`} className="avar-shortcuts-settings__category-row">
                    <th colSpan={2} scope="rowgroup">
                      {t(row.key)}
                    </th>
                  </tr>
                ) : (
                  <tr key={row.id}>
                    <td>{t(row.labelKey)}</td>
                    <td>
                      <Button
                        size="sm"
                        variant="secondary"
                        className="avar-shortcuts-settings__combo"
                        onClick={() => handleRecord(row.id)}
                      >
                        {formatCombo(shortcuts[row.id])}
                      </Button>
                    </td>
                  </tr>
                ),
              )}
            </tbody>
          </table>
        </div>

        <Button
          size="sm"
          variant="secondary"
          className="avar-shortcuts-settings__reset"
          onClick={resetAll}
        >
          {t("shortcuts.resetAll")}
        </Button>
      </div>
    </div>
  );
}
