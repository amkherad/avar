#pragma once

namespace avar::gui {

enum class HostingMode {
    Desktop,
    Wasm,
};

HostingMode detectHostingMode();

bool hostingSupportsExtensionSubprocess(HostingMode mode);
bool hostingUsesRelativeDaemonApi(HostingMode mode);

} // namespace avar::gui
