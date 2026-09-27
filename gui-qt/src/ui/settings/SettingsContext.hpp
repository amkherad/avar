#pragma once

namespace avar::gui {

class AppSettings;
class DaemonClient;
class ExtensionBridgeClient;
class GuiPreferences;
class SessionManager;
class Translator;

struct SettingsContext {
    Translator &translator;
    AppSettings &appSettings;
    GuiPreferences &guiPreferences;
    DaemonClient &daemon;
    SessionManager &sessions;
    ExtensionBridgeClient &extension;
};

} // namespace avar::gui
