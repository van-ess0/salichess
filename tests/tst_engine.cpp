// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include <algorithm>

#include "chess/chessgame.h"
#include "core/appsettings.h"
#include "engine/enginecontroller.h"
#include "engine/nnueweights.h"
#include "engine/stockfishengine.h"

// Stockfish itself. Searching needs the networks, which are more than 60 MB
// and are not in the repository, so those tests run only where they have
// already been downloaded:
//
//   SALICHESS_NNUE_DIR=~/.local/share/io.github.vaness0/harbour-salichess/nnue
//
// The checks that do not search run everywhere.
class TestEngine : public QObject
{
    Q_OBJECT

private:
    static QString netDir()
    {
        return QString::fromLocal8Bit(qgetenv("SALICHESS_NNUE_DIR"));
    }

    // The two networks, biggest first: which is which is decided by size,
    // not by the order the directory happens to list them in. Handing the
    // small one to Stockfish as the big one makes it refuse both.
    static QStringList networks()
    {
        const QDir dir(netDir());
        QStringList found;
        for (const QString &name : dir.entryList(QStringList() << QStringLiteral("*.nnue"),
                                                 QDir::Files))
            found.append(dir.absoluteFilePath(name));
        std::sort(found.begin(), found.end(), [](const QString &a, const QString &b) {
            return QFileInfo(a).size() > QFileInfo(b).size();
        });
        return found;
    }

private slots:
    void rejectsFilesThatAreNotNetworks()
    {
        // Anything Stockfish would refuse has to be caught before it sees
        // it: a failed load ends the process rather than the search.
        QTemporaryFile empty;
        QVERIFY(empty.open());
        QVERIFY(!NnueWeights::looksUsable(empty.fileName()));

        QTemporaryFile htmlError;
        QVERIFY(htmlError.open());
        htmlError.write(QByteArray("<html>404</html>").repeated(200000));
        htmlError.flush();
        QVERIFY(!NnueWeights::looksUsable(htmlError.fileName()));

        QVERIFY(!NnueWeights::looksUsable(QStringLiteral("/nowhere/nothing.nnue")));
    }

    void refusesToSearchWithoutNetworks()
    {
        StockfishEngine engine;
        QSignalSpy failed(&engine, &StockfishEngine::failed);
        QSignalSpy info(&engine, &StockfishEngine::info);

        engine.load(QStringLiteral("/nowhere/big.nnue"), QStringLiteral("/nowhere/small.nnue"));
        QCOMPARE(failed.count(), 1);
        QVERIFY(!engine.isReady());

        // The search must be dropped rather than reaching Stockfish.
        engine.search(QStringLiteral("startpos"), QStringList(), 4);
        QTest::qWait(200);
        QCOMPARE(info.count(), 0);
    }

    void findsTheBestMove()
    {
        const QStringList nets = networks();
        if (nets.size() < 2)
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");
        const QString big = nets.first();
        const QString small = nets.last();
        QVERIFY(NnueWeights::looksUsable(big));
        QVERIFY(NnueWeights::looksUsable(small));

        StockfishEngine engine;
        QSignalSpy ready(&engine, &StockfishEngine::readyChanged);
        engine.load(big, small);
        QTRY_COMPARE(ready.count(), 1);
        QVERIFY(engine.isReady());

        engine.setMultiPv(2);
        QSignalSpy info(&engine, &StockfishEngine::info);
        QSignalSpy finished(&engine, &StockfishEngine::searchFinished);

        // White mates in one with Qxf7.
        engine.search(QStringLiteral("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 4 4"),
                      QStringList(), 10);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 60000);
        QVERIFY(info.count() > 0);

        // The last line reported for the first variation is the mate.
        int mate = 0;
        QString pv;
        for (const QList<QVariant> &line : info) {
            if (line.at(1).toInt() != 1)
                continue;
            mate = line.at(3).toInt();
            pv = line.at(4).toString();
        }
        QCOMPARE(mate, 1);
        QVERIFY(pv.startsWith(QStringLiteral("f3f7")));

        // Two variations were asked for, so a second one was reported.
        bool second = false;
        for (const QList<QVariant> &line : info)
            second = second || line.at(1).toInt() == 2;
        QVERIFY(second);
    }

    // The engine rests while the app is in the background. It has to pick
    // the same board up again afterwards: taking the board away instead left
    // it stopped for as long as the page stayed open.
    void resumesAfterTheAppComesBack()
    {
        const QStringList nets = networks();
        if (nets.size() < 2)
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");

        // Put the networks where NnueWeights looks, so nothing is downloaded.
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/nnue");
        QDir().mkpath(dir);
        AppSettings settings;
        NnueWeights weights;
        for (const QString &net : nets) {
            const QString target = net.endsWith(QFileInfo(weights.bigPath()).fileName())
                    ? weights.bigPath() : weights.smallPath();
            QFile::remove(target);
            QVERIFY(QFile::link(net, target));
        }
        QVERIFY(weights.ready());

        settings.setEngineEnabled(true);
        settings.setEngineDepth(10);
        EngineController engine(&settings, &weights);
        ChessGame game;
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        engine.setGame(&game);
        QTRY_VERIFY_WITH_TIMEOUT(engine.hasEval(), 90000);

        engine.setApplicationActive(false);
        QTRY_VERIFY(!engine.searching());
        // The board it was given is still its board.
        QCOMPARE(engine.game(), &game);
        QVERIFY(!engine.hasEval());

        engine.setApplicationActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(engine.hasEval(), 90000);
        QVERIFY(!engine.lines().isEmpty());
    }

    // A game that ended in mate opens the analysis board on a position with
    // no legal moves. Stockfish reports that through a callback it calls
    // without checking, so leaving any of its callbacks unset took the whole
    // app down here, not just the search.
    void handlesPositionsWithNoMoves()
    {
        const QStringList nets = networks();
        if (nets.size() < 2)
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");
        StockfishEngine engine;
        engine.load(nets.first(), nets.last());
        QVERIFY(engine.isReady());

        QSignalSpy info(&engine, &StockfishEngine::info);
        QSignalSpy finished(&engine, &StockfishEngine::searchFinished);

        // Fool's mate: white is to move and has been mated.
        engine.search(QStringLiteral("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"),
                      QStringList(), 8);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
        QVERIFY(info.count() > 0);
        // Mated, so the score is a mate against the side to move.
        QVERIFY(info.last().at(3).toInt() < 0);

        // Stalemate: black to move, not in check, no legal move.
        finished.clear();
        info.clear();
        engine.search(QStringLiteral("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"), QStringList(), 8);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 30000);
        QVERIFY(info.count() > 0);
        QCOMPARE(info.last().at(2).toInt(), 0); // a draw is worth nothing
        QCOMPARE(info.last().at(3).toInt(), 0);
    }

    void stopsOnDemand()
    {
        const QStringList nets = networks();
        if (nets.size() < 2)
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");
        StockfishEngine engine;
        engine.load(nets.first(), nets.last());
        QVERIFY(engine.isReady());

        QSignalSpy finished(&engine, &StockfishEngine::searchFinished);
        engine.search(QStringLiteral("startpos"), QStringList(), 40);
        QTest::qWait(300);
        engine.stop();
        // Stopping waits for the search to end, so a new one can start at once.
        engine.search(QStringLiteral("startpos"), QStringList(), 6);
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, 60000);
    }
};

int runEngineTests(int argc, char *argv[])
{
    TestEngine test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_engine.moc"
