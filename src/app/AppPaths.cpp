#include "app/AppPaths.h"

#include <QDir>
#include <QObject>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace talkbox {

namespace {

QString resolveDataDir()
{
    const QString override =
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("TALKBOX_DATA_DIR"));
    if (!override.trimmed().isEmpty())
        return QDir::cleanPath(override.trimmed());
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

} // namespace

QString AppPaths::dataDir()
{
    return resolveDataDir();
}

QString AppPaths::databaseFile()
{
    return dataDir() + QStringLiteral("/talkbox.db");
}

QString AppPaths::attachmentsDir()
{
    return dataDir() + QStringLiteral("/attachments");
}

QString AppPaths::backupsDir()
{
    return dataDir() + QStringLiteral("/backups");
}

QString AppPaths::logsDir()
{
    return dataDir() + QStringLiteral("/logs");
}

bool AppPaths::ensureDirectories(QString *errorMessage)
{
    const QStringList dirs{dataDir(), attachmentsDir(), backupsDir(), logsDir()};
    for (const QString &dir : dirs) {
        if (!QDir().mkpath(dir)) {
            if (errorMessage) {
                *errorMessage =
                    QObject::tr("无法创建应用数据目录：%1").arg(QDir::toNativeSeparators(dir));
            }
            return false;
        }
    }
    return true;
}

} // namespace talkbox
