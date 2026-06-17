---
name: gui-code-style
description: TypeScript and React code style for the Avar GUI in gui/. Apply when editing .ts, .tsx, .css, or gui/ package files — not when editing C sources in src/ or include/.
---

# GUI Code Style

Apply to all files under `gui/`. For C code, use the **code-style** skill instead.

## TypeScript

- Strict mode; no `any` unless unavoidable (document why).
- Prefer `interface` for object shapes; `type` for unions and aliases.
- Named exports for components and utilities.
- File names: `PascalCase.tsx` for components, `camelCase.ts` for hooks/utils.
- Path alias: `@/` → `src/`.

## React

- Function components only.
- Props: explicit `interface XxxProps` exported from the same file.
- Hooks in `src/hooks/`; one concern per hook.
- Context providers in feature folders (`config/`, `theme/`).
- Avoid `useEffect` for data that can be derived; prefer `useMemo`.
- Event handlers: `onClick={() => void handler()}` for async functions.

## CSS

- Global tokens in `theme/global.css` and `theme/themes.ts`.
- Component layout/styles in `theme/components.css` using `avar-*` BEM-like classes.
- Use CSS variables (`var(--avar-primary)`); no raw hex in component files.
- Prefer logical properties for RTL: `margin-inline`, `padding-inline`, `border-inline-end`.

## i18n

- Keys grouped by feature: `session.*`, `queue.*`, `download.*`.
- Use `useTranslation()` in components; never hard-code user-visible English in JSX.
- Add keys to every supported locale file when adding UI text.

## Imports

Order:

1. React / external packages
2. `@/` absolute imports
3. Relative imports (avoid when `@/` works)

## Formatting

- 2-space indent
- Double quotes for strings
- Trailing commas in multiline objects/arrays
- Max line length ~100 (soft)

## Do not

- Put business logic in `main.tsx` or `App.tsx` beyond routing/shell wiring
- Import from `src/` C tree or `third_party/`
- Add CSS-in-JS libraries without team approval
- Use default exports except for `i18n/index.ts` and Electron entry if required by tooling
