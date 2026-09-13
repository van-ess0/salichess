// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MOVETREE_H
#define MOVETREE_H

#include <QObject>
#include <QVariantList>

class ChessGame;

// The moves of a ChessGame laid out for reading, the way a PGN reads: the
// game itself as one run of moves, and every side line as an indented run
// underneath the move it branches off from.
//
// "paragraphs" is a list of runs, each {"depth", "moves"}, where a move is
// {"node", "san", "ply", "white", "first"}: |node| is the id to hand back to
// ChessGame::goToNode(), |white| says whether white played it, and |first|
// marks the move a run starts with, which is the one that needs its move
// number written out.
//
// The list only changes when the shape of the tree does. Which move is on
// the board is currentNode, so stepping through a game does not rebuild it.
class MoveTree : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ChessGame *game READ game WRITE setGame NOTIFY gameChanged)
    Q_PROPERTY(QVariantList paragraphs READ paragraphs NOTIFY paragraphsChanged)
    Q_PROPERTY(int currentNode READ currentNode NOTIFY currentNodeChanged)
    Q_PROPERTY(bool hasVariations READ hasVariations NOTIFY paragraphsChanged)

public:
    explicit MoveTree(QObject *parent = nullptr);

    ChessGame *game() const { return m_game; }
    void setGame(ChessGame *game);
    QVariantList paragraphs() const { return m_paragraphs; }
    int currentNode() const;
    bool hasVariations() const { return m_paragraphs.size() > 1; }

signals:
    void gameChanged();
    void paragraphsChanged();
    void currentNodeChanged();

private:
    void rebuild();
    // Walks the line starting at |node|, appending its own run and, after
    // each move, the runs of the lines that branch off it.
    void appendRun(int node, int depth);

    ChessGame *m_game = nullptr;
    QVariantList m_paragraphs;
};

#endif // MOVETREE_H
