#pragma once

#include <QObject>
#include <QString>

namespace talkbox {

/// Read-only application information for the UI (data locations,
/// version) plus small shell helpers.
class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString dataDirectory() const;
    QString databasePath() const;
    QString logDirectory() const;
    QString appVersion() const;

    Q_INVOKABLE bool openDataDirectory() const;
    Q_INVOKABLE bool openLogDirectory() const;

    /// Window geometry and pin state (UI preference, stored in
    /// <dataDir>/ui.ini). Returns an empty map when nothing was saved yet.
    Q_INVOKABLE QVariantMap loadWindowState() const;
    Q_INVOKABLE void saveWindowState(int x, int y, int width, int height, bool pinned) const;
};

} // namespace talkbox
