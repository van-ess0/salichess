// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "puzzlelogic.h"

#include "chess/chessposition.h"

void PuzzleLogic::start(const QStringList &solution)
{
    m_solution = solution;
    m_step = 0;
    m_failed = false;
}

PuzzleLogic::Verdict PuzzleLogic::playerMove(const ChessPosition &before, const QString &uci)
{
    if (finished())
        return Wrong;

    const QString played = before.normalizeUci(uci);
    if (played.isEmpty())
        return Wrong;

    const QString expected = before.normalizeUci(m_solution.at(m_step));
    if (played == expected) {
        ++m_step;
        return finished() ? Solved : Correct;
    }

    ChessPosition after(before);
    after.playUci(played);
    if (after.outcome() == ChessPosition::Checkmate) {
        m_step = m_solution.size();
        return Solved;
    }

    m_failed = true;
    return Wrong;
}

QString PuzzleLogic::nextReply() const
{
    return finished() ? QString() : m_solution.at(m_step);
}

void PuzzleLogic::replyPlayed()
{
    if (!finished())
        ++m_step;
}

QString PuzzleLogic::expectedMove() const
{
    return finished() ? QString() : m_solution.at(m_step);
}
