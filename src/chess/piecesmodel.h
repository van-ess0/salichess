// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PIECESMODEL_H
#define PIECESMODEL_H

#include <QAbstractListModel>
#include <QVector>

#include <array>

// One row per piece on the board. Rows keep their identity across position
// changes (a moved piece only changes its square), so QML delegates can
// animate piece movement.
class PiecesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PieceRole = Qt::UserRole + 1, // "wK", "bp", ...
        SquareRole                    // 0..63, a1 = 0
    };

    explicit PiecesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Pieces encoded as FEN characters, '.' for empty.
    void setPosition(const std::array<char, 64> &board);

private:
    struct Piece {
        char code;
        int square;
    };
    QVector<Piece> m_pieces;
};

#endif // PIECESMODEL_H
