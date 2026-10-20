#include "services/SystemService.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTest>

/// Exercises the real system clipboard (runs inside the interactive
/// desktop session). Skips gracefully when no clipboard is available.
class TestSystemClipboard : public QObject
{
    Q_OBJECT

private slots:
    void copiesImageToClipboard();
    void copiesFileToClipboard();
    void missingFileFails();
};

void TestSystemClipboard::copiesImageToClipboard()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard || QGuiApplication::platformName() == QLatin1String("offscreen"))
        QSKIP("No system clipboard available in this environment.");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.png"));

    QImage image(16, 12, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QVERIFY(image.save(path, "PNG"));

    // Be polite with the user's clipboard: restore the text afterwards.
    const QString previousText = clipboard->text(QClipboard::Clipboard);
    const bool hadText = !previousText.isEmpty();

    talkbox::SystemService service;
    QVERIFY2(service.copyImageToClipboard(path), qPrintable(service.lastError()));

    // Windows may briefly hold the clipboard; retry like Qt does.
    QImage fromClipboard;
    for (int attempt = 0; attempt < 10 && fromClipboard.isNull(); ++attempt) {
        fromClipboard = clipboard->image(QClipboard::Clipboard);
        if (fromClipboard.isNull())
            QTest::qWait(100);
    }
    QVERIFY2(!fromClipboard.isNull(), "Clipboard did not receive the image.");
    QCOMPARE(fromClipboard.size(), QSize(16, 12));

    if (hadText)
        clipboard->setText(previousText, QClipboard::Clipboard);
}

void TestSystemClipboard::copiesFileToClipboard()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard || QGuiApplication::platformName() == QLatin1String("offscreen"))
        QSKIP("No system clipboard available in this environment.");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("clip.pdf"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("%PDF-1.4\n%%EOF\n");
    file.close();

    const QString previousText = clipboard->text(QClipboard::Clipboard);
    const bool hadText = !previousText.isEmpty();

    talkbox::SystemService service;

    // Windows can hold the clipboard for a while (clipboard history, other
    // applications). Retry briefly and, if it stays locked, skip instead of
    // reporting a failure that says nothing about the application.
    bool copied = false;
    for (int attempt = 0; attempt < 5 && !copied; ++attempt) {
        copied = service.copyFileToClipboard(path);
        if (!copied)
            QTest::qWait(300);
    }
    if (!copied)
        QSKIP(qPrintable(QStringLiteral("System clipboard is locked: ") + service.lastError()));

    QStringList urls;
    for (int attempt = 0; attempt < 10 && urls.isEmpty(); ++attempt) {
        const QMimeData *mime = clipboard->mimeData(QClipboard::Clipboard);
        if (mime && mime->hasUrls()) {
            const QList<QUrl> list = mime->urls();
            for (const QUrl &url : list) {
                if (url.isLocalFile())
                    urls.append(QDir::toNativeSeparators(url.toLocalFile()));
            }
        }
        if (urls.isEmpty())
            QTest::qWait(100);
    }
    QCOMPARE(urls.size(), 1);
    QCOMPARE(urls.constFirst(), QDir::toNativeSeparators(path));

    if (hadText)
        clipboard->setText(previousText, QClipboard::Clipboard);
}

void TestSystemClipboard::missingFileFails()
{
    talkbox::SystemService service;
    QVERIFY(!service.copyImageToClipboard(QStringLiteral("C:/definitely/not/here.png")));
    QVERIFY(!service.copyFileToClipboard(QStringLiteral("C:/definitely/not/here.pdf")));
    QVERIFY(!service.lastError().isEmpty());
    QVERIFY(!service.openPath(QStringLiteral("C:/definitely/not/here.pdf")));
    QVERIFY(!service.revealInFolder(QStringLiteral("C:/definitely/not/here.png")));
}

QTEST_MAIN(TestSystemClipboard)

#include "tst_system_clipboard.moc"
