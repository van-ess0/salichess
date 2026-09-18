// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "chess/hotseatcontroller.h"
#include "chess/movetree.h"
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

    void unplayableFensRejected()
    {
        // The rules engine reads out of bounds without both kings.
        const char *const fens[] = {
            "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w - - 0 1",    // no kings
            "rnbq1bnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1BNR w KQkq - 0 1", // ... with castling rights
            "4k3/8/8/8/8/8/8/3KK3 w - - 0 1",                           // two white kings
            "4k2P/8/8/8/8/8/8/4K3 w - - 0 1",                           // pawn on the 8th rank
            "4k3/8/8/8/8/8/8/p3K3 b - - 0 1",                           // pawn on the 1st rank
            "4k3/8/8/8/8/8/8/4K2r b - - 0 1",                           // side not to move in check
            "4k3/8/8/8/8/8/4K3 w - - 0 1",                              // seven ranks
            "garbage",
        };
        for (const char *fen : fens) {
            ChessPosition position;
            QVERIFY2(!position.setFen(QString::fromLatin1(fen)), fen);
            QCOMPARE(position.fen(), ChessPosition::startFen()); // left unchanged
        }

        ChessGame game;
        game.reset(QString::fromLatin1(fens[0]));
        QCOMPARE(game.fen(), ChessPosition::startFen());
        QVERIFY(ChessPosition().setFen(QStringLiteral("4k3/8/8/8/8/8/8/4K2r w - - 0 1"))); // in check, to move
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
        game.goToPly(0);
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

    void materialBalance()
    {
        ChessGame game;
        QVERIFY(game.whiteMaterial().isEmpty());
        QVERIFY(game.blackMaterial().isEmpty());
        QCOMPARE(game.materialScore(), 0);

        QVERIFY(game.setUciMoves({ "e2e4", "d7d5", "e4d5" }));
        QCOMPARE(game.whiteMaterial(), QStringList({ "bP" }));
        QVERIFY(game.blackMaterial().isEmpty());
        QCOMPARE(game.materialScore(), 1);

        // It follows the position being viewed.
        game.viewPrevious();
        QCOMPARE(game.materialScore(), 0);
        game.viewLatest();

        // Types are compared one by one, so promotions count.
        game.reset(QStringLiteral("4k3/pp6/8/8/8/8/8/QQ2K3 w - - 0 1"));
        QCOMPARE(game.whiteMaterial(), QStringList({ "bQ", "bQ" }));
        QCOMPARE(game.blackMaterial(), QStringList({ "wP", "wP" }));
        QCOMPARE(game.materialScore(), 16);

        game.reset(QStringLiteral("r3k3/8/8/8/8/8/8/1N2K3 w - - 0 1"));
        QCOMPARE(game.whiteMaterial(), QStringList({ "bN" }));
        QCOMPARE(game.blackMaterial(), QStringList({ "wR" }));
        QCOMPARE(game.materialScore(), -2);
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

    // --- Side lines ---

    void variationsAreOffByDefault()
    {
        // A game or a puzzle board: a move is always the next move of the
        // game, even while an earlier position is on show.
        ChessGame game;
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        game.goToPly(1);
        QVERIFY(game.playUci("b8c6"));
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5", "Nf3", "Nc6" }));
        QVERIFY(!game.inVariation());
        // The view stays where it was put.
        QCOMPARE(game.viewPly(), 1);
    }

    void variationBranchesAndReturns()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3", "b8c6" }));

        // Playing something else at move two starts a side line, and the
        // board follows it.
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        QVERIFY(game.inVariation());
        QCOMPARE(game.variationStartPly(), 2);
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5" }));
        QCOMPARE(game.viewPly(), 2);
        QCOMPARE(game.ply(), 2);

        // The side line carries on.
        QVERIFY(game.playUci("g1f3"));
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5", "Nf3" }));

        // The game itself is untouched underneath.
        game.exitVariation();
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5", "Nf3", "Nc6" }));
        QCOMPARE(game.viewPly(), 1);
    }

    void variationPlayedTwiceIsTheSameLine()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        const int nodes = game.nodeCount();

        // Going back and playing the same move again re-enters the line
        // rather than adding a second copy of it.
        game.exitVariation();
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        QCOMPARE(game.nodeCount(), nodes);
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5" }));

        // Replaying the game's own move just follows the game.
        game.exitVariation();
        game.goToPly(1);
        QVERIFY(game.playUci("e7e5"));
        QVERIFY(!game.inVariation());
        QCOMPARE(game.nodeCount(), nodes);
    }

    void variationCanBePromotedAndDeleted()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        QVERIFY(game.playUci("g1f3"));

        // Promoting makes the side line the game, and the game the side line.
        game.promoteVariation();
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5", "Nf3" }));
        game.goToPly(1);
        QVERIFY(game.playUci("e7e5"));
        QVERIFY(game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5", "Nf3" }));

        // Deleting throws the line away and goes back to the game.
        game.deleteVariation();
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5", "Nf3" }));
        QCOMPARE(game.viewPly(), 1);

        // The deleted nodes are reused rather than leaked.
        const int nodes = game.nodeCount();
        game.goToPly(1);
        QVERIFY(game.playUci("e7e6"));
        QCOMPARE(game.nodeCount(), nodes);
    }

    void variationsSurviveAGameUpdate()
    {
        // A side line hangs off a move that a takeback removes: the game is
        // what the server says, so the line goes with it.
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        game.goToPly(2);
        QVERIFY(game.playUci("d2d4"));
        QVERIFY(game.inVariation());

        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5" }));
        QCOMPARE(game.ply(), 2);
    }

    void boardInputFollowsTheViewedPosition()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        const int e2 = ChessPosition::squareFromName("e2");
        const int e4 = ChessPosition::squareFromName("e4");

        // At the end of the game the pawn stands on e4.
        QCOMPARE(game.pieceAt(e4), QStringLiteral("wP"));
        QVERIFY(game.pieceAt(e2).isEmpty());
        QCOMPARE(game.viewSideToMove(), QStringLiteral("black"));

        // Back at the start it stands on e2 again and can move there.
        game.viewFirst();
        QCOMPARE(game.pieceAt(e2), QStringLiteral("wP"));
        QCOMPARE(game.viewSideToMove(), QStringLiteral("white"));
        QCOMPARE(game.uciForMove(e2, e4), QStringLiteral("e2e4"));
        // sideToMove stays the side to move in the game itself.
        QCOMPARE(game.sideToMove(), QStringLiteral("black"));
    }

    void moveTreeIsWalkable()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));

        const QVector<int> firstMoves = game.nodeChildren(ChessGame::rootNode());
        QCOMPARE(firstMoves.size(), 1);
        const QVector<int> replies = game.nodeChildren(firstMoves.first());
        QCOMPARE(replies.size(), 2);
        QCOMPARE(game.nodeSan(replies.at(0)), QStringLiteral("e5"));  // the game
        QCOMPARE(game.nodeSan(replies.at(1)), QStringLiteral("c5"));  // the side line
        QCOMPARE(game.nodeDepth(replies.at(1)), 2);
        QCOMPARE(game.nodeParent(replies.at(1)), firstMoves.first());

        // Jumping to a node switches to its line.
        game.goToNode(replies.at(0));
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5" }));
        game.goToNode(replies.at(1));
        QVERIFY(game.inVariation());
        QCOMPARE(game.viewPly(), 2);
        QCOMPARE(game.currentNode(), replies.at(1));
    }


    void moveTreeReadsLikeAPgn()
    {
        ChessGame game;
        game.setAllowVariations(true);
        MoveTree tree;
        tree.setGame(&game);

        QVERIFY(game.setUciMoves({ "e2e4", "e7e5", "g1f3" }));
        QCOMPARE(tree.paragraphs().size(), 1);
        QVERIFY(!tree.hasVariations());
        QVariantMap run = tree.paragraphs().first().toMap();
        QCOMPARE(run.value("depth").toInt(), 0);
        QCOMPARE(run.value("moves").toList().size(), 3);
        QVariantMap first = run.value("moves").toList().first().toMap();
        QCOMPARE(first.value("san").toString(), QStringLiteral("e4"));
        QCOMPARE(first.value("ply").toInt(), 1);
        QVERIFY(first.value("white").toBool());
        QVERIFY(first.value("first").toBool());

        // A side line becomes a run of its own, one step further in.
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        QVERIFY(game.playUci("g1f3"));
        QCOMPARE(tree.paragraphs().size(), 2);
        QVERIFY(tree.hasVariations());
        const QVariantMap side = tree.paragraphs().at(1).toMap();
        QCOMPARE(side.value("depth").toInt(), 1);
        QCOMPARE(side.value("moves").toList().size(), 2);
        QCOMPARE(side.value("moves").toList().first().toMap().value("san").toString(),
                 QStringLiteral("c5"));

        // The move on the board is reported apart from the tree, so stepping
        // through the game does not rebuild it.
        const int sideNode = side.value("moves").toList().first().toMap().value("node").toInt();
        QSignalSpy rebuilds(&tree, &MoveTree::paragraphsChanged);
        QSignalSpy current(&tree, &MoveTree::currentNodeChanged);
        game.viewPrevious(); // from Nf3 of the side line back to c5
        QCOMPARE(rebuilds.count(), 0);
        QVERIFY(current.count() > 0);
        QCOMPARE(tree.currentNode(), sideNode);

        // Jumping to the game's own second move leaves the side line.
        const int gameSecond = run.value("moves").toList().at(1).toMap().value("node").toInt();
        game.goToNode(gameSecond);
        QCOMPARE(tree.currentNode(), gameSecond);
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5", "Nf3" }));
    }

    void undoRemovesOnlyTheLastMove()
    {
        ChessGame game;
        game.setAllowVariations(true);
        QVERIFY(game.setUciMoves({ "e2e4", "e7e5" }));
        game.goToPly(1);
        QVERIFY(game.playUci("c7c5"));
        QVERIFY(game.playUci("g1f3"));

        game.undo();
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "c5" }));
        game.undo();
        // Back on the game, since the side line is gone.
        QVERIFY(!game.inVariation());
        QCOMPARE(game.sanMoves(), QStringList({ "e4", "e5" }));
    }

    // --- Hotseat: two players sharing one phone ---

    void hotseatClockChangesHandsOnTheButton()
    {
        HotseatController hotseat;
        hotseat.setInitialSeconds(60);
        hotseat.setIncrement(2);
        hotseat.start();
        QCOMPARE(hotseat.state(), HotseatController::Running);
        QCOMPARE(hotseat.runningClock(), QStringLiteral("white"));
        QVERIFY(hotseat.acceptsMoves());

        hotseat.move(QStringLiteral("e2e4"));
        // The move alone does not hand the clock over: white still owes a
        // press, and nothing can be played in the meantime.
        QCOMPARE(hotseat.awaitingPress(), QStringLiteral("white"));
        QCOMPARE(hotseat.runningClock(), QStringLiteral("white"));
        QVERIFY(!hotseat.acceptsMoves());
        hotseat.move(QStringLiteral("e7e5"));
        QCOMPARE(hotseat.game()->ply(), 1);

        hotseat.pressClock();
        QCOMPARE(hotseat.runningClock(), QStringLiteral("black"));
        QVERIFY(hotseat.awaitingPress().isEmpty());
        QVERIFY(hotseat.acceptsMoves());
        // The increment went to the side that pressed, and only to it.
        QVERIFY(hotseat.whiteTime() > 60000);
        QVERIFY(hotseat.blackTime() <= 60000);
    }

    void hotseatAutoSwitchNeedsNoPress()
    {
        HotseatController hotseat;
        hotseat.setInitialSeconds(60);
        hotseat.setAutoSwitch(true);
        hotseat.start();

        hotseat.move(QStringLiteral("e2e4"));
        QVERIFY(hotseat.awaitingPress().isEmpty());
        QCOMPARE(hotseat.runningClock(), QStringLiteral("black"));
        QVERIFY(hotseat.acceptsMoves());
        hotseat.move(QStringLiteral("e7e5"));
        QCOMPARE(hotseat.game()->sanMoves(), QStringList({ "e4", "e5" }));
    }

    void hotseatCheckmateEndsTheGame()
    {
        HotseatController hotseat;
        hotseat.setInitialSeconds(0); // untimed: there is nothing to press
        hotseat.start();

        // Fool's mate.
        hotseat.move(QStringLiteral("f2f3"));
        hotseat.move(QStringLiteral("e7e5"));
        hotseat.move(QStringLiteral("g2g4"));
        hotseat.move(QStringLiteral("d8h4"));
        QVERIFY(hotseat.gameOver());
        QCOMPARE(hotseat.status(), QStringLiteral("checkmate"));
        QCOMPARE(hotseat.winner(), QStringLiteral("black"));
        QVERIFY(!hotseat.acceptsMoves());

        // Taking the mate back puts the game back on.
        hotseat.undoMove();
        QVERIFY(!hotseat.gameOver());
        QVERIFY(hotseat.acceptsMoves());
        QCOMPARE(hotseat.sideToMove(), QStringLiteral("black"));
    }

    void hotseatFlagLosesOnTime()
    {
        HotseatController hotseat;
        hotseat.setInitialSeconds(1);
        hotseat.setAutoSwitch(true);
        hotseat.start();

        QTRY_COMPARE_WITH_TIMEOUT(hotseat.state(), HotseatController::Finished, 3000);
        QCOMPARE(hotseat.status(), QStringLiteral("timeout"));
        QCOMPARE(hotseat.winner(), QStringLiteral("black"));
        QCOMPARE(hotseat.whiteTime(), 0);
    }

    void hotseatFlagIsADrawWithoutMatingMaterial()
    {
        HotseatController hotseat;
        hotseat.setInitialSeconds(1);
        hotseat.start(QStringLiteral("8/8/8/4k3/8/8/4K3/8 w - - 0 1"));

        QTRY_COMPARE_WITH_TIMEOUT(hotseat.state(), HotseatController::Finished, 3000);
        QCOMPARE(hotseat.status(), QStringLiteral("timeout"));
        // Black has a bare king and could never mate: nobody wins.
        QVERIFY(hotseat.winner().isEmpty());
    }
};

int runChessTests(int argc, char *argv[])
{
    TestChess test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_chess.moc"
