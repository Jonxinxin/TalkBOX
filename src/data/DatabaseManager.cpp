#include "data/DatabaseManager.h"

#include "data/Migrations.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace talkbox {

DatabaseManager::DatabaseManager(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

QString DatabaseManager::defaultConnectionName()
{
    return QStringLiteral("talkbox_main");
}

bool DatabaseManager::open(const QString &databaseFilePath, QString *errorMessage)
{
    if (m_isOpen) {
        if (m_filePath == databaseFilePath)
            return true;
        if (errorMessage) {
            *errorMessage = QObject::tr("数据库连接 %1 已在使用中。").arg(m_connectionName);
        }
        return false;
    }

    // A previous connection under this name would make addDatabase() fail.
    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);

    const QString directoryPath = QFileInfo(databaseFilePath).absolutePath();
    if (!QDir().mkpath(directoryPath)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法创建数据目录：%1")
                                .arg(QDir::toNativeSeparators(directoryPath));
        }
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(databaseFilePath);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法打开数据库 %1：%2")
                                .arg(QDir::toNativeSeparators(databaseFilePath),
                                     db.lastError().text());
        }
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    {
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
        pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"));
    }
    {
        QSqlQuery check(db);
        if (check.exec(QStringLiteral("PRAGMA foreign_keys")) && check.next()
            && !check.value(0).toBool()) {
            qWarning("SQLite foreign key enforcement could not be enabled.");
        }
    }

    if (!migrations::apply(db, errorMessage)) {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    m_filePath = databaseFilePath;
    m_isOpen = true;
    return true;
}

void DatabaseManager::close()
{
    if (!m_isOpen)
        return;
    {
        // Keep the copy scoped so removeDatabase() does not warn about
        // a connection still being in use.
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_isOpen = false;
    m_filePath.clear();
}

bool DatabaseManager::isOpen() const
{
    return m_isOpen;
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

QString DatabaseManager::filePath() const
{
    return m_filePath;
}

} // namespace talkbox
