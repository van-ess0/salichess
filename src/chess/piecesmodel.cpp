// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "piecesmodel.h"

#include <QtGlobal>

namespace {

QString pieceName(char code)
{
    const bool white = code >= 'A' && code <= 'Z';
    return QString(QChar(white ? 'w' : 'b')) + QChar(code).toUpper();
}

int distance(int a, int b)
{
    return qMax(qAbs(a % 8 - b % 8), qAbs(a / 8 - b / 8));
}

} // namespace

PiecesModel::PiecesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PiecesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_pieces.size();
}

QVariant PiecesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_pieces.size())
        return QVariant();
    const Piece &piece = m_pieces.at(index.row());
    switch (role) {
    case PieceRole: return pieceName(piece.code);
    case SquareRole: return piece.square;
    default: return QVariant();
    }
}

QHash<int, QByteArray> PiecesModel::roleNames() const
{
    return {
        { PieceRole, "piece" },
        { SquareRole, "square" },
    };
}

void PiecesModel::setPosition(const std::array<char, 64> &board)
{
    // Squares of the new position that still need a piece.
    std::array<bool, 64> unmatched;
    for (int sq = 0; sq < 64; ++sq)
        unmatched[sq] = board[sq] != '.';

    // 1. Pieces that stay where they are.
    QVector<int> leftover; // rows that must move or disappear
    for (int row = 0; row < m_pieces.size(); ++row) {
        const Piece &piece = m_pieces.at(row);
        if (unmatched[piece.square] && board[piece.square] == piece.code)
            unmatched[piece.square] = false;
        else
            leftover.append(row);
    }

    // 2. Moved pieces: pair each leftover row with the nearest free square
    // holding the same piece type.
    QVector<int> removed;
    for (int row : leftover) {
        Piece &piece = m_pieces[row];
        int best = -1;
        for (int sq = 0; sq < 64; ++sq) {
            if (unmatched[sq] && board[sq] == piece.code
                    && (best < 0 || distance(sq, piece.square) < distance(best, piece.square)))
                best = sq;
        }
        if (best < 0) {
            removed.append(row);
            continue;
        }
        unmatched[best] = false;
        piece.square = best;
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { SquareRole });
    }

    // 3. Captured pieces (and pawns that promoted) disappear.
    for (int i = removed.size() - 1; i >= 0; --i) {
        const int row = removed.at(i);
        beginRemoveRows(QModelIndex(), row, row);
        m_pieces.remove(row);
        endRemoveRows();
    }

    // 4. New pieces (initial setup, promotions).
    QVector<Piece> added;
    for (int sq = 0; sq < 64; ++sq) {
        if (unmatched[sq])
            added.append(Piece { board[sq], sq });
    }
    if (!added.isEmpty()) {
        beginInsertRows(QModelIndex(), m_pieces.size(), m_pieces.size() + added.size() - 1);
        m_pieces += added;
        endInsertRows();
    }
}
