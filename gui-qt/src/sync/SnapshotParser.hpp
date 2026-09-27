#pragma once

#include "api/DaemonTypes.hpp"

#include <QJsonValue>
#include <optional>

namespace avar::gui {

[[nodiscard]] bool isUnchangedStreamPayload(const QJsonValue &raw);

[[nodiscard]] std::optional<SystemStatsInfo> parseStreamStatsPayload(const QJsonValue &raw);

[[nodiscard]] std::optional<SnapshotPayload> parseSnapshotPayload(const QJsonValue &raw);

[[nodiscard]] DownloadInfo parseDownloadItem(const QJsonValue &item);
[[nodiscard]] QueueInfo parseQueueRecord(const QJsonValue &item);

} // namespace avar::gui
