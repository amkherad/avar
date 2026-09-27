#include "ui/AvarWindow.hpp"

#include "ui/FramelessShellChrome.hpp"
#include "ui/widgets/HeaderWindowDrag.hpp"
#include "ui/widgets/WindowControls.hpp"

#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>
#include <QStyle>
#include <QVBoxLayout>

namespace avar::gui {

namespace {

void enableDesktopChrome(QWidget *widget)
{
    if (widget != nullptr) {
        widget->setAttribute(Qt::WA_StyledBackground, true);
    }
}

} // namespace

AvarWindow::~AvarWindow() = default;

AvarWindow::AvarWindow(QWidget *parent)
    : QMainWindow(parent)
{
#if !defined(AVAR_GUI_HOSTING_DESKTOP)
    setObjectName(QStringLiteral("AvarWindow"));
#endif
}

void AvarWindow::ensureChromeBuilt()
{
    if (m_chromeBuilt) {
        return;
    }
    m_chromeBuilt = true;

#if defined(AVAR_GUI_HOSTING_DESKTOP)
    setObjectName(QStringLiteral("AvarSecondaryWindow"));
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    auto *outer = new QWidget(this);
    outer->setObjectName(QStringLiteral("AvarDesktopOuter"));
    enableDesktopChrome(outer);
    auto *outerLayout = new QVBoxLayout(outer);
    outerLayout->setContentsMargins(kWindowedOuterMargin, kWindowedOuterMargin, kWindowedOuterMargin,
                                    kWindowedOuterMargin);
    outerLayout->setSpacing(0);

    m_frame = new QWidget(outer);
    m_frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    m_frame->setProperty("secondary", true);
    enableDesktopChrome(m_frame);
    auto *frameLayout = new QVBoxLayout(m_frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    auto *header = new QWidget(m_frame);
    header->setObjectName(QStringLiteral("AvarHeader"));
    header->setProperty("desktop", true);
    header->setProperty("secondary", true);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_titleLabel = new QLabel(windowTitle(), header);
    m_titleLabel->setObjectName(QStringLiteral("AvarHeaderTitle"));

    headerLayout->addWidget(m_titleLabel, 1);
    headerLayout->addWidget(new WindowControls(header));

    m_contentHost = new QWidget(m_frame);
    m_contentHost->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    frameLayout->addWidget(header);
    frameLayout->addWidget(m_contentHost, 1);

    outerLayout->addWidget(m_frame, 1);
    setCentralWidget(outer);

    m_headerDrag = new HeaderWindowDrag(header, this);
    m_chrome = std::make_unique<FramelessShellChrome>(*this);
    m_chrome->setFrame(m_frame);
    m_chrome->setLayoutChangeCallback([this] { updateShellChrome(); });
    updateShellChrome();
#else
    m_frame = new QWidget(this);
    m_frame->setObjectName(QStringLiteral("AvarDesktopFrame"));
    m_contentHost = m_frame;
    setCentralWidget(m_frame);
#endif
}

void AvarWindow::setContentWidget(QWidget *widget)
{
    ensureChromeBuilt();

    auto *layout = qobject_cast<QVBoxLayout *>(m_contentHost->layout());
    if (layout == nullptr) {
        layout = new QVBoxLayout(m_contentHost);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }

    layout->addWidget(widget, 1);

    if (m_titleLabel != nullptr) {
        m_titleLabel->setText(windowTitle());
    }
}

void AvarWindow::updateShellChrome()
{
#if !defined(AVAR_GUI_HOSTING_DESKTOP)
    return;
#endif

    QWidget *outer = centralWidget();
    if (outer == nullptr) {
        return;
    }

    const bool maximized = windowIsMaximized(*this);
    const int margin = maximized ? 0 : kWindowedOuterMargin;

    applyFramelessPresentation(*this, maximized);

    if (QLayout *outerLayout = outer->layout()) {
        outerLayout->setContentsMargins(margin, margin, margin, margin);
    }

    polishDynamicFlag(outer, "maximized", maximized);
    polishDynamicFlag(m_frame, "maximized", maximized);

    QWidget *header = m_frame != nullptr ? m_frame->findChild<QWidget *>(QStringLiteral("AvarHeader")) : nullptr;
    polishDynamicFlag(header, "maximized", maximized);

    if (m_chrome != nullptr) {
        m_chrome->setChromeRadius(10);
        m_chrome->sync(maximized);
    }
}

void AvarWindow::centerOnScreen()
{
    QScreen *screen = nullptr;
    if (QWidget *parentWindow = parentWidget() != nullptr ? parentWidget()->window() : nullptr) {
        screen = parentWindow->screen();
    }
    if (screen == nullptr && windowHandle() != nullptr) {
        screen = windowHandle()->screen();
    }
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }

    const QRect available = screen->availableGeometry();
    const QSize windowSize = size().isValid() ? size() : QSize(480, 320);
    const int x = available.x() + (available.width() - windowSize.width()) / 2;
    const int y = available.y() + (available.height() - windowSize.height()) / 2;
    move(x, y);
}

void AvarWindow::showCentered()
{
    m_pendingCenter = true;
    show();
}

void AvarWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_pendingCenter) {
        m_pendingCenter = false;
        centerOnScreen();
    }
}

void AvarWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowTitleChange && m_titleLabel != nullptr) {
        m_titleLabel->setText(windowTitle());
    }
    if (event->type() == QEvent::WindowStateChange) {
        updateShellChrome();
    }
}

} // namespace avar::gui
