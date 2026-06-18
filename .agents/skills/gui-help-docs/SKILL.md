---
name: gui-help-docs
description: Keep Avar GUI in-app help documentation in gui/docs/ synchronized with front-end features. Apply when adding, renaming, or removing user-visible GUI functionality, settings, shortcuts, or navigation.
---

# GUI Help Documentation

Apply when changing user-facing behavior under `gui/`.

## Location

```text
gui/docs/
├── en/          # English (required for every topic)
└── fa/          # Persian (add when UI is localized; falls back to en)
```

Each topic is one Markdown file. The index is `index.md`.

## Topic registry

Topics are listed in `gui/src/lib/helpDocs.ts` (`HELP_TOPICS`). When you add a feature area:

1. Add `en/<topic>.md` (and `fa/<topic>.md` when possible).
2. Register the topic in `HELP_TOPICS` with `id`, `titleKey`, and `file`.
3. Add `help.topics.<id>` keys to **all** locale JSON files (`en.json`, `fa.json`).

## Content rules

- Document **what the user sees and does**, not implementation details.
- Cover: location in the UI, primary actions, keyboard shortcuts, and settings that affect the feature.
- Use tables for shortcut lists and field descriptions.
- Keep English and Persian docs semantically equivalent (not word-for-word required).
- When removing a feature, delete or update the corresponding doc and remove the topic from `HELP_TOPICS`.

## Checklist (every GUI feature change)

- [ ] Help markdown updated in `gui/docs/en/` (and `fa/` if applicable)
- [ ] `HELP_TOPICS` in `helpDocs.ts` matches files on disk
- [ ] i18n `help.topics.*` keys added or removed
- [ ] Shortcuts doc matches `shortcuts/definitions.ts` defaults
- [ ] Settings doc lists new settings categories under **Settings**

## Related files

| File | Role |
|------|------|
| `gui/src/pages/HelpPage.tsx` | Renders docs |
| `gui/src/lib/helpDocs.ts` | Topic list + `import.meta.glob` loader |
| `gui/src/shortcuts/definitions.ts` | Shortcut defaults (sync with `shortcuts.md`) |
| `gui/src/i18n/locales/*.json` | Topic titles and UI strings |
