import { useMemo } from "react";
import { useTranslation } from "react-i18next";
import { Button } from "@/components/ui/Button";
import { useConfigStore } from "@/stores/configStore";
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

  const grouped = useMemo(() => {
    const map = new Map<string, typeof SHORTCUT_DEFINITIONS>();
    for (const def of SHORTCUT_DEFINITIONS) {
      const list = map.get(def.categoryKey) ?? [];
      list.push(def);
      map.set(def.categoryKey, list);
    }
    return map;
  }, []);

  function handleRecord(id: ShortcutActionId) {
    function onKeyDown(event: KeyboardEvent) {
      event.preventDefault();
      event.stopPropagation();

      if (event.key === "Escape") {
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
      window.removeEventListener("keydown", onKeyDown, true);
    }

    window.addEventListener("keydown", onKeyDown, true);
  }

  function resetAll() {
    updateConfig({ shortcuts: defaultShortcutMap() });
  }

  return (
    <div className="avar-shortcuts-settings">
      <p className="avar-settings-hint">{t("shortcuts.hint")}</p>

      {[...grouped.entries()].map(([categoryKey, defs]) => (
        <section key={categoryKey} className="avar-shortcuts-settings__group">
          <h3 className="avar-shortcuts-settings__heading">{t(categoryKey)}</h3>
          <ul className="avar-shortcuts-settings__list">
            {defs.map((def) => (
              <li key={def.id} className="avar-shortcuts-settings__item">
                <span className="avar-shortcuts-settings__label">{t(def.labelKey)}</span>
                <Button
                  size="sm"
                  variant="secondary"
                  className="avar-shortcuts-settings__combo"
                  onClick={() => handleRecord(def.id)}
                >
                  {formatCombo(shortcuts[def.id])}
                </Button>
              </li>
            ))}
          </ul>
        </section>
      ))}

      <Button size="sm" variant="ghost" onClick={resetAll}>
        {t("shortcuts.resetAll")}
      </Button>
    </div>
  );
}
