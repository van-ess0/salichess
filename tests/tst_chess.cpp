// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "chess/piecesmodel.h"
#include "lichess/puzzlecontroller.h"
#include "lichess/puzzlelogic.h"
#include "testdata.h"

namespace {

QString fenWithoutCounters(const QString &fen)
{
    return fen.section(QLatin1Char(' '), 0, 3);
}

} // namespace

// Chess rules, game history and puzzle solving; no network involved.
class TestChess : public QObject
{
    Q_OBJECT

private slots:
    void startPosition()
    {
        ChessPosition position;
        QCOMPARE(position.legalMovesUci().size(), 20);
        QCOMPARE(position.sideToMove(), ChessPosition::White);
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("e1")), 'K');
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("d8")), 'q');
        QCOMPARE(position.outcome(), ChessPosition::Ongoing);
    }

    void castlingNotations()
    {
        ChessPosition position(QStringLiteral("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
        QCOMPARE(position.normalizeUci("e1g1"), QStringLiteral("e1g1"));
        QCOMPARE(position.normalizeUci("e1h1"), QStringLiteral("e1g1"));
        QCOMPARE(position.normalizeUci("e1a1"), QStringLiteral("e1c1"));
        const QVector<int> targets = position.legalTargets(ChessPosition::squareFromName("e1"));
        QVERIFY(targets.contains(ChessPosition::squareFromName("g1")));
        QVERIFY(targets.contains(ChessPosition::squareFromName("c1")));
        QVERIFY(position.playUci("e1h1"));
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("g1")), 'K');
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("f1")), 'R');
    }

    void promotion()
    {
        ChessPosition position(QStringLiteral("8/P7/8/8/8/8/8/k6K w - - 0 1"));
        const int a7 = ChessPosition::squareFromName("a7");
        const int a8 = ChessPosition::squareFromName("a8");
        QVERIFY(position.isPromotion(a7, a8));
        QVERIFY(position.normalizeUci("a7a8").isEmpty());
        QCOMPARE(position.sanForUci("a7a8n"), QStringLiteral("a8=N"));
        QVERIFY(position.playUci("a7a8q"));
        QCOMPARE(position.pieceAt(a8), 'Q');
    }

    void illegalMovesRejected()
    {
        ChessPosition position;
        QVERIFY(!position.playUci("e2e5"));
        QVERIFY(!position.playUci("garbage"));
        QVERIFY(position.uciForSan("Ke2").isEmpty());
        QCOMPARE(position.fen(), ChessPosition::startFen());
    }

    void replayPgn()
    {
        ChessGame game;
        const QJsonObject data = QJsonDocument::fromJson(DailyPuzzle).object();
        QVERIFY(game.playSanMoves(data.value("game").toObject().value("pgn").toString()));
        QCOMPARE(game.ply(), 73);
        QCOMPARE(fenWithoutCounters(game.fen()), QString::fromLatin1(DailyPuzzleFen));
        QCOMPARE(game.sideToMove(), QStringLiteral("black"));
        QCOMPARE(game.sanMoves().last(), QStringLiteral("Ke3"));
    }

    void gameHistoryAndSync()
    {
        ChessGame game;
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5", "Nf3" }));
        QCOMPARE(game.lastMoveTo(), ChessPosition::squareFromName("f3"));

        game.viewPrevious();
        QCOMPARE(game.viewPly(), 2);
        QVERIFY(!game.atLatest());
        game.viewLatest();

        // A takeback: the server list is shorter.
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        QCOMPARE(game.ply(), 2);
        QCOMPARE(game.sideToMove(), QStringLiteral("white"));

        // A diverging list is rebuilt from the common prefix.
        QVERIFY(game.setUciMoves({ "e2e4", "c7c5" }));
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5" }));

        QVERIFY(!game.setUciMoves({ "e2e4", "c7c5", "e1e8" }));
        QCOMPARE(game.ply(), 2);
    }

    void piecesModelKeepsIdentity()
    {
        ChessGame game;
        PiecesModel *pieces = game.pieces();
        QCOMPARE(pieces->rowCount(), 32);

        auto rowOn = [pieces](const char *square) {
            for (int row = 0; row < pieces->rowCount(); ++row) {
                if (pieces->data(pieces->index(row), PiecesModel::SquareRole).toInt()
                        == ChessPosition::squareFromName(square))
                    return row;
            }
            return -1;
        };

        const int pawnRow = rowOn("e2");
        QVERIFY(pawnRow >= 0);
        QVERIFY(game.playUci("e2e4"));
        QCOMPARE(rowOn("e4"), pawnRow);
        QCOMPARE(pieces->rowCount(), 32);

        QVERIFY(game.setUciMoves({ "e2e4", "d7d5", "e4d5" }));
        QCOMPARE(pieces->rowCount(), 31);
        QCOMPARE(rowOn("d5"), pawnRow);
        QCOMPARE(pieces->data(pieces->index(pawnRow), PiecesModel::PieceRole).toString(), QStringLiteral("wP"));
    }

    void puzzleLogicSolves()
    {
        ChessGame game;
        const QJsonObject data = QJsonDocument::fromJson(DailyPuzzle).object();
        QVERIFY(game.playSanMoves(data.value("game").toObject().value("pgn").toString()));

        PuzzleLogic logic;
        logic.start({ "b1b3", "d2d3", "f8f3", "e3f3", "b3d3" });

        QCOMPARE(logic.playerMove(game.position(), "b1b2"), PuzzleLogic::Wrong);
        QVERIFY(logic.failed());

        QCOMPARE(logic.playerMove(game.position(), "b1b3"), PuzzleLogic::Correct);
        game.playUci("b1b3");
        QCOMPARE(logic.nextReply(), QStringLiteral("d2d3"));
        game.playUci(logic.nextReply());
        logic.replyPlayed();

        QCOMPARE(logic.playerMove(game.position(), "f8f3"), PuzzleLogic::Correct);
        game.playUci("f8f3");
        game.playUci(logic.nextReply());
        logic.replyPlayed();

        QCOMPARE(logic.playerMove(game.position(), "b3d3"), PuzzleLogic::Solved);
        QVERIFY(logic.finished());
    }

    void puzzleAcceptsAlternativeMate()
    {
        ChessPosition position(QStringLiteral("6k1/5ppp/8/8/8/8/8/RR4K1 w - - 0 1"));
        PuzzleLogic logic;
        logic.start({ "a1a8" });
        QCOMPARE(logic.playerMove(position, "b1b8"), PuzzleLogic::Solved);
        QVERIFY(!logic.failed());
    }

    void puzzleController()
    {
        PuzzleController controller;
        QVERIFY(controller.startPuzzle(QJsonDocument::fromJson(DailyPuzzle).object(), true));
        QCOMPARE(controller.playerColor(), QStringLiteral("black"));
        QCOMPARE(controller.puzzleId(), QStringLiteral("1Sqyb"));
        QCOMPARE(controller.state(), PuzzleController::Playing);

        ChessGame *game = controller.game();
        QCOMPARE(game->ply(), 72);
        QTRY_COMPARE(game->ply(), 73); // opponent's lead-in move

        controller.move("b1b3");
        QCOMPARE(controller.feedback(), PuzzleController::GoodMove);
        QTRY_COMPARE(game->ply(), 75);

        controller.move("h7h6"); // legal but wrong: gets taken back
        QCOMPARE(controller.feedback(), PuzzleController::WrongMove);
        QVERIFY(controller.hadMistake());
        QTRY_COMPARE(game->ply(), 75);

        controller.move("f8f3");
        QTRY_COMPARE(game->ply(), 77);
        controller.move("b3d3");
        QCOMPARE(controller.state(), PuzzleController::Finished);
        QCOMPARE(controller.feedback(), PuzzleController::Success);
        QVERIFY(!controller.solved()); // there was a mistake
    }

    void puzzleHint()
    {
        PuzzleController controller;
        QVERIFY(controller.startPuzzle(QJsonDocument::fromJson(DailyPuzzle).object(), false));
        QTRY_COMPARE(controller.game()->ply(), 73);
        controller.showHint();
        QCOMPARE(controller.hintSquare(), ChessPosition::squareFromName("b1"));
        QCOMPARE(controller.hintTarget(), -1);
        controller.showHint();
        QCOMPARE(controller.hintTarget(), ChessPosition::squareFromName("b3"));
        controller.move("b1b3");
        QCOMPARE(controller.hintSquare(), -1);
        QCOMPARE(controller.hintTarget(), -1);
    }

    void puzzleSolutionPlayback()
    {
        PuzzleController controller;
        QVERIFY(controller.startPuzzle(QJsonDocument::fromJson(DailyPuzzle).object(), false));
        QTRY_COMPARE(controller.game()->ply(), 73);
        controller.viewSolution();
        QCOMPARE(controller.state(), PuzzleController::ShowingSolution);
        QTRY_COMPARE_WITH_TIMEOUT(controller.state(), PuzzleController::Finished, 10000);
        QCOMPARE(controller.game()->ply(), 78);
        QCOMPARE(controller.game()->sanMoves().last(), QStringLiteral("Qxd3+"));
        QVERIFY(!controller.solved());
    }

    void enPassant()
    {
        ChessPosition position(QStringLiteral("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"));
        QCOMPARE(position.sanForUci("e5d6"), QStringLiteral("exd6"));
        QVERIFY(position.playUci("e5d6"));
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("d5")), '.');
        QCOMPARE(position.pieceAt(ChessPosition::squareFromName("d6")), 'P');
    }

    void outcomes()
    {
        QCOMPARE(ChessPosition(QStringLiteral("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")).outcome(), ChessPosition::Stalemate);
        QCOMPARE(ChessPosition(QStringLiteral("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1")).outcome(), ChessPosition::Ongoing);
        QCOMPARE(ChessPosition(QStringLiteral("R5k1/5ppp/8/8/8/8/8/6K1 b - - 0 1")).outcome(), ChessPosition::Checkmate);
        QCOMPARE(ChessPosition(QStringLiteral("8/8/4k3/8/8/2K5/8/5N2 w - - 0 1")).outcome(), ChessPosition::InsufficientMaterial);
    }

    void noCastlingThroughCheck()
    {
        // The black rook on f8 covers f1.
        ChessPosition position(QStringLiteral("5r1k/8/8/8/8/8/8/4K2R w K - 0 1"));
        QVERIFY(!position.legalTargets(ChessPosition::squareFromName("e1")).contains(ChessPosition::squareFromName("g1")));
        QVERIFY(position.normalizeUci("e1g1").isEmpty());
    }

    void sanWithMoveNumbers()
    {
        ChessGame game;
        QVERIFY(game.playSanMoves(QStringLiteral("1. e4 e5 2.Nf3 Nc6 3. Bb5")));
        QCOMPARE(game.uciMoves(), QStringList({ "e2e4", "e7e5", "g1f3", "b8c6", "f1b5" }));
        QVERIFY(!game.playSanMoves(QStringLiteral("Qxh7")));
        QCOMPARE(game.ply(), 5);
    }

    void firstViewablePly()
    {
        ChessGame game;
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3", "b8c6" }));
        game.setFirstViewablePly(2);
        game.viewFirst();
        QCOMPARE(game.viewPly(), 2);
        game.viewPrevious();
        QCOMPARE(game.viewPly(), 2);
        game.viewPly(0);
        QCOMPARE(game.viewPly(), 2);
        game.viewLatest();
        QCOMPARE(game.viewPly(), 4);
        game.reset();
        QCOMPARE(game.firstViewablePly(), 0);
    }

    void piecesModelPromotionAndCastling()
    {
        ChessGame game;
        game.reset(QStringLiteral("4k3/1P6/8/8/8/8/8/4K2R w K - 0 1"));
        PiecesModel *pieces = game.pieces();
        QCOMPARE(pieces->rowCount(), 4);

        auto pieceOn = [pieces](const char *square) {
            for (int row = 0; row < pieces->rowCount(); ++row) {
                if (pieces->data(pieces->index(row), PiecesModel::SquareRole).toInt()
                        == ChessPosition::squareFromName(square))
                    return pieces->data(pieces->index(row), PiecesModel::PieceRole).toString();
            }
            return QString();
        };

        QVERIFY(game.playUci("b7b8n"));
        QCOMPARE(pieces->rowCount(), 4);
        QCOMPARE(pieceOn("b8"), QStringLiteral("wN"));
        QCOMPARE(pieceOn("b7"), QString());

        QVERIFY(game.playUci("e8f7"));
        QVERIFY(game.playUci("e1g1"));
        QCOMPARE(pieceOn("g1"), QStringLiteral("wK"));
        QCOMPARE(pieceOn("f1"), QStringLiteral("wR"));
        QCOMPARE(game.sanMoves().last(), QStringLiteral("O-O+")); // the rook checks the king on f7
    }

    void uciForBoardInput()
    {
        ChessGame game;
        const int e2 = ChessPosition::squareFromName("e2");
        QCOMPARE(game.pieceAt(e2), QStringLiteral("wP"));
        QCOMPARE(game.legalTargets(e2).size(), 2);
        QCOMPARE(game.uciForMove(e2, ChessPosition::squareFromName("e4")), QStringLiteral("e2e4"));
        QVERIFY(game.uciForMove(e2, ChessPosition::squareFromName("e5")).isEmpty());
    }
};

int runChessTests(int argc, char *argv[])
{
    TestChess test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_chess.moc"
