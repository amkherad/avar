#pragma once

#include "api/DaemonTypes.hpp"

#include <QWidget>

class QLabel;

namespace avar::gui {

class Translator;

class DownloadDetailPanelWidget final : public QWidget {
    Q_OBJECT

public:
    explicit DownloadDetailPanelWidget(Translator &translator, QWidget *parent = nullptr);

    void setDownload(const DownloadInfo &download, bool valid);

private:
    Translator &m_tr;
    QLabel *m_title = nullptr;
    QLabel *m_body = nullptr;
};

} // namespace avar::gui
