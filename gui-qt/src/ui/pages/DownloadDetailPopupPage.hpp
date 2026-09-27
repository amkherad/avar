#pragma once

#include "api/DaemonTypes.hpp"

#include <QWidget>

namespace avar::gui {

class Translator;

class DownloadDetailPopupPage final : public QWidget {
    Q_OBJECT

public:
    DownloadDetailPopupPage(Translator &translator, QWidget *parent = nullptr);

    void setDownload(const DownloadInfo &info);

private:
    Translator &m_tr;
};

} // namespace avar::gui
