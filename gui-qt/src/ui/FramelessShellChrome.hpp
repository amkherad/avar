#pragma once

#include <QList>

#include <functional>

class QMainWindow;
class QWidget;

namespace avar::gui {

[[nodiscard]] bool windowIsMaximized(const QMainWindow &window);
void polishDynamicFlag(QWidget *widget, const char *name, bool value);
/** Square, opaque client area when maximized (native-style); translucent inset when windowed. */
void applyFramelessPresentation(QMainWindow &window, bool maximized);

/** Rounded mask + edge resize grips for frameless desktop windows. */
class FramelessShellChrome final {
public:
    explicit FramelessShellChrome(QMainWindow &window);

    void setFrame(QWidget *frame);
    void setChromeRadius(int radiusPx);
    void setLayoutChangeCallback(std::function<void()> callback);
    void sync(bool maximized);
    void onWindowStateChanged();

private:
    void layoutGrips();
    void applyFrameMask(bool maximized);
    void ensureGrips();

    QMainWindow &m_window;
    QWidget *m_frame = nullptr;
    int m_radiusPx = 10;
    bool m_gripsBuilt = false;
    QList<QWidget *> m_grips;
    std::function<void()> m_layoutChangeCallback;
};

} // namespace avar::gui
