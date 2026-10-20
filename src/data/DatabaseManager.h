#pragma once

#include <QSqlDatabase>
#include <QString>

namespace talkbox {

/// Owns the SQLite connection and the schema migration lifecycle.
/// One instance maps to one named QSqlDatabase connection.
class DatabaseManager
{
public:
    explicit DatabaseManager(QString connectionName);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    /// Opens (creating if needed) the database file and runs migrations.
    /// Returns false and sets errorMessage on failure.
    bool open(const QString &databaseFilePath, QString *errorMessage);

    void close();

    bool isOpen() const;
    QSqlDatabase database() const;
    QString filePath() const;

    static QString defaultConnectionName();

private:
    QString m_connectionName;
    QString m_filePath;
    bool m_isOpen = false;
};

} // namespace talkbox
