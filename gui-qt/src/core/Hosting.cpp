#include "core/Hosting.hpp"

namespace avar::gui {

HostingMode detectHostingMode()
{
#if defined(AVAR_GUI_HOSTING_WASM)
    return HostingMode::Wasm;
#else
    return HostingMode::Desktop;
#endif
}

bool hostingSupportsExtensionSubprocess(HostingMode mode)
{
    return mode == HostingMode::Desktop;
}

bool hostingUsesRelativeDaemonApi(HostingMode mode)
{
    return mode == HostingMode::Wasm;
}

} // namespace avar::gui
