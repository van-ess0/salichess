// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHESSPOSITION_H
#define CHESSPOSITION_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>
#include <memory>

namespace chess { class Board; }

// Value-type wrapper around chess-library's Board. It is the only class that
// includes chess.hpp, so the rules engine can be swapped without touching the
// rest of the app.
//
// Squares are indexed 0..63 with a1 = 0, b1 = 1, ..., h8 = 63.
// Pieces are encoded as FEN characters ('P', 'n', ...), '.' for empty.
class ChessPosition
{
public:
    enum Color { White, Black };
    enum Outcome {
        Ongoing,
        Checkmate,
        Stalemate,
        InsufficientMaterial,
        FiftyMoveRule,
        ThreefoldRepetition
    };

    ChessPosition();
    explicit ChessPosition(const QString &fen);
    ChessPosition(const ChessPosition &other);
    ChessPosition &operator=(const ChessPosition &other);
    ~ChessPosition();

    static QString startFen();

    // Accepts a FEN or "startpos"/empty for the standard start position.
    // Rejects positions with a king missing, pawns on the first or last rank
    // or the side not to move in check. On failure the position is unchanged.
    bool setFen(const QString &fen);
    QString fen() const;

    Color sideToMove() const;
    char pieceAt(int square) const;
    std::array<char, 64> pieces() const;
    bool inCheck() const;
    int kingSquare(Color color) const;
    Outcome outcome() const;

    // Whether |color| has no more than a game can give it: eight pawns, and
    // no more extra queens, rooks, bishops and knights than it has lost
    // pawns to promote. Stockfish relies on it: it keeps room for 32 pieces
    // and not many more moves, and a set-up board with more crashes it.
    static bool hasStandardMaterial(const std::array<char, 64> &board, Color color);
    bool hasStandardMaterial() const;

    // Returns the canonical UCI string ("e1g1" for castling) if the move is
    // legal here, otherwise an empty string. Accepts both the standard and the
    // king-takes-rook castling notations.
    QString normalizeUci(const QString &uci) const;
    QString sanForUci(const QString &uci) const;
    QString uciForSan(const QString &san) const;
    QStringList legalMovesUci() const;

    // Target squares for a piece on |from|. Castling is reported as the king's
    // destination square (g1/c1/g8/c8).
    QVector<int> legalTargets(int from) const;
    bool isPromotion(int from, int to) const;

    // Plays a move given in UCI notation; returns false (and leaves the
    // position unchanged) if the move is illegal.
    bool playUci(const QString &uci);

    static int squareFromName(const QString &name);
    static QString squareName(int square);

private:
    std::unique_ptr<chess::Board> m_board;
};

#endif // CHESSPOSITION_H
