// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QJsonObject>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QtTest>

#include "chess/chessgame.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "lichess/puzzlecontroller.h"

// Talks to the real lichess.org (anonymously). Skipped unless
// SALICHESS_NETWORK_TESTS=1.
class TestLive : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        if (qgetenv("SALICHESS_NETWORK_TESTS") != "1")
            QSKIP("set SALICHESS_NETWORK_TESTS=1 to run");
    }

    void livePuzzles()
    {
        LichessApi api;
        Services::init(&api, nullptr, nullptr, nullptr);

        PuzzleController controller;
        controller.loadDaily();
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Playing, 20000);
        QVERIFY(!controller.puzzleId().isEmpty());
        QVERIFY(controller.game()->ply() > 0);
        controller.viewSolution();
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Finished, 20000);

        controller.setAngle(QStringLiteral("mateIn2"));
        controller.loadNext();
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Playing, 20000);
        QVERIFY(controller.themes().contains(QStringLiteral("mateIn2")));
        controller.viewSolution();
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Finished, 20000);
        QCOMPARE(controller.game()->outcome(), QStringLiteral("checkmate"));
        Services::init(nullptr, nullptr, nullptr, nullptr);
    }

    void liveStream()
    {
        LichessApi api;
        QScopedPointer<NdjsonStream> stream(api.openStream(QStringLiteral("/api/tv/feed")));
        QSignalSpy messages(stream.data(), &NdjsonStream::message);
        QTRY_VERIFY_WITH_TIMEOUT(messages.count() >= 1, 20000);
        const QJsonObject first = messages.first().first().toJsonObject();
        QCOMPARE(first.value("t").toString(), QStringLiteral("featured"));
    }
};

int runLiveTests(int argc, char *argv[])
{
    TestLive test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_live.moc"
