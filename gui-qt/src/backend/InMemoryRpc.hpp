#pragma once

#include <QByteArray>
#include <QString>

namespace avar::gui {

bool inMemoryRpcRequest(const QByteArray &requestJson, QByteArray *responseJsonOut, QString *errorOut);

} // namespace avar::gui
