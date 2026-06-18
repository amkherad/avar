#!/usr/bin/env python3
"""Embeds gui/dist static assets into a generated C source file."""

from __future__ import annotations

import argparse
import mimetypes
import os
from pathlib import Path


def guess_mime(path: Path) -> str:
    mime, _ = mimetypes.guess_type(path.name)
    if mime:
        return mime
    if path.suffix == ".js":
        return "application/javascript"
    if path.suffix == ".css":
        return "text/css"
    if path.suffix == ".svg":
        return "image/svg+xml"
    return "application/octet-stream"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dist", required=True, help="Path to gui/dist")
    parser.add_argument("--out-c", required=True, help="Output C file path")
    parser.add_argument("--out-h", required=True, help="Output header path")
    args = parser.parse_args()

    dist = Path(args.dist)
    if not dist.is_dir():
        raise SystemExit(f"dist directory not found: {dist}")

    entries: list[tuple[str, Path, str]] = []
    for file_path in sorted(dist.rglob("*")):
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(dist).as_posix()
        web_path = "/" if rel == "index.html" else f"/{rel}"
        entries.append((web_path, file_path, guess_mime(file_path)))

    out_c = Path(args.out_c)
    out_h = Path(args.out_h)
    out_c.parent.mkdir(parents=True, exist_ok=True)

    with out_c.open("w", encoding="utf-8", newline="\n") as fh:
        fh.write('#include "gui_embed.h"\n')
        fh.write('#include <mongoose.h>\n')
        fh.write('#include <string.h>\n\n')
        fh.write('typedef struct {\n')
        fh.write('    const char *path;\n')
        fh.write('    const char *mime;\n')
        fh.write('    const unsigned char *data;\n')
        fh.write('    size_t size;\n')
        fh.write('} GuiEmbedAsset;\n\n')

        for idx, (_, file_path, _) in enumerate(entries):
            data = file_path.read_bytes()
            fh.write(f'static const unsigned char gui_embed_blob_{idx}[] = {{\n')
            for i in range(0, len(data), 16):
                chunk = data[i : i + 16]
                line = ", ".join(f"0x{b:02x}" for b in chunk)
                fh.write(f"    {line},\n")
            fh.write("};\n\n")

        fh.write("static const GuiEmbedAsset gui_embed_assets[] = {\n")
        for idx, (web_path, _, mime) in enumerate(entries):
            fh.write(
                f'    {{"{web_path}", "{mime}", gui_embed_blob_{idx}, '
                f"sizeof gui_embed_blob_{idx}}},\n"
            )
        fh.write("};\n\n")
        fh.write("static const size_t gui_embed_asset_count = sizeof gui_embed_assets / sizeof gui_embed_assets[0];\n\n")

        fh.write("bool gui_embed_available(void) {\n")
        fh.write("    return gui_embed_asset_count > 0U;\n")
        fh.write("}\n\n")

        fh.write("static const GuiEmbedAsset *gui_embed_find(const char *uri) {\n")
        fh.write("    if (uri == NULL) {\n")
        fh.write('        return NULL;\n')
        fh.write("    }\n")
        fh.write('    if (strcmp(uri, "/") == 0) {\n')
        fh.write('        uri = "/index.html";\n')
        fh.write("    }\n")
        fh.write("    for (size_t i = 0; i < gui_embed_asset_count; ++i) {\n")
        fh.write("        if (strcmp(gui_embed_assets[i].path, uri) == 0) {\n")
        fh.write("            return &gui_embed_assets[i];\n")
        fh.write("        }\n")
        fh.write("    }\n")
        fh.write("    return NULL;\n")
        fh.write("}\n\n")

        fh.write("bool gui_embed_serve(struct mg_connection *connection, const char *uri) {\n")
        fh.write("    const GuiEmbedAsset *asset = gui_embed_find(uri);\n")
        fh.write("    if (asset == NULL || connection == NULL) {\n")
        fh.write("        return false;\n")
        fh.write("    }\n")
        fh.write("    mg_printf(connection,\n")
        fh.write('              "HTTP/1.1 200 OK\\r\\n"\n')
        fh.write('              "Content-Type: %s\\r\\n"\n')
        fh.write('              "Content-Length: %zu\\r\\n"\n')
        fh.write('              "\\r\\n",\n')
        fh.write("              asset->mime, asset->size);\n")
        fh.write("    mg_send(connection, asset->data, asset->size);\n")
        fh.write("    return true;\n")
        fh.write("}\n")

    with out_h.open("w", encoding="utf-8", newline="\n") as fh:
        fh.write("#ifndef AVAR_GUI_EMBED_GENERATED_H\n")
        fh.write("#define AVAR_GUI_EMBED_GENERATED_H\n")
        fh.write(f"/* Generated from {dist.as_posix()} with {len(entries)} assets. */\n")
        fh.write("#endif\n")

    print(f"Embedded {len(entries)} files into {out_c}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
