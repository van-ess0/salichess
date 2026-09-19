// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BOARDEDITOR_H
#define BOARDEDITOR_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

#include <array>

class PiecesModel;

// A position being set up by hand, e.g. copied from a board on the table.
// Created in QML:
//     BoardEditor { id: editor }
//     ChessBoard { game: editor }
//
// Unlike ChessGame it holds any arrangement of pieces, including ones that
// cannot be played (no kings, pawns on the last rank), because that is what
// a board looks like halfway through setting it up. |valid| says whether the
// position can be played, and |problem| why not.
//
// It offers what ChessBoard reads off a game (pieces, pieceAt(), the last
// move and check squares, positionChanged()), so the same board shows it.
class BoardEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PiecesModel *pieces READ pieces CONSTANT)
    Q_PROPERTY(QString fen READ fen NOTIFY positionChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY positionChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY positionChanged)
    // "white" or "black".
    Q_PROPERTY(QString sideToMove READ sideToMove WRITE setSideToMove NOTIFY positionChanged)
    // Castling rights as the user wants them. A right only reaches the FEN
    // while its king and rook stand on their starting squares, which the
    // *Possible properties say.
    Q_PROPERTY(bool whiteKingside READ whiteKingside WRITE setWhiteKingside NOTIFY positionChanged)
    Q_PROPERTY(bool whiteQueenside READ whiteQueenside WRITE setWhiteQueenside NOTIFY positionChanged)
    Q_PROPERTY(bool blackKingside READ blackKingside WRITE setBlackKingside NOTIFY positionChanged)
    Q_PROPERTY(bool blackQueenside READ blackQueenside WRITE setBlackQueenside NOTIFY positionChanged)
    Q_PROPERTY(bool whiteKingsidePossible READ whiteKingsidePossible NOTIFY positionChanged)
    Q_PROPERTY(bool whiteQueensidePossible READ whiteQueensidePossible NOTIFY positionChanged)
    Q_PROPERTY(bool blackKingsidePossible READ blackKingsidePossible NOTIFY positionChanged)
    Q_PROPERTY(bool blackQueensidePossible READ blackQueensidePossible NOTIFY positionChanged)
    // Squares whose piece is only a guess, e.g. from reading a photo. Setting
    // a square by hand takes it off the list.
    Q_PROPERTY(QVariantList uncertainSquares READ uncertainSquares NOTIFY uncertainSquaresChanged)
    // Every change to the position can be undone, and what was undone can
    // be redone until the next change is made.
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

    // For ChessBoard: a set-up position has no last move.
    Q_PROPERTY(int lastMoveFrom READ noSquare CONSTANT)
    Q_PROPERTY(int lastMoveTo READ noSquare CONSTANT)
    Q_PROPERTY(int checkSquare READ checkSquare NOTIFY positionChanged)

public:
    explicit BoardEditor(QObject *parent = nullptr);

    PiecesModel *pieces() const { return m_model; }
    QString fen() const;
    bool valid() const { return m_problem.isEmpty(); }
    QString problem() const { return m_problem; }

    QString sideToMove() const;
    void setSideToMove(const QString &color);

    bool whiteKingside() const { return m_castling[0]; }
    bool whiteQueenside() const { return m_castling[1]; }
    bool blackKingside() const { return m_castling[2]; }
    bool blackQueenside() const { return m_castling[3]; }
    void setWhiteKingside(bool on) { setCastling(0, on); }
    void setWhiteQueenside(bool on) { setCastling(1, on); }
    void setBlackKingside(bool on) { setCastling(2, on); }
    void setBlackQueenside(bool on) { setCastling(3, on); }
    bool whiteKingsidePossible() const { return castlingPossible(0); }
    bool whiteQueensidePossible() const { return castlingPossible(1); }
    bool blackKingsidePossible() const { return castlingPossible(2); }
    bool blackQueensidePossible() const { return castlingPossible(3); }

    QVariantList uncertainSquares() const;

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    int noSquare() const { return -1; }
    int checkSquare() const;

    // Pieces are named as on the board: "wK", "bp"; "" for an empty square.
    Q_INVOKABLE QString pieceAt(int square) const;
    Q_INVOKABLE void setPiece(int square, const QString &piece);
    Q_INVOKABLE void movePiece(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setStartPosition();
    // Takes the placement, side to move and castling rights of |fen|; the
    // rest (en passant, move counters) is not kept. The placement need not
    // be playable. Returns false, changing nothing, if |fen| cannot be read.
    Q_INVOKABLE bool setFen(const QString &fen);
    // Marks |square| as a guess; see uncertainSquares.
    Q_INVOKABLE void setUncertain(int square, bool uncertain);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    // Forgets the changes made so far, e.g. once the position to start from
    // has been loaded.
    Q_INVOKABLE void clearHistory();

signals:
    void positionChanged();
    void uncertainSquaresChanged();
    void historyChanged();

private:
    // What undo brings back. Which squares were guesses is not part of it.
    struct State {
        std::array<char, 64> board;
        bool whiteToMove;
        std::array<bool, 4> castling;
        bool operator==(const State &other) const
        {
            return board == other.board && whiteToMove == other.whiteToMove
                    && castling == other.castling;
        }
    };
    State state() const { return { m_board, m_whiteToMove, m_castling }; }
    void restore(const State &state);

    void setCastling(int index, bool on);
    bool castlingPossible(int index) const;
    QString placement() const;
    QString castlingField() const;
    void update();

    PiecesModel *m_model;
    std::array<char, 64> m_board;
    bool m_whiteToMove = true;
    // K, Q, k, q.
    std::array<bool, 4> m_castling;
    std::array<bool, 64> m_uncertain;
    QString m_problem;
    // The position as the last change left it, which becomes an undo step
    // when the next change is made.
    State m_last;
    QVector<State> m_undo;
    QVector<State> m_redo;
};

#endif // BOARDEDITOR_H
