#include "services/AppInfo.h"

#include "app/AppPaths.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

namespace talkbox {

namespace {

/// UI preferences live next to the data so everything stays in one place.
QSettings uiSettings()
{
    return QSettings(AppPaths::dataDir() + QStringLiteral("/ui.ini"), QSettings::IniFormat);
}

constexpr auto kAutoSendKey = "drag/autoSendAfterDrag";

} // namespace

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
    , m_autoSendAfterDrag(
          uiSettings().value(QString::fromLatin1(kAutoSendKey), false).toBool())
{
}

QString AppInfo::dataDirectory() const
{
    return AppPaths::dataDir();
}

QString AppInfo::databasePath() const
{
    return AppPaths::databaseFile();
}

QString AppInfo::logDirectory() const
{
    return AppPaths::logsDir();
}

QString AppInfo::appVersion() const
{
    return QCoreApplication::applicationVersion();
}

bool AppInfo::openDataDirectory() const
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::dataDir()));
}

bool AppInfo::openLogDirectory() const
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::logsDir()));
}

bool AppInfo::autoSendAfterDrag() const
{
    return m_autoSendAfterDrag;
}

void AppInfo::setAutoSendAfterDrag(bool enabled)
{
    if (m_autoSendAfterDrag == enabled)
        return;

    m_autoSendAfterDrag = enabled;
    QSettings settings = uiSettings();
    settings.setValue(QString::fromLatin1(kAutoSendKey), enabled);
    settings.sync();
    emit autoSendAfterDragChanged();
}

QVariantMap AppInfo::loadWindowState() const
{
    QSettings settings = uiSettings();
    if (!settings.contains(QStringLiteral("window/width")))
        return {};

    QVariantMap state;
    state.insert(QStringLiteral("x"), settings.value(QStringLiteral("window/x"), -1).toInt());
    state.insert(QStringLiteral("y"), settings.value(QStringLiteral("window/y"), -1).toInt());
    state.insert(QStringLiteral("width"), settings.value(QStringLiteral("window/width")).toInt());
    state.insert(QStringLiteral("height"), settings.value(QStringLiteral("window/height")).toInt());
    state.insert(QStringLiteral("pinned"),
                 settings.value(QStringLiteral("window/pinned"), false).toBool());
    return state;
}

void AppInfo::saveWindowState(int x, int y, int width, int height, bool pinned) const
{
    QSettings settings = uiSettings();
    settings.setValue(QStringLiteral("window/x"), x);
    settings.setValue(QStringLiteral("window/y"), y);
    settings.setValue(QStringLiteral("window/width"), width);
    settings.setValue(QStringLiteral("window/height"), height);
    settings.setValue(QStringLiteral("window/pinned"), pinned);
}

} // namespace talkbox
