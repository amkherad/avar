#pragma once

#include <QObject>
#include <QString>

namespace avar::gui {

class Translator final : public QObject {
    Q_OBJECT

public:
    explicit Translator(QObject *parent = nullptr);

    void setLocale(const QString &locale);
    [[nodiscard]] QString tr(const QString &key) const;
    [[nodiscard]] bool isRtl() const;

private:
    QString m_locale = QStringLiteral("en");
};

} // namespace avar::gui
