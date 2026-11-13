#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    // The rest of the palette, so the window looks like the desktop around it.
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString warning READ warning NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    bool dark() const { return m_dark; }
    QString background() const { return m_colors.value("background"); }
    QString surface() const { return m_colors.value("surface"); }
    QString foreground() const { return m_colors.value("foreground"); }
    QString muted() const { return m_colors.value("muted"); }
    QString selection() const { return m_colors.value("selection"); }
    QString error() const { return m_colors.value("error"); }
    QString warning() const { return m_colors.value("warning"); }

    static QString readAccent(const QString &path);
    // Every `key = "#rrggbb"` in an Omarchy colors.toml, plus mode.
    static QHash<QString, QString> readColors(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    bool m_dark = true;
    QHash<QString, QString> m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
