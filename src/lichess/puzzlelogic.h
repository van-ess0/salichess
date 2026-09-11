// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PUZZLELOGIC_H
#define PUZZLELOGIC_H

#include <QString>
#include <QStringList>

class ChessPosition;

// Rules for solving a Lichess puzzle, independent of UI and network.
//
// The solution alternates player move, opponent reply, player move, ...
// starting with the player. Like on Lichess, a different move that delivers
// checkmate also solves the puzzle.
class PuzzleLogic
{
public:
    enum Verdict {
        Wrong,
        Correct,   // right move; the opponent reply is nextReply()
        Solved     // right move and nothing left to find
    };

    void start(const QStringList &solution);

    // |before| is the position before the player's move |uci|.
    Verdict playerMove(const ChessPosition &before, const QString &uci);

    // Opponent reply that follows a Correct verdict; call replyPlayed() once
    // it's on the board.
    QString nextReply() const;
    void replyPlayed();

    // The move the player should find now (for hints and "view solution").
    QString expectedMove() const;
    bool finished() const { return m_step >= m_solution.size(); }
    // Set as soon as the player makes one wrong move.
    bool failed() const { return m_failed; }
    void markFailed() { m_failed = true; }
    int step() const { return m_step; }

private:
    QStringList m_solution;
    int m_step = 0;
    bool m_failed = false;
};

#endif // PUZZLELOGIC_H
