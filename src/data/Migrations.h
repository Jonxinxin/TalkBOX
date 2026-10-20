#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace talkbox::migrations {

/// Schema version the application expects after migrations ran.
inline constexpr int kTargetVersion = 3;

/// Brings the database from its current PRAGMA user_version up to
/// kTargetVersion. Every migration runs inside its own transaction; on
/// failure the transaction is rolled back, errorMessage is set and the
/// caller must not continue with a partially migrated database.
bool apply(QSqlDatabase &database, QString *errorMessage);

} // namespace talkbox::migrations
