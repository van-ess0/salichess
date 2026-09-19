// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "movetree.h"

#include "chessgame.h"

MoveTree::MoveTree(QObject *parent)
    : QObject(parent)
{
}

void MoveTree::setGame(ChessGame *game)
{
    if (m_game == game)
        return;
    if (m_game)
        m_game->disconnect(this);
    m_game = game;
    if (m_game) {
        connect(m_game, &ChessGame::treeChanged, this, &MoveTree::rebuild);
        connect(m_game, &ChessGame::positionChanged, this, &MoveTree::currentNodeChanged);
        connect(m_game, &QObject::destroyed, this, [this]() {
            m_game = nullptr;
            rebuild();
            emit gameChanged();
        });
    }
    emit gameChanged();
    rebuild();
}

int MoveTree::currentNode() const
{
    return m_game ? m_game->currentNode() : ChessGame::rootNode();
}

void MoveTree::rebuild()
{
    m_paragraphs.clear();
    if (m_game) {
        const QVector<int> firstMoves = m_game->nodeChildren(ChessGame::rootNode());
        for (int i = 0; i < firstMoves.size(); ++i)
            appendRun(firstMoves.at(i), i == 0 ? 0 : 1);
    }
    emit paragraphsChanged();
    emit currentNodeChanged();
}

void MoveTree::appendRun(int node, int depth)
{
    QVariantList moves;
    QVector<QPair<int, int>> branches; // side lines found along the way

    const int startPly = m_game->startPly();
    for (int at = node; m_game->nodeExists(at); ) {
        const int ply = m_game->nodeDepth(at);
        const int played = startPly + ply; // counted from the very first move
        QVariantMap move;
        move.insert(QStringLiteral("node"), at);
        move.insert(QStringLiteral("san"), m_game->nodeSan(at));
        move.insert(QStringLiteral("ply"), ply);
        move.insert(QStringLiteral("number"), (played + 1) / 2);
        move.insert(QStringLiteral("white"), played % 2 != 0);
        move.insert(QStringLiteral("first"), moves.isEmpty());
        moves.append(move);

        const QVector<int> children = m_game->nodeChildren(at);
        for (int i = 1; i < children.size(); ++i)
            branches.append(qMakePair(children.at(i), depth + 1));
        at = children.isEmpty() ? -1 : children.first();
    }

    QVariantMap paragraph;
    paragraph.insert(QStringLiteral("depth"), depth);
    paragraph.insert(QStringLiteral("moves"), moves);
    m_paragraphs.append(paragraph);

    // The side lines come after the run they branch off, deeper in.
    for (const QPair<int, int> &branch : branches)
        appendRun(branch.first, branch.second);
}
