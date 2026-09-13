// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>

int runChessTests(int argc, char *argv[]);
int runLichessTests(int argc, char *argv[]);
int runEngineTests(int argc, char *argv[]);
int runLiveTests(int argc, char *argv[]);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // Same names as the app, so QStandardPaths' test mode mirrors its layout.
    app.setOrganizationName(QStringLiteral("io.github.vaness0"));
    app.setApplicationName(QStringLiteral("harbour-salichess"));
    app.setApplicationVersion(QStringLiteral("test"));

    int failures = 0;
    failures += runChessTests(argc, argv);
    failures += runLichessTests(argc, argv);
    failures += runEngineTests(argc, argv);
    failures += runLiveTests(argc, argv);
    return failures;
}
