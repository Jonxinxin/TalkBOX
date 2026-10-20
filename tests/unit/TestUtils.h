#pragma once

#include "data/DatabaseManager.h"

#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace testutils {

/// Per-test environment: a temporary data directory plus an opened,
/// migrated database with a unique connection name so multiple managers
/// can coexist inside one test binary.
class TestEnv
{
public:
    TestEnv()
        : database(QUuid::createUuid().toString(QUuid::WithoutBraces))
    {
    }

    bool init(QString *errorMessage = nullptr)
    {
        if (!m_tempDir.isValid()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Temporary directory could not be created.");
            return false;
        }
        return database.open(m_tempDir.filePath(QStringLiteral("talkbox.db")), errorMessage);
    }

    QString databaseFilePath() const
    {
        return m_tempDir.filePath(QStringLiteral("talkbox.db"));
    }

    // Declared before `database` so the connection is closed before the
    // temporary directory is removed on Windows.
    QTemporaryDir m_tempDir;

    talkbox::DatabaseManager database;
};

/// Uniquely named connection for short-lived managers inside a test.
inline QString uniqueConnectionName()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace testutils
