#pragma once

#include "theme/ThemeTokens.hpp"

class QString;

namespace avar::gui {

QString buildApplicationStylesheet(const ThemeTokens &tokens, const QString &baseTemplate);

} // namespace avar::gui
