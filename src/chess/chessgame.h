// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHESSGAME_H
#define CHESSGAME_H

#include "chessposition.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>

class PiecesModel;

// A game as a start position plus a list of moves, with the ability to browse
// earlier positions. This is what the QML ChessBoard displays; controllers
// (online games, puzzles, later analysis) own one and feed moves into it.
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
    // Material balance as on Lichess: the pieces each side is up by, compared
    // type by type (so promotions count), and the score from white's point of
    // view with P=1, N=B=3, R=5, Q=9. Pieces are listed in the opponent's
    // colour, like captured pieces: whiteMaterial holds "bQ", "bP", ...
    Q_PROPERTY(QStringList whiteMaterial READ whiteMaterial NOTIFY positionChanged)
    Q_PROPERTY(QStringList blackMaterial READ blackMaterial NOTIFY positionChanged)
    Q_PROPERTY(int materialScore READ materialScore NOTIFY positionChanged)

    // State of the latest position.
    Q_PROPERTY(int ply READ ply NOTIFY movesChanged)
    Q_PROPERTY(QString sideToMove READ sideToMove NOTIFY movesChanged)
    Q_PROPERTY(QStringList sanMoves READ sanMoves NOTIFY movesChanged)
    Q_PROPERTY(QString outcome READ outcome NOTIFY movesChanged)

    // Plies before this one are hidden from browsing (e.g. a puzzle's
    // lead-in game).
    Q_PROPERTY(int firstViewablePly READ firstViewablePly WRITE setFirstViewablePly NOTIFY firstViewablePlyChanged)

public:
    explicit ChessGame(QObject *parent = nullptr);

    PiecesModel *pieces() const { return m_pieces; }

    QString fen() const;
    int viewPly() const { return m_viewPly; }
    bool atLatest() const { return m_viewPly == ply(); }
    int lastMoveFrom() const;
    int lastMoveTo() const;
    int checkSquare() const;
    QStringList whiteMaterial() const { return materialAdvantage(true); }
    QStringList blackMaterial() const { return materialAdvantage(false); }
    int materialScore() const;

    int ply() const { return m_moves.size(); }
    QString sideToMove() const;
    QStringList sanMoves() const;
    QStringList uciMoves() const;
    QString outcome() const;
    const ChessPosition &position() const { return m_current; }

    int firstViewablePly() const { return m_firstViewablePly; }
    void setFirstViewablePly(int ply);

    Q_INVOKABLE void reset(const QString &fen = QString());
    // Appends moves given in SAN, separated by spaces (move numbers are
    // ignored). Returns false at the first illegal move.
    Q_INVOKABLE bool playSanMoves(const QString &moves);
    Q_INVOKABLE bool playUci(const QString &uci);
    Q_INVOKABLE void undo();

    // Makes the move list equal to |moves| (UCI), reusing the common prefix.
    // Returns false if a move is illegal; the valid prefix is kept then.
    bool setUciMoves(const QStringList &moves);

    // Queries on the latest position, used by the board for input.
    Q_INVOKABLE QString pieceAt(int square) const;
    Q_INVOKABLE QVariantList legalTargets(int from) const;
    Q_INVOKABLE bool isPromotion(int from, int to) const;
    Q_INVOKABLE QString uciForMove(int from, int to, const QString &promotion = QString()) const;

    Q_INVOKABLE void viewFirst();
    Q_INVOKABLE void viewPrevious();
    Q_INVOKABLE void viewNext();
    Q_INVOKABLE void viewLatest();
    Q_INVOKABLE void viewPly(int ply);

signals:
    void positionChanged();
    void movesChanged();
    void firstViewablePlyChanged();

private:
    struct Snapshot {
        QString uci;
        QString san;
        QString fen;
        std::array<char, 64> pieces;
        int from;
        int to;
        int check;
    };

    bool appendMove(const QString &uci);
    Snapshot snapshotOf(const ChessPosition &position) const;
    void setView(int ply);
    const std::array<char, 64> &viewedPieces() const;
    QStringList materialAdvantage(bool white) const;

    PiecesModel *m_pieces;
    ChessPosition m_start;
    ChessPosition m_current;
    Snapshot m_startSnapshot;
    QVector<Snapshot> m_moves;
    int m_viewPly = 0;
    int m_firstViewablePly = 0;
};

#endif // CHESSGAME_H
