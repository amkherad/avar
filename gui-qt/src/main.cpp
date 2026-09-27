#include "core/Application.hpp"
#include "ui/MainWindow.hpp"

int main(int argc, char *argv[])
{
    avar::gui::Application app(argc, argv);
    avar::gui::MainWindow window(app);
    window.show(); // shows DesktopShellWindow on desktop
    return QApplication::exec();
}
