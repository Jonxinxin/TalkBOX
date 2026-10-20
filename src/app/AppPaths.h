#pragma once

#include <QString>

namespace talkbox {

/// Central resolver for all application data locations.
///
/// All business data lives under a single user-writable root directory
/// (default: QStandardPaths::AppLocalDataLocation, i.e.
/// %LOCALAPPDATA%\TalkBOX). For tests and portable runs the environment
/// variable TALKBOX_DATA_DIR overrides the root.
class AppPaths
{
public:
    static QString dataDir();
    static QString databaseFile();
    static QString attachmentsDir();
    static QString backupsDir();
    static QString logsDir();

    /// Creates the data root and all sub-directories.
    /// Returns false and sets errorMessage on the first failure.
    static bool ensureDirectories(QString *errorMessage);
};

} // namespace talkbox
