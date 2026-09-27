#pragma once

#include <QHash>
#include <QObject>
#include <QString>

namespace avar::gui {

class Translator final : public QObject {
    Q_OBJECT

public:
    explicit Translator(QObject *parent = nullptr);

    void setLocale(const QString &locale);
    [[nodiscard]] QString locale() const;
    [[nodiscard]] QString tr(const QString &key) const;
    [[nodiscard]] bool isRtl() const;

signals:
    void translationsChanged();

private:
    void reloadStrings();

    QString m_locale = QStringLiteral("en");
    QHash<QString, QString> m_strings;
};

} // namespace avar::gui
