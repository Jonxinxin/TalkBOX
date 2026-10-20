#include "app/AppPaths.h"
#include "app/Logging.h"
#include "data/CategoryRepository.h"
#include "data/DatabaseManager.h"
#include "data/SnippetRepository.h"
#include "models/CategoryListModel.h"
#include "models/SnippetListModel.h"
#include "platform/WindowChrome.h"
#include "services/AppInfo.h"
#include "services/AttachmentService.h"
#include "services/BackupService.h"
#include "services/CategoryService.h"
#include "services/ClipboardWriter.h"
#include "services/SnippetService.h"
#include "services/SystemService.h"

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QWindow>

using namespace talkbox;

namespace {

/// Shows a minimal fallback window with the given error (used when the
/// database or the data directories cannot be initialized). Never opens a
/// blank main window on a broken installation.
int showStartupError(const QString &message)
{
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("startupErrorMessage"), message);
    engine.loadFromModule("TalkBox", "StartupError");
    if (engine.rootObjects().isEmpty())
        return 1;
    return QCoreApplication::exec();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("TalkBOX"));
    QCoreApplication::setApplicationVersion(QStringLiteral(TALKBOX_VERSION));
    // Taskbar / Alt+Tab icon (the caption icon is drawn by the QML title bar).
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/TalkBox/icons/app.svg")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppInfo appInfo;
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "AppInfo", &appInfo);

    QString errorMessage;
    if (!AppPaths::ensureDirectories(&errorMessage)) {
        qCritical().noquote() << "Failed to create application directories:"
                              << errorMessage;
        return showStartupError(errorMessage);
    }

    logging::init(AppPaths::logsDir());
    qInfo().noquote() << "TalkBOX" << QCoreApplication::applicationVersion()
                      << "starting; data dir:" << AppPaths::dataDir();

    DatabaseManager database(DatabaseManager::defaultConnectionName());
    if (!database.open(AppPaths::databaseFile(), &errorMessage)) {
        qCritical().noquote() << "Database initialization failed:" << errorMessage;
        return showStartupError(errorMessage);
    }
    qInfo().noquote() << "Database ready:" << database.filePath();

    CategoryRepository categoryRepository(database);
    SnippetRepository snippetRepository(database);
    SnippetListModel snippetsModel(snippetRepository);
    CategoryListModel categoriesModel(categoryRepository);
    SystemClipboardWriter clipboard;
    AttachmentService attachmentService;
    SystemService systemService;
    SnippetService snippetService(snippetRepository, clipboard, attachmentService);
    CategoryService categoryService(categoryRepository);
    BackupService backupService(database);

    QObject::connect(&snippetService, &SnippetService::snippetsChanged, &snippetsModel,
                     &SnippetListModel::refresh);
    // Category changes affect both the category chips and the category
    // labels rendered on the cards.
    QObject::connect(&categoryService, &CategoryService::categoriesChanged, &categoriesModel,
                     &CategoryListModel::refresh);
    QObject::connect(&categoryService, &CategoryService::categoriesChanged, &snippetsModel,
                     &SnippetListModel::refresh);
    // A successful restore replaced the database contents; reload everything.
    QObject::connect(&backupService, &BackupService::restoreFinished, &categoriesModel,
                     [&categoriesModel, &snippetsModel](bool ok) {
                         if (!ok)
                             return;
                         categoriesModel.refresh();
                         snippetsModel.refresh();
                     });

    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Snippets", &snippetsModel);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Categories", &categoriesModel);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "SnippetService", &snippetService);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Attachments", &attachmentService);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "SystemService", &systemService);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "CategoryService", &categoryService);
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "BackupService", &backupService);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);

    engine.loadFromModule("TalkBox", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    // Custom title bar on top of the native frame. The window is created
    // hidden (visible: false in Main.qml), so the frame is prepared before the
    // first frame is shown: no system caption flash, no geometry jump.
    WindowChrome chrome;
    chrome.setBorderColor(QColor(QStringLiteral("#DDE5F0")));
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
        if (!chrome.attach(window))
            qWarning().noquote() << "Custom window chrome could not be attached";
        window->setVisible(true);
        window->requestActivate();
    }

    const int exitCode = app.exec();
    qInfo().noquote() << "TalkBOX exiting with code" << exitCode;
    return exitCode;
}
