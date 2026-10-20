#pragma once

#include <QString>

namespace talkbox::logging {

/// Installs a Qt message handler that appends log lines to
/// <logDir>/talkbox.log and rotates the file when it grows too large
/// (up to 5 generations: talkbox.log.1 ... talkbox.log.5). Messages are
/// also mirrored to stderr. Snippet content is never logged.
///
/// Must be called after the log directory exists.
void init(const QString &logDir);

} // namespace talkbox::logging
