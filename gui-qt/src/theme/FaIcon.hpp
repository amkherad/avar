#pragma once

class QColor;
class QIcon;
class QString;

namespace avar::gui {

/** Font Awesome solid icons from `:/fa/solid/<name>.svg` (see gui-qt/third_party/fontawesome-free). */
namespace FaIcon {

struct SolidIconOptions {
    /** Flip the glyph horizontally (e.g. arrow-left → back arrow in RTL). */
    bool mirrorHorizontally = false;
};

[[nodiscard]] QIcon solid(const QString &iconName,
                            int pixelSize,
                            const QColor &color,
                            SolidIconOptions options = {});

/** Navigation “back” chevron: points toward the layout start edge (mirrors in RTL). */
[[nodiscard]] QIcon solidBack(int pixelSize, const QColor &color, bool layoutRtl);

} // namespace FaIcon

} // namespace avar::gui
