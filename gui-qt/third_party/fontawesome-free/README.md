# Font Awesome Free (icons subset)

Solid SVG icons used by `avar-gui-qt`, aligned with `@fortawesome/free-solid-svg-icons` in the React GUI.

This directory currently ships a **minimal subset** (~5 KiB of SVGs) plus `LICENSE.txt`. For the full icon set, add the official package as a submodule:

```bash
git submodule add --depth 1 https://github.com/FortAwesome/Font-Awesome-Free.git gui-qt/third_party/fontawesome-free
```

**SVG vs PNG for Qt:** prefer **SVG** — one small vector file per icon, sharp at any DPI. PNG only makes sense for fixed-size raster artwork; a full icon font is smaller only when you use dozens of glyphs.

Icons are embedded via `resources/avar_gui.qrc` (paths under `svgs/solid/`).
