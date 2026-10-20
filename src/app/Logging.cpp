#include "app/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>

namespace talkbox::logging {

namespace {

constexpr qint64 kMaxFileSize = 2 * 1024 * 1024; // 2 MB per file
constexpr int kMaxGenerations = 5;               // talkbox.log.1 ... .5

QMutex g_mutex;
QFile g_logFile;

void rotateIfNeeded(const QString &logFilePath)
{
    const QFileInfo info(logFilePath);
    if (!info.exists() || info.size() <= kMaxFileSize)
        return;

    QFile::remove(logFilePath + QStringLiteral(".%1").arg(kMaxGenerations));
    for (int generation = kMaxGenerations - 1; generation >= 1; --generation) {
        const QString from = logFilePath + QStringLiteral(".%1").arg(generation);
        const QString to = logFilePath + QStringLiteral(".%1").arg(generation + 1);
        if (QFile::exists(from))
            QFile::rename(from, to);
    }
    QFile::rename(logFilePath, logFilePath + QStringLiteral(".1"));
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "DEBUG";
    case QtInfoMsg:
        return "INFO";
    case QtWarningMsg:
        return "WARN";
    case QtCriticalMsg:
        return "ERROR";
    case QtFatalMsg:
        return "FATAL";
    }
    return "LOG";
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);

    const QString line =
        QStringLiteral("%1 [%2] %3")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                 QString::fromLatin1(levelName(type)), message);

    {
        QMutexLocker locker(&g_mutex);
        if (g_logFile.isOpen()) {
            g_logFile.write(line.toUtf8());
            g_logFile.write("\n");
            g_logFile.flush();
        }
    }

    const QByteArray local = line.toLocal8Bit();
    std::fprintf(stderr, "%s\n", local.constData());
    std::fflush(stderr);

    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

void init(const QString &logDir)
{
    QDir().mkpath(logDir);

    const QString logFilePath = logDir + QStringLiteral("/talkbox.log");
    rotateIfNeeded(logFilePath);

    QMutexLocker locker(&g_mutex);
    g_logFile.setFileName(logFilePath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    qInstallMessageHandler(messageHandler);
}

} // namespace talkbox::logging
