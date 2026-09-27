#include "backend/InMemoryRpc.hpp"

#include <daemon/daemon_embed.h>

#include <cstdlib>

namespace avar::gui {

bool inMemoryRpcRequest(const QByteArray &requestJson, QByteArray *responseJsonOut, QString *errorOut)
{
    if (responseJsonOut == nullptr) {
        return false;
    }
    char *response = nullptr;
    if (!daemon_embed_rpc(requestJson.constData(), &response)) {
        if (errorOut != nullptr) {
            *errorOut = QStringLiteral("Embedded daemon is not running");
        }
        return false;
    }
    if (response == nullptr) {
        if (errorOut != nullptr) {
            *errorOut = QStringLiteral("Empty RPC response");
        }
        return false;
    }
    *responseJsonOut = QByteArray(response);
    free(response);
    return true;
}

} // namespace avar::gui
