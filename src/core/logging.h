// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGGING_H
#define LOGGING_H

#include <QLoggingCategory>
#include <QStringList>

// One logging category per area, all named "salichess.*". Messages are
// qCDebug() for what helps to follow what the app does (requests, state
// changes) and qCInfo() for the few lifecycle events (login, logout). Never
// log tokens, account data or whole response bodies.
Q_DECLARE_LOGGING_CATEGORY(lcApi)     // requests, streams, connectivity
Q_DECLARE_LOGGING_CATEGORY(lcSession) // login, logout
Q_DECLARE_LOGGING_CATEGORY(lcGame)    // games, seeks, challenges
Q_DECLARE_LOGGING_CATEGORY(lcPuzzle)  // puzzle store
Q_DECLARE_LOGGING_CATEGORY(lcEngine)  // Stockfish and its networks

namespace Logging {

enum Level { Debug, Info, Warning, Error };

// Picks the log level from the command line: "--log-level=DEBUG",
// "--log-level DEBUG" or "log_level=DEBUG" (DEBUG, INFO, WARNING or ERROR;
// case does not matter). WARNING is the default, which keeps a normal run
// quiet. Call this before anything logs. Returns false, and keeps the
// default, for a level it does not know.
bool configure(const QStringList &arguments);

// The same, for a level already picked. Used by configure() and the tests.
void setLevel(Level level);

}

#endif // LOGGING_H
