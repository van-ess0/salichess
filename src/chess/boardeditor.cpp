// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "boardeditor.h"

#include "chessposition.h"
#include "piecesmodel.h"

namespace {

const char Empty = '.';

// King and rook squares for each castling right: K, Q, k, q.
struct CastlingSquares {
    char king;
    int kingSquare;
    char rook;
    int rookSquare;
};
const CastlingSquares Castling[4] = {
    { 'K', 4, 'R', 7 },
    { 'K', 4, 'R', 0 },
    { 'k', 60, 'r', 63 },
    { 'k', 60, 'r', 56 },
};
const char CastlingLetters[4] = { 'K', 'Q', 'k', 'q' };

// "wK" -> 'K', "bp" -> 'p', anything else -> Empty.
char codeFromName(const QString &name)
{
    if (name.size() != 2)
        return Empty;
    const QChar color = name.at(0);
    const char type = name.at(1).toUpper().toLatin1();
    if (!QByteArray("PNBRQK").contains(type))
        return Empty;
    if (color == QLatin1Char('w'))
        return type;
    if (color == QLatin1Char('b'))
        return QChar(type).toLower().toLatin1();
    return Empty;
}

int count(const std::array<char, 64> &board, char code)
{
    int n = 0;
    for (char c : board)
        n += c == code;
    return n;
}

} // namespace

BoardEditor::BoardEditor(QObject *parent)
    : QObject(parent)
    , m_model(new PiecesModel(this))
{
    m_board.fill(Empty);
    m_castling.fill(true);
    m_uncertain.fill(false);
    m_last = state();
    setStartPosition();
    clearHistory();
}

void BoardEditor::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(state());
    restore(m_undo.takeLast());
}

void BoardEditor::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(state());
    restore(m_redo.takeLast());
}

void BoardEditor::clearHistory()
{
    if (m_undo.isEmpty() && m_redo.isEmpty())
        return;
    m_undo.clear();
    m_redo.clear();
    emit historyChanged();
}

void BoardEditor::restore(const State &state)
{
    m_board = state.board;
    m_whiteToMove = state.whiteToMove;
    m_castling = state.castling;
    // update() records a change against m_last; this is not a new one.
    m_last = state;
    m_uncertain.fill(false);
    emit uncertainSquaresChanged();
    update();
    emit historyChanged();
}

QString BoardEditor::sideToMove() const
{
    return m_whiteToMove ? QStringLiteral("white") : QStringLiteral("black");
}

void BoardEditor::setSideToMove(const QString &color)
{
    const bool white = color != QLatin1String("black");
    if (white == m_whiteToMove)
        return;
    m_whiteToMove = white;
    update();
}

void BoardEditor::setCastling(int index, bool on)
{
    if (m_castling[index] == on)
        return;
    m_castling[index] = on;
    update();
}

bool BoardEditor::castlingPossible(int index) const
{
    const CastlingSquares &squares = Castling[index];
    return m_board[squares.kingSquare] == squares.king
            && m_board[squares.rookSquare] == squares.rook;
}

QVariantList BoardEditor::uncertainSquares() const
{
    QVariantList squares;
    for (int sq = 0; sq < 64; ++sq) {
        if (m_uncertain[sq])
            squares.append(sq);
    }
    return squares;
}

void BoardEditor::setUncertain(int square, bool uncertain)
{
    if (square < 0 || square > 63 || m_uncertain[square] == uncertain)
        return;
    m_uncertain[square] = uncertain;
    emit uncertainSquaresChanged();
}

int BoardEditor::checkSquare() const
{
    if (!valid())
        return -1;
    const ChessPosition position(fen());
    return position.inCheck()
            ? position.kingSquare(m_whiteToMove ? ChessPosition::White : ChessPosition::Black)
            : -1;
}

QString BoardEditor::pieceAt(int square) const
{
    if (square < 0 || square > 63 || m_board[square] == Empty)
        return QString();
    const char code = m_board[square];
    const bool white = code >= 'A' && code <= 'Z';
    return QString(QChar(white ? 'w' : 'b')) + QChar(code).toUpper();
}

void BoardEditor::setPiece(int square, const QString &piece)
{
    if (square < 0 || square > 63)
        return;
    setUncertain(square, false);
    const char code = codeFromName(piece);
    if (m_board[square] == code)
        return;
    m_board[square] = code;
    update();
}

void BoardEditor::movePiece(int from, int to)
{
    if (from < 0 || from > 63 || to < 0 || to > 63 || from == to || m_board[from] == Empty)
        return;
    setUncertain(from, false);
    setUncertain(to, false);
    m_board[to] = m_board[from];
    m_board[from] = Empty;
    update();
}

void BoardEditor::clear()
{
    m_board.fill(Empty);
    m_uncertain.fill(false);
    emit uncertainSquaresChanged();
    update();
}

void BoardEditor::setStartPosition()
{
    setFen(ChessPosition::startFen());
}

bool BoardEditor::setFen(const QString &fen)
{
    const QStringList fields = fen.trimmed().split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (fields.isEmpty())
        return false;

    const QStringList ranks = fields.at(0).split(QLatin1Char('/'));
    if (ranks.size() != 8)
        return false;
    std::array<char, 64> board;
    board.fill(Empty);
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i; // the FEN starts with the eighth rank
        int file = 0;
        for (const QChar c : ranks.at(i)) {
            if (c.isDigit()) {
                file += c.digitValue();
            } else if (QByteArray("PNBRQKpnbrqk").contains(c.toLatin1()) && file < 8) {
                board[rank * 8 + file] = c.toLatin1();
                ++file;
            } else {
                return false;
            }
            if (file > 8)
                return false;
        }
        if (file != 8)
            return false;
    }

    bool whiteToMove = true;
    if (fields.size() > 1) {
        if (fields.at(1) == QLatin1String("b"))
            whiteToMove = false;
        else if (fields.at(1) != QLatin1String("w"))
            return false;
    }

    // Without a castling field every right is left on, to be granted
    // wherever king and rook still stand at home.
    std::array<bool, 4> castling;
    castling.fill(true);
    if (fields.size() > 2) {
        for (int i = 0; i < 4; ++i)
            castling[i] = fields.at(2).contains(QLatin1Char(CastlingLetters[i]));
    }

    m_board = board;
    m_whiteToMove = whiteToMove;
    m_castling = castling;
    m_uncertain.fill(false);
    emit uncertainSquaresChanged();
    update();
    return true;
}

QString BoardEditor::placement() const
{
    QString result;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const char code = m_board[rank * 8 + file];
            if (code == Empty) {
                ++empty;
                continue;
            }
            if (empty > 0)
                result += QString::number(empty);
            empty = 0;
            result += QLatin1Char(code);
        }
        if (empty > 0)
            result += QString::number(empty);
        if (rank > 0)
            result += QLatin1Char('/');
    }
    return result;
}

QString BoardEditor::castlingField() const
{
    QString result;
    for (int i = 0; i < 4; ++i) {
        if (m_castling[i] && castlingPossible(i))
            result += QLatin1Char(CastlingLetters[i]);
    }
    return result.isEmpty() ? QStringLiteral("-") : result;
}

QString BoardEditor::fen() const
{
    return placement() + (m_whiteToMove ? QStringLiteral(" w ") : QStringLiteral(" b "))
            + castlingField() + QStringLiteral(" - 0 1");
}

void BoardEditor::update()
{
    const State now = state();
    if (!(now == m_last)) {
        m_undo.append(m_last);
        m_redo.clear();
        m_last = now;
        emit historyChanged();
    }

    m_model->setPosition(m_board);

    // The same rules ChessPosition applies, checked one at a time so the
    // user can be told which one is broken.
    m_problem.clear();
    const int whiteKings = count(m_board, 'K');
    const int blackKings = count(m_board, 'k');
    bool pawnOnEdge = false;
    for (int file = 0; file < 8; ++file) {
        for (int sq : { file, 56 + file }) {
            if (m_board[sq] == 'P' || m_board[sq] == 'p')
                pawnOnEdge = true;
        }
    }
    if (whiteKings == 0)
        m_problem = tr("White has no king");
    else if (whiteKings > 1)
        m_problem = tr("White has more than one king");
    else if (blackKings == 0)
        m_problem = tr("Black has no king");
    else if (blackKings > 1)
        m_problem = tr("Black has more than one king");
    else if (pawnOnEdge)
        m_problem = tr("Pawns cannot stand on the first or last rank");
    else if (!ChessPosition::hasStandardMaterial(m_board, ChessPosition::White))
        m_problem = tr("White has more pieces than a game can give it: at most eight pawns, and an extra queen, rook, bishop or knight only for each pawn gone");
    else if (!ChessPosition::hasStandardMaterial(m_board, ChessPosition::Black))
        m_problem = tr("Black has more pieces than a game can give it: at most eight pawns, and an extra queen, rook, bishop or knight only for each pawn gone");
    else if (!ChessPosition().setFen(fen()))
        // With the kings and pawns in order this is the one rule left.
        m_problem = m_whiteToMove ? tr("Black is in check, but it is White's move")
                                  : tr("White is in check, but it is Black's move");

    emit positionChanged();
}
