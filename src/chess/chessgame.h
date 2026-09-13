// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHESSGAME_H
#define CHESSGAME_H

#include "chessposition.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class PiecesModel;

// A game as a start position plus the moves played from it, with the ability
// to browse earlier positions. This is what the QML ChessBoard displays;
// controllers (online games, puzzles, analysis) own one and feed moves in.
//
// The moves form a tree: besides the game itself there can be side lines,
// the "what if I had played this?" of the analysis board. One root-to-leaf
// path is the current line, and that is what ply(), sanMoves() and the view
// functions work on, so everything that only ever plays one line behaves as
// if the moves were a plain list. The first child of a node continues its
// line, which makes the chain of first children the main line: the game as
// it was really played.
//
// Side lines are only created where they were asked for: without
// allowVariations a move always extends the current line, as in a game or a
// puzzle, where a move can only ever be the next one.
class ChessGame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PiecesModel *pieces READ pieces CONSTANT)

    // State of the position being viewed (may be an earlier one).
    Q_PROPERTY(QString fen READ fen NOTIFY positionChanged)
    Q_PROPERTY(int viewPly READ viewPly NOTIFY positionChanged)
    Q_PROPERTY(bool atLatest READ atLatest NOTIFY positionChanged)
    Q_PROPERTY(int lastMoveFrom READ lastMoveFrom NOTIFY positionChanged)
    Q_PROPERTY(int lastMoveTo READ lastMoveTo NOTIFY positionChanged)
    Q_PROPERTY(int checkSquare READ checkSquare NOTIFY positionChanged)
    // Whose move it is in the position being viewed, as opposed to
    // sideToMove, which is whose move it is at the end of the line.
    Q_PROPERTY(QString viewSideToMove READ viewSideToMove NOTIFY positionChanged)
    // Material balance as on Lichess: the pieces each side is up by, compared
    // type by type (so promotions count), and the score from white's point of
    // view with P=1, N=B=3, R=5, Q=9. Pieces are listed in the opponent's
    // colour, like captured pieces: whiteMaterial holds "bQ", "bP", ...
    Q_PROPERTY(QStringList whiteMaterial READ whiteMaterial NOTIFY positionChanged)
    Q_PROPERTY(QStringList blackMaterial READ blackMaterial NOTIFY positionChanged)
    Q_PROPERTY(int materialScore READ materialScore NOTIFY positionChanged)

    // State of the end of the current line.
    Q_PROPERTY(int ply READ ply NOTIFY movesChanged)
    Q_PROPERTY(QString sideToMove READ sideToMove NOTIFY movesChanged)
    Q_PROPERTY(QStringList sanMoves READ sanMoves NOTIFY movesChanged)
    Q_PROPERTY(QString outcome READ outcome NOTIFY movesChanged)

    // Plies before this one are hidden from browsing (e.g. a puzzle's
    // lead-in game).
    Q_PROPERTY(int firstViewablePly READ firstViewablePly WRITE setFirstViewablePly NOTIFY firstViewablePlyChanged)

    // A move played from an earlier position starts a side line instead of
    // extending the game. Off unless a page asks for it.
    Q_PROPERTY(bool allowVariations READ allowVariations WRITE setAllowVariations NOTIFY allowVariationsChanged)
    // The current line leaves the main line somewhere.
    Q_PROPERTY(bool inVariation READ inVariation NOTIFY movesChanged)
    // The ply at which it does, 0 when it does not.
    Q_PROPERTY(int variationStartPly READ variationStartPly NOTIFY movesChanged)

public:
    explicit ChessGame(QObject *parent = nullptr);

    PiecesModel *pieces() const { return m_pieces; }

    QString fen() const;
    int viewPly() const { return m_viewPly; }
    bool atLatest() const { return m_viewPly == ply(); }
    int lastMoveFrom() const;
    int lastMoveTo() const;
    int checkSquare() const;
    QString viewSideToMove() const;
    QStringList whiteMaterial() const { return materialAdvantage(true); }
    QStringList blackMaterial() const { return materialAdvantage(false); }
    int materialScore() const;

    int ply() const { return m_line.size(); }
    QString sideToMove() const;
    QStringList sanMoves() const;
    QStringList uciMoves() const;
    QString outcome() const;
    const ChessPosition &position() const { return m_current; }

    int firstViewablePly() const { return m_firstViewablePly; }
    void setFirstViewablePly(int ply);

    bool allowVariations() const { return m_allowVariations; }
    void setAllowVariations(bool allow);
    bool inVariation() const { return variationStartPly() > 0; }
    int variationStartPly() const;

    Q_INVOKABLE void reset(const QString &fen = QString());
    // Appends moves given in SAN, separated by spaces (move numbers are
    // ignored). Returns false at the first illegal move.
    Q_INVOKABLE bool playSanMoves(const QString &moves);
    Q_INVOKABLE bool playUci(const QString &uci);
    Q_INVOKABLE void undo();

    // Makes the main line equal to |moves| (UCI), reusing the common prefix.
    // Returns false if a move is illegal; the valid prefix is kept then.
    bool setUciMoves(const QStringList &moves);

    // Queries on the position being viewed, used by the board for input.
    Q_INVOKABLE QString pieceAt(int square) const;
    Q_INVOKABLE QVariantList legalTargets(int from) const;
    Q_INVOKABLE bool isPromotion(int from, int to) const;
    Q_INVOKABLE QString uciForMove(int from, int to, const QString &promotion = QString()) const;

    Q_INVOKABLE void viewFirst();
    Q_INVOKABLE void viewPrevious();
    Q_INVOKABLE void viewNext();
    Q_INVOKABLE void viewLatest();
    // Not called viewPly: a property of that name hides a method of the
    // same name from QML, which is how tapping a move came to do nothing.
    Q_INVOKABLE void goToPly(int ply);

    // Side lines. All of these do nothing unless the current line is one.
    // Makes the current line the game: it becomes the main line, and what
    // was the main line becomes the side line.
    Q_INVOKABLE void promoteVariation();
    // Throws the side line away and returns to the main line.
    Q_INVOKABLE void deleteVariation();
    // Returns to the main line, keeping the side line where it is.
    Q_INVOKABLE void exitVariation();

    // --- The move tree, for MoveTreeModel ---

    // Node ids are stable for as long as the node exists. 0 is the start
    // position, which is not a move.
    static int rootNode() { return 0; }
    int nodeCount() const { return m_nodes.size(); }
    bool nodeExists(int node) const;
    QString nodeSan(int node) const;
    QString nodeUci(int node) const;
    int nodeParent(int node) const;
    // The ply the node sits at: 1 for a first move.
    int nodeDepth(int node) const;
    QVector<int> nodeChildren(int node) const;
    // The nodes of the current line, the start position excluded.
    QVector<int> currentLine() const { return m_line; }
    // Moves the view onto |node|, switching lines if it is on another one.
    Q_INVOKABLE void goToNode(int node);
    // The node the view sits on, 0 at the start position.
    int currentNode() const { return m_viewPly == 0 ? 0 : m_line.at(m_viewPly - 1); }

signals:
    void positionChanged();
    void movesChanged();
    void firstViewablePlyChanged();
    void allowVariationsChanged();
    // The shape of the tree changed: a line was added, promoted or dropped.
    void treeChanged();

private:
    struct Node {
        QString uci;
        QString san;
        QString fen;
        std::array<char, 64> pieces;
        int from = -1;
        int to = -1;
        int check = -1;
        int parent = -1;
        QVector<int> children; // children.first() continues this node's line
        bool alive = false;
    };

    // Creates a node for |uci| under |parent|, played in |parent|'s position,
    // or returns -1 if the move is illegal. |asMainLine| puts it first among
    // its siblings, which is what the game's own moves do.
    int addChild(int parent, const QString &uci, bool asMainLine);
    // The child of |node| playing |uci|, or -1.
    int childWithMove(int node, const QString &uci) const;
    int allocNode();
    void freeSubtree(int node);
    // Rebuilds the current line as the path from the root down to |node|,
    // then on along first children.
    void setLineTo(int node);
    // The path from the root to |node|, the root excluded.
    QVector<int> pathTo(int node) const;
    QVector<int> mainLine() const;
    // The position after the moves of the current line.
    void rebuildCurrent();
    ChessPosition positionAt(int ply) const;
    void setView(int ply);
    const std::array<char, 64> &viewedPieces() const;
    QStringList materialAdvantage(bool white) const;
    void clearTree(const ChessPosition &start);

    PiecesModel *m_pieces;
    ChessPosition m_start;
    ChessPosition m_current;   // at the end of the current line
    ChessPosition m_viewed;    // at m_viewPly
    QVector<Node> m_nodes;
    QVector<int> m_free;       // node slots that can be reused
    QVector<int> m_line;       // the current line; m_line[i] is ply i + 1
    int m_viewPly = 0;
    int m_firstViewablePly = 0;
    bool m_allowVariations = false;
};

#endif // CHESSGAME_H
