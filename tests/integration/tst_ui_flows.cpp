#if defined(_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "app/AppPaths.h"
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

#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

using namespace talkbox;

namespace {

/// While the secure (locked) desktop is active, the platform drives no
/// window frames and synthetic input cannot be delivered, so the lazily
/// created view delegates never appear. Skip the suite instead of
/// producing failures that say nothing about the application.
bool interactiveDesktopAccessible()
{
#if defined(_WIN32)
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
    if (!desktop)
        return false;
    CloseDesktop(desktop);
    return true;
#else
    return true;
#endif
}

} // namespace

/// UI integration tests: loads the real TalkBox QML module with real
/// services (pointed at a temporary data directory) and drives the
/// interface with synthetic input events. These run without desktop
/// interaction and cover the acceptance flows of PRD AT-04/08/10/11.
class TestUiFlows : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void imagePreviewCopiesImage();
    void menuClickDoesNotCopyButMenuItemDoes();
    void deleteFromMenuCancelKeepsAndConfirmRemoves();
    void gridUsesTwoColumns();
    void cardBodyClickCopiesContent();
    void rightClickOpensCardMenuWithoutCopying();
    void pdfCardBodyClickCopiesFile();
    void settingsCategoryManagementFlow();
    void noBuiltinCategoriesAndUncategorizedFilter();
    void clickOutsideClosesDialogs();
    void dragShowsContentFeedback();
    void typeFilterSitsAtWindowBottom();
    void cardBordersNotClipped();
    void titleBarMaximizesAndRestores();
    void startupShowsNoFocusRing();

private:
    QObject *object(const QString &name) const
    {
        return m_window ? m_window->findChild<QObject *>(name) : nullptr;
    }

    static QQuickItem *findItemInVisualTree(QQuickItem *root, const QString &name)
    {
        if (!root)
            return nullptr;
        if (root->objectName() == name)
            return root;
        const QList<QQuickItem *> children = root->childItems();
        for (QQuickItem *child : children) {
            if (QQuickItem *found = findItemInVisualTree(child, name))
                return found;
        }
        return nullptr;
    }

    static void collectItemsInVisualTree(QQuickItem *root, const QString &name,
                                         QList<QQuickItem *> &result)
    {
        if (!root)
            return;
        if (root->objectName() == name && root->isVisible())
            result.append(root);
        const QList<QQuickItem *> children = root->childItems();
        for (QQuickItem *child : children)
            collectItemsInVisualTree(child, name, result);
    }

    static QQuickItem *findVisibleItemInVisualTree(QQuickItem *root, const QString &name)
    {
        if (!root)
            return nullptr;
        if (root->objectName() == name && root->isVisible())
            return root;
        const QList<QQuickItem *> children = root->childItems();
        for (QQuickItem *child : children) {
            if (QQuickItem *found = findVisibleItemInVisualTree(child, name))
                return found;
        }
        return nullptr;
    }

    /// First matching item, visible or not (use waitForItem for clicks).
    QQuickItem *item(const QString &name) const
    {
        return m_window ? findItemInVisualTree(m_window->contentItem(), name) : nullptr;
    }

    /// All visible items with the given objectName, ordered top-left
    /// first (grid order).
    QList<QQuickItem *> items(const QString &name) const
    {
        QList<QQuickItem *> result;
        if (!m_window)
            return result;
        collectItemsInVisualTree(m_window->contentItem(), name, result);
        std::sort(result.begin(), result.end(), [](QQuickItem *a, QQuickItem *b) {
            const QPointF pa = a->mapToScene(QPointF(0, 0));
            const QPointF pb = b->mapToScene(QPointF(0, 0));
            if (!qFuzzyCompare(pa.y() + 1, pb.y() + 1))
                return pa.y() < pb.y();
            return pa.x() < pb.x();
        });
        return result;
    }

    static bool objectVisible(QObject *object)
    {
        return object && object->property("visible").toBool();
    }

    /// Some view operations force the item view to refill synchronously,
    /// creating delegates even when no compositor frame is driving the
    /// normal polish pipeline.
    void nudgeSnippetGrid()
    {
        auto *grid = m_window ? m_window->findChild<QQuickItem *>(QStringLiteral("snippetGrid"))
                              : nullptr;
        if (grid) {
            QMetaObject::invokeMethod(grid, "forceLayout");
            QMetaObject::invokeMethod(grid, "positionViewAtBeginning");
        }
        pumpRender();
    }

    void dumpTree()
    {
        auto *grid = m_window->findChild<QQuickItem *>(QStringLiteral("snippetGrid"));
        qInfo() << "dump: grid found:" << (grid != nullptr);
        if (grid) {
            qInfo() << "dump: grid visible:" << grid->isVisible() << "win:" << grid->width()
                    << grid->height() << "count:" << grid->property("count")
                    << "contentHeight:" << grid->property("contentHeight")
                    << "contentItem:" << grid->property("contentItem");
            if (auto *contentItem = grid->property("contentItem").value<QQuickItem *>())
                qInfo() << "dump: contentItem children:" << contentItem->childItems().size();
        }
        const QList<QObject *> all = m_window->findChildren<QObject *>();
        qInfo() << "dump: total QObjects under window:" << all.size();
        const QString shotPath =
            QStringLiteral("C:/Users/isudo/AppData/Local/Temp/opencode/ui_flows_debug.png");
        qInfo() << "dump: grabWindow result:" << m_window->grabWindow().save(shotPath);
    }

    void clickItem(QQuickItem *target)
    {
        QVERIFY(target);
        const QPointF center = target->mapToScene(
            QPointF(target->width() / 2.0, target->height() / 2.0));
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
    }

    /// Drives one polish + sync pass. Qt Quick runs view refills (delegate
    /// creation) during the window's polish phase, which is normally
    /// triggered by compositor-driven frames. requestUpdate() schedules
    /// that pass explicitly so the interface works while the desktop is
    /// locked and no external frames occur.
    void pumpRender()
    {
        if (!m_window)
            return;
        m_window->requestUpdate();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QTest::qWait(10);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    QQuickItem *waitForItem(const QString &name, int timeoutMs = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            // Hidden twins exist (e.g. per-type menu entries, built-in rows
            // without manage buttons), so require a visible match.
            if (QQuickItem *found = findVisibleItemInVisualTree(m_window->contentItem(), name))
                return found;
            pumpRender();
        }
        return nullptr;
    }

    /// Waits until at least `expected` visible items with the given name
    /// exist; returns how many were found.
    int waitForItemCount(const QString &name, int expected, int timeoutMs = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        int count = items(name).size();
        while (count < expected && timer.elapsed() < timeoutMs) {
            pumpRender();
            count = items(name).size();
        }
        return count;
    }

    /// File paths offered by the clipboard right now (empty when the
    /// clipboard holds no file reference).
    static QStringList clipboardFileUrls()
    {
        QStringList paths;
        const QMimeData *mime = QGuiApplication::clipboard()->mimeData(QClipboard::Clipboard);
        if (!mime)
            return paths;
        const QList<QUrl> urls = mime->urls();
        for (const QUrl &url : urls) {
            if (url.isLocalFile())
                paths.append(QDir::toNativeSeparators(url.toLocalFile()));
        }
        return paths;
    }

    /// Replaces the clipboard with plain text so a later file copy is
    /// unambiguous. Returns false when Windows keeps refusing the write
    /// (another process holding the clipboard); callers treat that as a
    /// soft failure rather than a test failure.
    static bool setClipboardText(const QString &text, int timeoutMs = 2000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
            if (QGuiApplication::clipboard()->text(QClipboard::Clipboard) == text)
                return true;
            QTest::qWait(50);
        }
        return false;
    }

    /// Clicks the item and polls `check` until it reports true. The click is
    /// repeated while a clipboard write is refused: Windows can lock the
    /// clipboard for a moment (clipboard history, other apps), which must
    /// not be mistaken for a broken card behaviour.
    template<typename Check>
    bool clickUntilCopied(QQuickItem *target, Check check, int timeoutMs = 8000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            clickItem(target);
            QElapsedTimer attempt;
            attempt.start();
            while (attempt.elapsed() < 1000) {
                if (check())
                    return true;
                QTest::qWait(50);
            }
        }
        return false;
    }

    /// Closes the open card menu with Esc. The popup grabs focus
    /// asynchronously, so wait for that first (otherwise the key event is
    /// delivered to the window instead of the menu).
    void closeMenuWithEscape(const QString &menuItemName)
    {
        QTRY_VERIFY(m_window->activeFocusItem() != nullptr
                    && m_window->activeFocusItem()->inherits("QQuickPopupItem"));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(items(menuItemName).isEmpty());
    }

    /// Visible card whose property equals the given value (cards are found
    /// by content / display name, never by grid position).
    QQuickItem *waitForCard(const QString &property, const QVariant &value, int timeoutMs = 5000)
    {
        const QByteArray name = property.toUtf8();
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            const QList<QQuickItem *> cards = items(QStringLiteral("snippetCard"));
            for (QQuickItem *card : cards) {
                if (card->property(name.constData()) == value)
                    return card;
            }
            pumpRender();
        }
        return nullptr;
    }

    /// Imports a file through the attachment service and registers a
    /// snippet for it; returns the managed relative path.
    QString importSnippet(const QString &sourcePath, const QString &type,
                          const QString &displayName)
    {
        QSignalSpy importSpy(m_attachmentService.get(), &AttachmentService::importFinished);
        const QString token = m_attachmentService->startImport(sourcePath, type);
        if (token.isEmpty())
            return {};
        if (!importSpy.wait(10000) || importSpy.isEmpty())
            return {};
        const QList<QVariant> result = importSpy.takeFirst();
        if (!result.value(1).toBool())
            return {};
        const QString relativePath = result.value(3).toString();
        if (!m_snippetService->createAttachmentSnippet(
                type, displayName, relativePath, result.value(4).toString(),
                result.value(5).toLongLong(), QString()))
            return {};
        return relativePath;
    }

    /// Generates a PNG in the temporary directory and registers it.
    QString createImageSnippet(const QString &displayName, const QSize &size)
    {
        const QString sourcePath = m_tempDir.filePath(displayName + QStringLiteral(".png"));
        QImage image(size, QImage::Format_RGB32);
        image.fill(Qt::blue);
        if (!image.save(sourcePath, "PNG"))
            return {};
        return importSnippet(sourcePath, QStringLiteral("image"), displayName);
    }

    /// Writes a minimal (header-valid) PDF and registers it.
    QString createPdfSnippet(const QString &displayName)
    {
        const QString sourcePath = m_tempDir.filePath(displayName + QStringLiteral(".pdf"));
        QFile file(sourcePath);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write("%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntrailer\n<<>>\n%%EOF\n");
        file.close();
        return importSnippet(sourcePath, QStringLiteral("pdf"), displayName);
    }

    static void copyDirectoryRecursive(const QString &source, const QString &destination)
    {
        QDir().mkpath(destination);
        const QFileInfoList entries =
            QDir(source).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            const QString target = destination + QLatin1Char('/') + entry.fileName();
            if (entry.isDir())
                copyDirectoryRecursive(entry.absoluteFilePath(), target);
            else
                QFile::copy(entry.absoluteFilePath(), target);
        }
    }

    /// The build-tree qmldir prefers the resource path (fine for the app
    /// binary, which embeds the module). For this file-based test we strip
    /// that directive so the engine loads the QML from disk.
    static void stripPreferLines(const QString &directoryPath)
    {
        QDirIterator iterator(directoryPath, QStringList{QStringLiteral("qmldir")}, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QStringList lines;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine());
                if (!line.trimmed().startsWith(QLatin1String("prefer")))
                    lines.append(line);
            }
            file.close();
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                for (const QString &line : lines)
                    file.write(line.toUtf8());
            }
        }
    }

    QTemporaryDir m_tempDir;
    std::unique_ptr<DatabaseManager> m_database;
    std::unique_ptr<CategoryRepository> m_categoryRepository;
    std::unique_ptr<SnippetRepository> m_snippetRepository;
    std::unique_ptr<SnippetListModel> m_snippetsModel;
    std::unique_ptr<CategoryListModel> m_categoriesModel;
    std::unique_ptr<SystemClipboardWriter> m_clipboard;
    std::unique_ptr<AttachmentService> m_attachmentService;
    std::unique_ptr<SystemService> m_systemService;
    std::unique_ptr<CategoryService> m_categoryService;
    std::unique_ptr<BackupService> m_backupService;
    std::unique_ptr<SnippetService> m_snippetService;
    std::unique_ptr<AppInfo> m_appInfo;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    std::unique_ptr<WindowChrome> m_chrome;
    QQuickWindow *m_window = nullptr;
    QString m_previousClipboardText;
};

void TestUiFlows::initTestCase()
{
    if (!interactiveDesktopAccessible())
        QSKIP("The interactive desktop is locked; UI flows cannot run right now.");

    // Same style as the application (main.cpp); without it Controls fall
    // back to the native style and reject the custom backgrounds.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QVERIFY(m_tempDir.isValid());
    qputenv("TALKBOX_DATA_DIR", m_tempDir.path().toUtf8());

    QString error;
    QVERIFY2(AppPaths::ensureDirectories(&error), qPrintable(error));

    m_database = std::make_unique<DatabaseManager>(QStringLiteral("ui_flows_test"));
    QVERIFY2(m_database->open(AppPaths::databaseFile(), &error), qPrintable(error));

    m_categoryRepository = std::make_unique<CategoryRepository>(*m_database);
    m_snippetRepository = std::make_unique<SnippetRepository>(*m_database);
    m_snippetsModel = std::make_unique<SnippetListModel>(*m_snippetRepository);
    m_categoriesModel = std::make_unique<CategoryListModel>(*m_categoryRepository);
    m_clipboard = std::make_unique<SystemClipboardWriter>();
    m_attachmentService = std::make_unique<AttachmentService>();
    m_systemService = std::make_unique<SystemService>();
    m_categoryService = std::make_unique<CategoryService>(*m_categoryRepository);
    m_backupService = std::make_unique<BackupService>(*m_database, m_tempDir.path());
    m_snippetService =
        std::make_unique<SnippetService>(*m_snippetRepository, *m_clipboard, *m_attachmentService);
    m_appInfo = std::make_unique<AppInfo>();

    QObject::connect(m_snippetService.get(), &SnippetService::snippetsChanged,
                     m_snippetsModel.get(), &SnippetListModel::refresh);
    QObject::connect(m_categoryService.get(), &CategoryService::categoriesChanged,
                     m_categoriesModel.get(), &CategoryListModel::refresh);
    QObject::connect(m_categoryService.get(), &CategoryService::categoriesChanged,
                     m_snippetsModel.get(), &SnippetListModel::refresh);

    qmlRegisterSingletonInstance("TalkBox", 1, 0, "AppInfo", m_appInfo.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Snippets", m_snippetsModel.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Categories", m_categoriesModel.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "SnippetService", m_snippetService.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "Attachments", m_attachmentService.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "SystemService", m_systemService.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "CategoryService", m_categoryService.get());
    qmlRegisterSingletonInstance("TalkBox", 1, 0, "BackupService", m_backupService.get());

    m_previousClipboardText = QGuiApplication::clipboard()->text(QClipboard::Clipboard);

    m_engine = std::make_unique<QQmlApplicationEngine>();
    // Stage the module from the build tree (same files that are embedded
    // into the application binary) and load it from disk.
    const QString moduleSource =
        QDir::cleanPath(QStringLiteral(TALKBOX_QML_IMPORT_DIR) + QStringLiteral("/TalkBox"));
    QVERIFY2(QFileInfo::exists(moduleSource + QStringLiteral("/qmldir")), qPrintable(moduleSource));
    const QString stageRoot = m_tempDir.filePath(QStringLiteral("qml-stage"));
    copyDirectoryRecursive(moduleSource, stageRoot + QStringLiteral("/TalkBox"));
    stripPreferLines(stageRoot + QStringLiteral("/TalkBox"));
    m_engine->addImportPath(stageRoot);
    m_engine->loadFromModule(QStringLiteral("TalkBox"), QStringLiteral("Main"));
    QVERIFY2(!m_engine->rootObjects().isEmpty(), "Main.qml failed to load");

    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().first());
    QVERIFY2(m_window, "root object is not a window");

    // Main.qml starts hidden so the application can prepare the native frame
    // first; the real window is shown by main.cpp. Mirror that here.
    m_chrome = std::make_unique<WindowChrome>();
    QVERIFY2(m_chrome->attach(m_window), "custom window chrome could not be attached");
    m_window->setVisible(true);

    qInfo() << "qWaitForWindowExposed:" << QTest::qWaitForWindowExposed(m_window)
            << "isExposed:" << m_window->isExposed();
    pumpRender();
}

void TestUiFlows::cleanupTestCase()
{
    m_chrome.reset();
    m_engine.reset();
    if (!m_previousClipboardText.isEmpty())
        QGuiApplication::clipboard()->setText(m_previousClipboardText, QClipboard::Clipboard);
}

void TestUiFlows::imagePreviewCopiesImage()
{
    // Deterministic panel size: the compact default (this is the size the
    // preview dialog used to overflow).
    m_window->setWidth(420);
    m_window->setHeight(744);
    m_window->requestUpdate();
    QTest::qWait(200);

    // Import a real PNG through the attachment service, then register it.
    const QString sourcePath = m_tempDir.filePath(QStringLiteral("preview.png"));
    QImage image(24, 18, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY(image.save(sourcePath, "PNG"));

    QSignalSpy importSpy(m_attachmentService.get(), &AttachmentService::importFinished);
    const QString token = m_attachmentService->startImport(sourcePath, QStringLiteral("image"));
    QVERIFY2(!token.isEmpty(), qPrintable(m_attachmentService->lastError()));
    QTRY_VERIFY_WITH_TIMEOUT(importSpy.count() >= 1, 10000);
    const QList<QVariant> importResult = importSpy.takeFirst();
    QVERIFY2(importResult.value(1).toBool(), qPrintable(importResult.value(2).toString()));

    QVERIFY2(m_snippetService->createAttachmentSnippet(
                 QStringLiteral("image"), QStringLiteral("预览测试.png"),
                 importResult.value(3).toString(), importResult.value(4).toString(),
                 importResult.value(5).toLongLong(), QString()),
             qPrintable(m_snippetService->lastError()));
    QTRY_COMPARE(m_snippetsModel->rowCount(), 1);
    nudgeSnippetGrid();

    // The preview is a menu action now: card body click copies instead.
    QQuickItem *card = waitForItem(QStringLiteral("snippetCard"));
    QVERIFY(card);
    clickItem(waitForItem(QStringLiteral("cardMoreButton")));
    QQuickItem *previewEntry = waitForItem(QStringLiteral("menuPreviewImage"));
    QVERIFY(previewEntry);
    clickItem(previewEntry);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("imagePreviewDialog"))));

    // Regression guard: on the compact panel every button of the dialog
    // must stay inside the window (they used to be pushed out and clipped).
    const qreal windowWidth = m_window->width();
    const QStringList buttonNames{QStringLiteral("previewCopyButton"),
                                  QStringLiteral("previewCloseButton")};
    for (const QString &name : buttonNames) {
        QQuickItem *button = waitForItem(name);
        QVERIFY(button);
        const QPointF topLeft = button->mapToScene(QPointF(0, 0));
        QVERIFY2(topLeft.x() >= 0 && topLeft.x() + button->width() <= windowWidth,
                 qPrintable(QStringLiteral("%1 is clipped: x=%2 width=%3 window=%4")
                                .arg(name)
                                .arg(topLeft.x())
                                .arg(button->width())
                                .arg(windowWidth)));
    }
    QObject *previewDialog = object(QStringLiteral("imagePreviewDialog"));
    QVERIFY(previewDialog);
    QVERIFY2(previewDialog->property("height").toDouble() <= m_window->height(),
             "preview dialog is taller than the window");

    // "复制图片" puts the image on the system clipboard.
    QQuickItem *copyButton = waitForItem(QStringLiteral("previewCopyButton"));
    QVERIFY2(clickUntilCopied(copyButton, []() {
                 return QGuiApplication::clipboard()->image(QClipboard::Clipboard).size()
                         == QSize(24, 18);
             }),
             "clipboard did not receive the image");

    QQuickItem *closeButton = waitForItem(QStringLiteral("previewCloseButton"));
    clickItem(closeButton);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("imagePreviewDialog"))));
}

void TestUiFlows::menuClickDoesNotCopyButMenuItemDoes()
{
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("菜单测试话术"),
                                                QString()));
    QTRY_COMPARE(m_snippetsModel->rowCount(), 2);
    nudgeSnippetGrid();

    QGuiApplication::clipboard()->setText(QStringLiteral("MENU_MARKER"), QClipboard::Clipboard);

    // The text card is the most recently updated entry -> the first card.
    QCOMPARE(waitForItemCount(QStringLiteral("cardMoreButton"), 2), 2);
    const QList<QQuickItem *> moreButtons = items(QStringLiteral("cardMoreButton"));
    QSignalSpy copiedSpy(m_snippetService.get(), &SnippetService::snippetCopied);

    clickItem(moreButtons.first());

    // Opening the menu must not trigger the card body behaviour (AT-04).
    // The menu items are only visible while the popup is open.
    QVERIFY(waitForItem(QStringLiteral("menuCopyContent")) != nullptr);
    QCOMPARE(copiedSpy.count(), 0);
    QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard),
             QStringLiteral("MENU_MARKER"));

    // The "复制内容" entry copies the full text.
    QQuickItem *copyEntry = waitForItem(QStringLiteral("menuCopyContent"));
    clickItem(copyEntry);
    QTRY_COMPARE(copiedSpy.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->text(QClipboard::Clipboard),
                              QStringLiteral("菜单测试话术"), 3000);
    QTRY_VERIFY(items(QStringLiteral("menuCopyContent")).isEmpty());
}

void TestUiFlows::deleteFromMenuCancelKeepsAndConfirmRemoves()
{
    // Two cards: text (first) and image (second). Delete the image.
    nudgeSnippetGrid();
    QCOMPARE(waitForItemCount(QStringLiteral("cardMoreButton"), 2), 2);
    const QList<QQuickItem *> moreButtons = items(QStringLiteral("cardMoreButton"));

    const auto repositoryCount = [this]() { return m_snippetRepository->list({}).size(); };
    QCOMPARE(repositoryCount(), 2);

    // Locate the image entry's managed file for the later check.
    QString managedFile;
    for (const Snippet &snippet : m_snippetRepository->list({})) {
        if (snippet.type == QLatin1String("image"))
            managedFile = m_attachmentService->absolutePath(snippet.relativePath);
    }
    QVERIFY(!managedFile.isEmpty());
    QVERIFY(QFileInfo::exists(managedFile));

    // Cancel path: nothing changes (AT-11).
    clickItem(moreButtons.at(1));
    QVERIFY(waitForItem(QStringLiteral("menuDelete")) != nullptr);
    QQuickItem *deleteEntry = waitForItem(QStringLiteral("menuDelete"));
    clickItem(deleteEntry);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("deleteConfirmDialog"))));

    QQuickItem *cancelButton = waitForItem(QStringLiteral("deleteCancelButton"));
    clickItem(cancelButton);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("deleteConfirmDialog"))));
    QCOMPARE(repositoryCount(), 2);
    QVERIFY(QFileInfo::exists(managedFile));

    // Confirm path: record disappears, managed file is cleaned up.
    QCOMPARE(waitForItemCount(QStringLiteral("cardMoreButton"), 2), 2);
    const QList<QQuickItem *> buttonsAgain = items(QStringLiteral("cardMoreButton"));
    clickItem(buttonsAgain.at(1));
    QVERIFY(waitForItem(QStringLiteral("menuDelete")) != nullptr);
    deleteEntry = waitForItem(QStringLiteral("menuDelete"));
    clickItem(deleteEntry);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("deleteConfirmDialog"))));

    QQuickItem *confirmButton = waitForItem(QStringLiteral("deleteConfirmButton"));
    clickItem(confirmButton);
    QTRY_COMPARE(repositoryCount(), 1);
    QVERIFY(!QFileInfo::exists(managedFile));
    QTRY_COMPARE(m_snippetsModel->rowCount(), 1);
}

void TestUiFlows::gridUsesTwoColumns()
{
    // Regression guard: cellWidth must represent the full column pitch,
    // otherwise Qt computes a single column and the right half of the
    // window stays empty.
    if (m_snippetsModel->rowCount() < 2)
        QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("布局检查"), QString()));

    // The compact default window is a single column by design; widen the
    // window to exercise the two-column layout.
    m_window->setWidth(760);
    QTest::qWait(250);
    nudgeSnippetGrid();
    QCOMPARE(waitForItemCount(QStringLiteral("cardMoreButton"), 2), 2);

    const QList<QQuickItem *> buttons = items(QStringLiteral("cardMoreButton"));
    QCOMPARE(buttons.size(), 2);
    const QPointF firstPosition = buttons.at(0)->mapToScene(QPointF(0, 0));
    const QPointF secondPosition = buttons.at(1)->mapToScene(QPointF(0, 0));

    // Same row (same y) and clearly different columns (different x).
    QCOMPARE(qRound(firstPosition.y()), qRound(secondPosition.y()));
    QVERIFY2(qAbs(firstPosition.x() - secondPosition.x()) > 100,
             "cards are stacked in one column");

    // The compact panel width also uses two columns (user requirement).
    m_window->setWidth(420);
    QTest::qWait(250);
    nudgeSnippetGrid();
    QCOMPARE(waitForItemCount(QStringLiteral("cardMoreButton"), 2), 2);
    const QList<QQuickItem *> narrowButtons = items(QStringLiteral("cardMoreButton"));
    QCOMPARE(narrowButtons.size(), 2);
    const QPointF narrowFirst = narrowButtons.at(0)->mapToScene(QPointF(0, 0));
    const QPointF narrowSecond = narrowButtons.at(1)->mapToScene(QPointF(0, 0));
    QCOMPARE(qRound(narrowFirst.y()), qRound(narrowSecond.y()));
    QVERIFY2(qAbs(narrowFirst.x() - narrowSecond.x()) > 60,
             "compact panel should still show two columns");
}

void TestUiFlows::cardBodyClickCopiesContent()
{
    // Text card: a body click copies the full text right away, without
    // opening the card menu (PRD F-06 / AT-03).
    const QString textBody = QStringLiteral("点击卡片直接复制测试话术");
    QVERIFY2(m_snippetService->createTextSnippet(textBody, QString()),
             qPrintable(m_snippetService->lastError()));
    QQuickItem *textCard = waitForCard(QStringLiteral("content"), textBody);
    QVERIFY2(textCard, "text card did not appear");

    setClipboardText(QStringLiteral("BODY_MARKER"));
    QVERIFY2(clickUntilCopied(textCard, [textBody]() {
                 return QGuiApplication::clipboard()->text(QClipboard::Clipboard) == textBody;
             }),
             "body click did not copy the text");
    QVERIFY2(items(QStringLiteral("menuCopyContent")).isEmpty(), "body click opened the menu");

    // Image card: the body click copies the bitmap instead of opening the
    // in-app preview (the preview is a menu action now).
    const QString imageName = QStringLiteral("点击复制图片.png");
    QVERIFY(!createImageSnippet(imageName, QSize(36, 24)).isEmpty());
    QQuickItem *imageCard = waitForCard(QStringLiteral("displayName"), imageName);
    QVERIFY2(imageCard, "image card did not appear");

    setClipboardText(QStringLiteral("IMAGE_MARKER"));
    QVERIFY2(clickUntilCopied(imageCard, []() {
                 return QGuiApplication::clipboard()->image(QClipboard::Clipboard).size()
                         == QSize(36, 24);
             }),
             "image body click did not copy the bitmap");
    QVERIFY2(!objectVisible(object(QStringLiteral("imagePreviewDialog"))),
             "body click must not open the preview dialog");

    // The menu (⋯ / right click) offers copy + preview for images and hides
    // the entries of the other types.
    QQuickItem *moreButton = findItemInVisualTree(imageCard, QStringLiteral("cardMoreButton"));
    QVERIFY(moreButton);
    clickItem(moreButton);
    QQuickItem *copyEntry = waitForItem(QStringLiteral("menuCopyImage"));
    QVERIFY2(copyEntry, "image menu has no copy entry");
    QVERIFY(waitForItem(QStringLiteral("menuPreviewImage")) != nullptr);
    QVERIFY(findVisibleItemInVisualTree(m_window->contentItem(),
                                       QStringLiteral("menuCopyFile")) == nullptr);
    closeMenuWithEscape(QStringLiteral("menuCopyImage"));
}

void TestUiFlows::rightClickOpensCardMenuWithoutCopying()
{
    const QString textBody = QStringLiteral("右键菜单测试话术");
    QVERIFY2(m_snippetService->createTextSnippet(textBody, QString()),
             qPrintable(m_snippetService->lastError()));
    nudgeSnippetGrid();
    QQuickItem *card = waitForCard(QStringLiteral("content"), textBody);
    QVERIFY(card);

    const QString marker = QStringLiteral("RIGHT_MARKER");
    const bool markerSet = setClipboardText(marker);

    const QPointF center = card->mapToScene(QPointF(card->width() / 2.0, card->height() / 2.0));
    QTest::mouseClick(m_window, Qt::RightButton, Qt::NoModifier, center.toPoint());

    // The menu opens and nothing is copied (PRD 4.5 / AT-04).
    QVERIFY(waitForItem(QStringLiteral("menuCopyContent")) != nullptr);
    if (markerSet)
        QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard), marker);

    // Esc closes the menu again (the popup grabs focus asynchronously).
    closeMenuWithEscape(QStringLiteral("menuCopyContent"));
    if (markerSet)
        QCOMPARE(QGuiApplication::clipboard()->text(QClipboard::Clipboard), marker);
}

void TestUiFlows::pdfCardBodyClickCopiesFile()
{
    const QString pdfName = QStringLiteral("点击复制文件.pdf");
    const QString relativePath = createPdfSnippet(pdfName);
    QVERIFY2(!relativePath.isEmpty(), qPrintable(m_snippetService->lastError()));
    QQuickItem *card = waitForCard(QStringLiteral("displayName"), pdfName);
    QVERIFY2(card, "pdf card did not appear");

    setClipboardText(QStringLiteral("PDF_MARKER"));

    // A body click copies the file itself (a file reference), it does not
    // open the system PDF reader.
    const QString managedPath =
        QDir::toNativeSeparators(m_attachmentService->absolutePath(relativePath));
    QVERIFY2(clickUntilCopied(card, [&managedPath]() {
                 const QStringList urls = clipboardFileUrls();
                 return urls.size() == 1 && urls.constFirst() == managedPath;
             }),
             "pdf body click did not copy the file");
    QVERIFY(!objectVisible(object(QStringLiteral("imagePreviewDialog"))));

    // The menu still offers opening the PDF (and the same copy action).
    QQuickItem *moreButton = findItemInVisualTree(card, QStringLiteral("cardMoreButton"));
    QVERIFY(moreButton);
    clickItem(moreButton);
    QVERIFY2(waitForItem(QStringLiteral("menuCopyFile")) != nullptr, "pdf menu has no copy entry");
    QVERIFY(waitForItem(QStringLiteral("menuOpenPdf")) != nullptr);
    closeMenuWithEscape(QStringLiteral("menuCopyFile"));
}

void TestUiFlows::settingsCategoryManagementFlow()
{
    // Open the settings dialog via the gear button.
    QQuickItem *settingsButton = waitForItem(QStringLiteral("settingsButton"));
    QVERIFY(settingsButton);
    clickItem(settingsButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("settingsDialog"))));

    // Switch to the category management section.
    QQuickItem *categoriesTab = waitForItem(QStringLiteral("categoriesSectionTab"));
    QVERIFY(categoriesTab);
    clickItem(categoriesTab);

    // Create a category through the dialog.
    QQuickItem *createButton = waitForItem(QStringLiteral("createCategoryButton"));
    QVERIFY(createButton);
    clickItem(createButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("categoryNameInputDialog"))));

    QQuickItem *field = waitForItem(QStringLiteral("textInputField"));
    QVERIFY(field);
    field->setProperty("text", QStringLiteral("阶段C测试分类"));
    QQuickItem *submit = waitForItem(QStringLiteral("textInputSubmitButton"));
    QVERIFY(submit);
    clickItem(submit);

    QString categoryId;
    QTRY_VERIFY(([&]() {
        for (const Category &category : m_categoryRepository->listAll()) {
            if (category.name == QStringLiteral("阶段C测试分类")) {
                categoryId = category.id;
                return true;
            }
        }
        return false;
    })());
    QVERIFY(!categoryId.isEmpty());

    // Rename it through the UI (exactly one custom category exists).
    QCOMPARE(waitForItemCount(QStringLiteral("renameCategoryButton"), 1), 1);
    clickItem(items(QStringLiteral("renameCategoryButton")).first());
    QTRY_VERIFY(objectVisible(object(QStringLiteral("categoryNameInputDialog"))));
    field = waitForItem(QStringLiteral("textInputField"));
    QVERIFY(field);
    field->setProperty("text", QStringLiteral("阶段C改名后"));
    submit = waitForItem(QStringLiteral("textInputSubmitButton"));
    QVERIFY(submit);
    clickItem(submit);
    QTRY_COMPARE(m_categoryRepository->findById(categoryId).value().name,
                 QStringLiteral("阶段C改名后"));

    // Delete it (content migration itself is covered by the repository test).
    QCOMPARE(waitForItemCount(QStringLiteral("deleteCategoryButton"), 1), 1);
    QQuickItem *deleteButton = items(QStringLiteral("deleteCategoryButton")).first();
    QVERIFY(deleteButton);
    clickItem(deleteButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("deleteCategoryConfirmDialog"))));
    QQuickItem *confirm = waitForItem(QStringLiteral("genericConfirmButton"));
    QVERIFY(confirm);
    clickItem(confirm);
    QTRY_VERIFY(!m_categoryRepository->exists(categoryId));

    // Close the settings dialog.
    QQuickItem *closeButton = waitForItem(QStringLiteral("settingsCloseButton"));
    QVERIFY(closeButton);
    clickItem(closeButton);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("settingsDialog"))));
}

void TestUiFlows::noBuiltinCategoriesAndUncategorizedFilter()
{
    // No built-in categories: only the user-created ones exist, and snippets
    // without a category are reachable through the virtual "未分类" chip.
    QVERIFY(m_categoryRepository->listAll().isEmpty());
    QCOMPARE(m_categoriesModel->totalCount(), 0);
    QCOMPARE(waitForItemCount(QStringLiteral("categoryChip"), 0), 0);

    // Content without a category is allowed and counted.
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("没有分类的话术"), QString()));
    QTRY_VERIFY(m_snippetsModel->uncategorizedCount() >= 1);

    QQuickItem *noneChip = waitForItem(QStringLiteral("uncategorizedChip"));
    QVERIFY(noneChip);
    QVERIFY(noneChip->isVisible());
    clickItem(noneChip);
    QTRY_COMPARE(m_snippetsModel->categoryFilter(), QStringLiteral("uncategorized"));
    QCOMPARE(m_snippetsModel->totalCount(), m_snippetsModel->uncategorizedCount());

    // Back to "all" for the following tests.
    m_snippetsModel->setCategoryFilter(QStringLiteral("all"));

    // The create dialog offers "未分类" as a category choice.
    QQuickItem *newButton = waitForItem(QStringLiteral("newButton"));
    QVERIFY(newButton);
    clickItem(newButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("newSnippetDialog"))));
    QQuickItem *typeText = waitForItem(QStringLiteral("typeTextTile"));
    QVERIFY(typeText);
    clickItem(typeText);
    QVERIFY(waitForItem(QStringLiteral("categoryPickerNone")) != nullptr);
    QQuickItem *closeButton = waitForItem(QStringLiteral("dialogCancelButton"));
    QVERIFY(closeButton);
    clickItem(closeButton);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("newSnippetDialog"))));
}

void TestUiFlows::clickOutsideClosesDialogs()
{
    const QPoint outside(6, 6); // inside the app, outside the centred panel

    // A plain dialog is dismissed by clicking the empty space around it.
    QQuickItem *settingsButton = waitForItem(QStringLiteral("settingsButton"));
    QVERIFY(settingsButton);
    clickItem(settingsButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("settingsDialog"))));
    QTest::qWait(300);
    QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("settingsDialog"))));

    // A nested dialog consumes the outside click; the parent stays open.
    clickItem(settingsButton);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("settingsDialog"))));
    QTest::qWait(300);
    clickItem(waitForItem(QStringLiteral("categoriesSectionTab")));
    clickItem(waitForItem(QStringLiteral("createCategoryButton")));
    QTRY_VERIFY(objectVisible(object(QStringLiteral("categoryNameInputDialog"))));
    QTest::qWait(200);
    QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("categoryNameInputDialog"))));
    QVERIFY2(objectVisible(object(QStringLiteral("settingsDialog"))),
             "closing the nested dialog also closed the settings panel");
    clickItem(waitForItem(QStringLiteral("settingsCloseButton")));
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("settingsDialog"))));

    // A dialog with unsaved input asks before discarding instead of closing.
    QTest::keyClick(m_window, Qt::Key_N, Qt::ControlModifier);
    QTRY_VERIFY(objectVisible(object(QStringLiteral("newSnippetDialog"))));
    QTest::qWait(250);
    clickItem(waitForItem(QStringLiteral("typeTextTile")));
    QQuickItem *contentArea = waitForItem(QStringLiteral("newSnippetContentArea"));
    QVERIFY(contentArea);
    contentArea->setProperty("text", QStringLiteral("未保存的草稿"));
    QTRY_COMPARE(contentArea->property("text").toString(), QStringLiteral("未保存的草稿"));

    QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_VERIFY(waitForItem(QStringLiteral("dialogDiscardButton")) != nullptr);
    QVERIFY(objectVisible(object(QStringLiteral("newSnippetDialog"))));

    // 继续编辑 returns to the form without closing.
    clickItem(waitForItem(QStringLiteral("dialogContinueEditingButton")));
    QTRY_VERIFY(waitForItem(QStringLiteral("dialogDiscardButton")) == nullptr);
    QVERIFY(objectVisible(object(QStringLiteral("newSnippetDialog"))));

    // Clicking outside again asks once more; 放弃 finally closes.
    QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_VERIFY(waitForItem(QStringLiteral("dialogDiscardButton")) != nullptr);
    clickItem(waitForItem(QStringLiteral("dialogDiscardButton")));
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("newSnippetDialog"))));

    // Same protection for the inline edit dialog.
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("弹窗外部关闭测试话术"), QString()));
    nudgeSnippetGrid();
    QQuickItem *card = waitForCard(QStringLiteral("content"), QStringLiteral("弹窗外部关闭测试话术"));
    QVERIFY(card);
    clickItem(findItemInVisualTree(card, QStringLiteral("cardMoreButton")));
    QVERIFY(waitForItem(QStringLiteral("menuEditContent")) != nullptr);
    clickItem(waitForItem(QStringLiteral("menuEditContent")));
    QTRY_VERIFY(objectVisible(object(QStringLiteral("editTextDialog"))));
    QTest::qWait(250);
    QQuickItem *editArea = findVisibleItemInVisualTree(m_window->contentItem(),
                                                       QStringLiteral("editTextContentArea"));
    QVERIFY(editArea);
    editArea->setProperty("text", QStringLiteral("弹窗外部关闭测试话术（已修改）"));
    QTRY_COMPARE(editArea->property("text").toString(),
                 QStringLiteral("弹窗外部关闭测试话术（已修改）"));
    QTest::mouseClick(m_window, Qt::LeftButton, Qt::NoModifier, outside);
    QVERIFY2(objectVisible(object(QStringLiteral("editTextDialog"))),
             "edit dialog closed and dropped the unsaved edit");
    QTRY_VERIFY(object(QStringLiteral("editTextDialog"))->property("confirmingDiscard").toBool());

    // Leaving the dialog discards the edit for real (the 放弃 path).
    QMetaObject::invokeMethod(object(QStringLiteral("editTextDialog")), "close");
    QTRY_VERIFY(!objectVisible(object(QStringLiteral("editTextDialog"))));
}

void TestUiFlows::dragShowsContentFeedback()
{
    // A platform drag (QDrag) cannot be synthesised headlessly, so the card's
    // drag signals are emitted directly to verify the window wiring: while a
    // card is carried the window shows what is being dragged and where it can
    // be dropped, then hides the hint again.
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("drag feedback snippet"), QString()));
    nudgeSnippetGrid();
    QQuickItem *card = waitForCard(QStringLiteral("content"), QStringLiteral("drag feedback snippet"));
    QVERIFY2(card, "drag feedback card did not appear");

    QQuickItem *hint = item(QStringLiteral("dragHint"));
    QVERIFY(hint);
    QVERIFY(!hint->isVisible());

    QVERIFY(QMetaObject::invokeMethod(card, "dragStarted", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("drag feedback snippet"))));
    QTRY_VERIFY(hint->isVisible());
    QCOMPARE(m_window->property("dragHintDetail").toString(),
             QStringLiteral("drag feedback snippet"));
    QCOMPARE(m_window->property("dragHintTitle").toString(),
             QStringLiteral("松开即可把文字拖到聊天窗口"));

    QVERIFY(QMetaObject::invokeMethod(card, "dragFinished", Qt::DirectConnection));
    QTRY_VERIFY(!hint->isVisible());
    QVERIFY(!m_window->property("dragHintVisible").toBool());
}

void TestUiFlows::typeFilterSitsAtWindowBottom()
{
    // The 全部内容 / 图片 / PDF filter row belongs to the bottom edge of the
    // window, below the list, with the count left and the buttons right.
    m_window->setHeight(720);
    QTest::qWait(300);
    pumpRender();
    QTest::qWait(200);
    QQuickItem *all = waitForItem(QStringLiteral("typeFilterAll"));
    QQuickItem *image = waitForItem(QStringLiteral("typeFilterImage"));
    QQuickItem *pdf = waitForItem(QStringLiteral("typeFilterPdf"));
    QVERIFY(all);
    QVERIFY(image);
    QVERIFY(pdf);

    const qreal windowHeight = m_window->height();
    const qreal filterTop = all->mapToScene(QPointF(0, 0)).y();

    QVERIFY2(filterTop + all->height() <= windowHeight - 4,
             "filter row is clipped by the window bottom");
    QVERIFY2(filterTop > windowHeight * 0.75,
             "filter row is not at the bottom of the window");

    auto *grid = m_window->findChild<QQuickItem *>(QStringLiteral("snippetGrid"));
    QVERIFY(grid);
    const qreal gridBottom = grid->mapToScene(QPointF(0, grid->height())).y();
    QVERIFY2(gridBottom <= filterTop + 1, "filter row overlaps the snippet list");

    // Left to right: 全部内容, 图片, PDF.
    QVERIFY(all->mapToScene(QPointF(0, 0)).x() < image->mapToScene(QPointF(0, 0)).x());
    QVERIFY(image->mapToScene(QPointF(0, 0)).x() < pdf->mapToScene(QPointF(0, 0)).x());

    // Count left-aligned, buttons right-aligned.
    QQuickItem *countLabel = waitForItem(QStringLiteral("resultLabel"));
    QVERIFY(countLabel);
    const qreal countLeft = countLabel->mapToScene(QPointF(0, 0)).x();
    const qreal allLeft = all->mapToScene(QPointF(0, 0)).x();
    QVERIFY2(countLeft < allLeft - 100, "count and filter buttons are grouped together");
    QVERIFY2(countLeft < 40, "count is not left-aligned");
    QVERIFY2(pdf->mapToScene(QPointF(0, 0)).x() + pdf->width() > m_window->width() - 40,
             "filter buttons are not right-aligned");
}

QTEST_MAIN(TestUiFlows)
void TestUiFlows::cardBordersNotClipped()
{
    // Cards are drawn inside a clipping scroll view; they must leave room for
    // their border (2px while dragging) so no edge is cut off.
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("边距检查话术"), QString()));
    nudgeSnippetGrid();
    m_window->setWidth(420);
    QTest::qWait(300);
    QQuickItem *card = waitForCard(QStringLiteral("content"), QStringLiteral("边距检查话术"));
    QVERIFY(card);
    auto *gridItem = m_window->findChild<QQuickItem *>(QStringLiteral("snippetGrid"));
    QVERIFY(gridItem);

    const QPointF gridOrigin = gridItem->mapToScene(QPointF(0, 0));
    const QPointF cardPos = card->mapToScene(QPointF(0, 0));
    const qreal leftGap = cardPos.x() - gridOrigin.x();
    const qreal topGap = cardPos.y() - gridOrigin.y();
    const qreal rightGap = gridOrigin.x() + gridItem->width() - (cardPos.x() + card->width());

    qInfo() << "grid" << gridItem->size()
            << "columns" << gridItem->property("columns")
            << "pitch" << gridItem->property("cellWidth")
            << "count" << gridItem->property("count")
            << "left" << leftGap << "right" << rightGap
            << "top" << topGap << "card" << card->size();

    // At least the full border width of room on every side of a card, so a
    // 2px drag outline is never cut off by the clipping scroll view.
    QVERIFY2(leftGap >= 2, qPrintable(QStringLiteral("left room %1").arg(leftGap)));
    QVERIFY2(topGap >= 2, qPrintable(QStringLiteral("top room %1").arg(topGap)));
    QVERIFY2(rightGap >= 2, qPrintable(QStringLiteral("right room %1").arg(rightGap)));

    // With two columns filled, the second card must end flush with the grid's
    // right padding - the same room the first card has on the left.
    QVERIFY(m_snippetService->createTextSnippet(QStringLiteral("边距检查话术二"), QString()));
    nudgeSnippetGrid();
    QVERIFY(waitForItemCount(QStringLiteral("snippetCard"), 2) >= 2);
    const QList<QQuickItem *> cards = items(QStringLiteral("snippetCard"));
    qreal widestRight = 0;
    for (QQuickItem *candidate : cards) {
        const QPointF pos = candidate->mapToScene(QPointF(0, 0));
        widestRight = qMax(widestRight, pos.x() + candidate->width());
    }
    const qreal rightRoom = gridOrigin.x() + gridItem->width() - widestRight;
    qInfo() << "right room with two columns" << rightRoom;
    QVERIFY2(rightRoom >= 2, qPrintable(QStringLiteral("right room %1").arg(rightRoom)));
    QVERIFY2(qAbs(rightRoom - leftGap) < 2.0,
             qPrintable(QStringLiteral("left %1 vs right %2").arg(leftGap).arg(rightRoom)));
}

void TestUiFlows::titleBarMaximizesAndRestores()
{
    // The custom caption sits on a native frame: the maximize / restore
    // button still has to reach the window state, and a maximized window must
    // keep its caption on screen (the native frame is inflated beyond the
    // monitor work area; WindowChrome clamps the client rect back to it).
    QVERIFY(m_window->isVisible());

    QQuickItem *titleBar = waitForItem(QStringLiteral("titleBar"));
    QVERIFY(titleBar);
    QCOMPARE(qRound(titleBar->height()), 42);

    const QPointF captionTop = titleBar->mapToScene(QPointF(0, 0));
    QVERIFY2(qAbs(captionTop.y()) < 1.0, qPrintable(QString::number(captionTop.y())));

    QQuickItem *maximizeButton = waitForItem(QStringLiteral("windowMaximizeButton"));
    QQuickItem *minimizeButton = waitForItem(QStringLiteral("windowMinimizeButton"));
    QQuickItem *closeButton = waitForItem(QStringLiteral("windowCloseButton"));
    QVERIFY(maximizeButton);
    QVERIFY(minimizeButton);
    QVERIFY(closeButton);

    // Caption buttons must not be able to take keyboard focus: a click may never
    // leave a focus ring behind (the "black flash" of the earlier attempt).
    QCOMPARE(maximizeButton->property("focus").toBool(), false);
    QCOMPARE(closeButton->property("focus").toBool(), false);
    QCOMPARE(maximizeButton->property("activeFocusOnTab").toBool(), false);
    QCOMPARE(closeButton->property("activeFocusOnTab").toBool(), false);

    // Minimize / maximize / close sit at the right edge, in that order.
    QVERIFY(minimizeButton->mapToScene(QPointF(0, 0)).x()
            < maximizeButton->mapToScene(QPointF(0, 0)).x());
    QVERIFY(maximizeButton->mapToScene(QPointF(0, 0)).x()
            < closeButton->mapToScene(QPointF(0, 0)).x());
    // Caption buttons are laid out flush against the window's right edge and
    // span the full caption height, the same way the native / Edge caption
    // buttons do.
    QVERIFY2(closeButton->mapToScene(QPointF(closeButton->width(), 0)).x()
                 <= m_window->width() + 0.5,
             "caption buttons are clipped by the right edge");
    QVERIFY2(qAbs(closeButton->mapToScene(QPointF(closeButton->width(), 0)).x()
                  - m_window->width()) < 1.5,
             "caption buttons do not reach the right edge");
    QCOMPARE(qRound(closeButton->height()), 42);

    if (m_window->visibility() == QWindow::Maximized)
        m_window->showNormal();
    QTRY_COMPARE(m_window->visibility(), QWindow::Windowed);

    clickItem(maximizeButton);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->visibility(), QWindow::Maximized, 3000);

    // The caption stays inside the work area, never above the screen edge.
    const QPoint topLeft = m_window->position();
    QVERIFY2(topLeft.y() >= 0, qPrintable(QStringLiteral("window top %1").arg(topLeft.y())));

    QQuickItem *restoreIconOwner = waitForItem(QStringLiteral("windowMaximizeButton"));
    QVERIFY(restoreIconOwner);
    QCOMPARE(restoreIconOwner->property("iconSource").toUrl().fileName(),
             QStringLiteral("window-restore.svg"));

    clickItem(restoreIconOwner);
    QTRY_COMPARE_WITH_TIMEOUT(m_window->visibility(), QWindow::Windowed, 3000);
}

void TestUiFlows::startupShowsNoFocusRing()
{
    // The snippet grid owns focus so Enter / Space work immediately, but
    // having focus alone must not outline the first card: the outline means
    // "keyboard position", not "this window is focused".
    m_window->setWidth(420);
    m_window->setHeight(720);
    QTest::qWait(200);
    nudgeSnippetGrid();

    QList<QQuickItem *> cards = items(QStringLiteral("snippetCard"));
    QVERIFY(!cards.isEmpty());

    auto *grid = m_window->findChild<QQuickItem *>(QStringLiteral("snippetGrid"));
    QVERIFY(grid);
    QCOMPARE(grid->property("keyboardNavigation").toBool(), false);

    for (QQuickItem *card : cards) {
        QCOMPARE(card->property("keyboardCurrent").toBool(), false);
        QCOMPARE(card->property("highlighted").toBool(), false);
    }

    // Hovering a card still emphasises it.
    QQuickItem *first = cards.first();
    const QPointF centre = first->mapToScene(
        QPointF(first->width() / 2.0, first->height() / 2.0));
    QTest::mouseMove(m_window, centre.toPoint());
    QTRY_VERIFY(first->property("highlighted").toBool());

    // Moving the pointer away clears the emphasis again.
    QTest::mouseMove(m_window, QPoint(2, m_window->height() - 2));
    QTRY_VERIFY(!first->property("highlighted").toBool());

    // Keyboard navigation does show the outline on the current card.
    QTest::keyClick(m_window, Qt::Key_Down);
    QTRY_VERIFY(grid->property("keyboardNavigation").toBool());
    QQuickItem *current = nullptr;
    for (QQuickItem *card : items(QStringLiteral("snippetCard"))) {
        if (card->property("keyboardCurrent").toBool()) {
            current = card;
            break;
        }
    }
    QVERIFY2(current, "no card is marked as the keyboard position");

    // A mouse click hands the indication back to hover.
    clickItem(current);
    QTRY_VERIFY(!grid->property("keyboardNavigation").toBool());

    // The create dialog opened from a mouse click: the first tile still has
    // focus (so Enter / Space work) but no focus outline is painted until the
    // user navigates with the keyboard.
    QQuickItem *newButton = waitForItem(QStringLiteral("newButton"));
    QVERIFY(newButton);
    clickItem(newButton);
    QObject *dialog = object(QStringLiteral("newSnippetDialog"));
    QTRY_VERIFY(objectVisible(dialog));
    QTRY_VERIFY(dialog->property("keyboardFocusVisible").toBool() == false);

    QQuickItem *tile = waitForItem(QStringLiteral("typeTextTile"));
    QVERIFY(tile);
    QCOMPARE(tile->property("activeFocus").toBool(), true);
    QCOMPARE(tile->property("focusOutline").toBool(), false);

    // Tab into the tiles turns the keyboard outline on.
    QTest::keyClick(m_window, Qt::Key_Tab);
    QTRY_VERIFY(dialog->property("keyboardFocusVisible").toBool());
}

#include "tst_ui_flows.moc"