#pragma once

class QApplication;

namespace avar::gui {

class Translator;

void applyApplicationLocale(QApplication &app, const Translator &translator);

} // namespace avar::gui
