// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chessposition.h"

#include <chess.hpp>

#include <QtGlobal>

#include <algorithm>

namespace {

chess::Move findLegalMove(const chess::Board &board, const QString &uci)
{
    if (uci.size() < 4 || uci.size() > 5)
        return chess::Move(chess::Move::NO_MOVE);

    const std::string wanted = uci.toLower().toStdString();
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    for (const chess::Move &move : moves) {
        // Standard notation ("e1g1") and king-takes-rook notation ("e1h1") are
        // both accepted, since Lichess may send either.
        if (chess::uci::moveToUci(move, false) == wanted
                || chess::uci::moveToUci(move, true) == wanted)
            return move;
    }
    return chess::Move(chess::Move::NO_MOVE);
}

int castlingKingTarget(const chess::Move &move)
{
    const int from = move.from().index();
    const int to = move.to().index();
    const int rank = from / 8;
    return rank * 8 + (to > from ? 6 : 2);
}

// As in Lichess: one king per side and no pawns on the first or last rank.
// The rules engine relies on both kings being there.
bool isPlayablePlacement(const QString &placement)
{
    if (placement.count(QLatin1Char('K')) != 1 || placement.count(QLatin1Char('k')) != 1)
        return false;
    const QStringList ranks = placement.split(QLatin1Char('/'));
    if (ranks.size() != 8)
        return false;
    const auto hasPawn = [](const QString &rank) {
        return rank.contains(QLatin1Char('P')) || rank.contains(QLatin1Char('p'));
    };
    return !hasPawn(ranks.first()) && !hasPawn(ranks.last());
}

} // namespace

ChessPosition::ChessPosition()
    : m_board(new chess::Board())
{
}

ChessPosition::ChessPosition(const QString &fen)
    : m_board(new chess::Board())
{
    setFen(fen);
}

ChessPosition::ChessPosition(const ChessPosition &other)
    : m_board(new chess::Board(*other.m_board))
{
}

ChessPosition &ChessPosition::operator=(const ChessPosition &other)
{
    if (this != &other)
        *m_board = *other.m_board;
    return *this;
}

ChessPosition::~ChessPosition() = default;

QString ChessPosition::startFen()
{
    return QString::fromLatin1(chess::constants::STARTPOS);
}

bool ChessPosition::setFen(const QString &fen)
{
    const QString trimmed = fen.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("startpos"))
        return m_board->setFen(chess::constants::STARTPOS);
    // Checked before parsing: the parser itself looks up both kings.
    if (!isPlayablePlacement(trimmed.section(QLatin1Char(' '), 0, 0)))
        return false;
    chess::Board board;
    if (!board.setFen(trimmed.toStdString()))
        return false;
    // The side that just moved can't be in check, or its king could be taken.
    if (board.isAttacked(board.kingSq(~board.sideToMove()), board.sideToMove()))
        return false;
    *m_board = board;
    return true;
}

QString ChessPosition::fen() const
{
    return QString::fromStdString(m_board->getFen());
}

ChessPosition::Color ChessPosition::sideToMove() const
{
    return m_board->sideToMove() == chess::Color::WHITE ? White : Black;
}

char ChessPosition::pieceAt(int square) const
{
    if (square < 0 || square > 63)
        return '.';
    return static_cast<std::string>(m_board->at(chess::Square(square)))[0];
}

std::array<char, 64> ChessPosition::pieces() const
{
    std::array<char, 64> result;
    for (int sq = 0; sq < 64; ++sq)
        result[sq] = pieceAt(sq);
    return result;
}

bool ChessPosition::inCheck() const
{
    return m_board->inCheck();
}

int ChessPosition::kingSquare(Color color) const
{
    return m_board->kingSq(color == White ? chess::Color::WHITE : chess::Color::BLACK).index();
}

ChessPosition::Outcome ChessPosition::outcome() const
{
    using chess::GameResultReason;
    switch (m_board->isGameOver().first) {
    case GameResultReason::CHECKMATE: return Checkmate;
    case GameResultReason::STALEMATE: return Stalemate;
    case GameResultReason::INSUFFICIENT_MATERIAL: return InsufficientMaterial;
    case GameResultReason::FIFTY_MOVE_RULE: return FiftyMoveRule;
    case GameResultReason::THREEFOLD_REPETITION: return ThreefoldRepetition;
    default: return Ongoing;
    }
}

bool ChessPosition::hasStandardMaterial(const std::array<char, 64> &board, Color color)
{
    const bool white = color == White;
    const auto count = [&](char type) {
        const char code = white ? type : char(type - 'A' + 'a');
        return int(std::count(board.begin(), board.end(), code));
    };
    const int pawns = count('P');
    const int promoted = qMax(0, count('Q') - 1) + qMax(0, count('R') - 2)
            + qMax(0, count('B') - 2) + qMax(0, count('N') - 2);
    return pawns <= 8 && promoted <= 8 - pawns;
}

bool ChessPosition::hasStandardMaterial() const
{
    const std::array<char, 64> board = pieces();
    return hasStandardMaterial(board, White) && hasStandardMaterial(board, Black);
}

QString ChessPosition::normalizeUci(const QString &uci) const
{
    const chess::Move move = findLegalMove(*m_board, uci);
    if (move == chess::Move::NO_MOVE)
        return QString();
    return QString::fromStdString(chess::uci::moveToUci(move, false));
}

QString ChessPosition::sanForUci(const QString &uci) const
{
    const chess::Move move = findLegalMove(*m_board, uci);
    if (move == chess::Move::NO_MOVE)
        return QString();
    return QString::fromStdString(chess::uci::moveToSan(*m_board, move));
}

QString ChessPosition::uciForSan(const QString &san) const
{
    try {
        const chess::Move move = chess::uci::parseSan(*m_board, san.trimmed().toStdString());
        if (move == chess::Move::NO_MOVE)
            return QString();
        return QString::fromStdString(chess::uci::moveToUci(move, false));
    } catch (const std::exception &) {
        return QString();
    }
}

QStringList ChessPosition::legalMovesUci() const
{
    QStringList result;
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, *m_board);
    for (const chess::Move &move : moves)
        result.append(QString::fromStdString(chess::uci::moveToUci(move, false)));
    return result;
}

QVector<int> ChessPosition::legalTargets(int from) const
{
    QVector<int> result;
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, *m_board);
    for (const chess::Move &move : moves) {
        if (move.from().index() != from)
            continue;
        const int to = move.typeOf() == chess::Move::CASTLING ? castlingKingTarget(move)
                                                              : move.to().index();
        if (!result.contains(to))
            result.append(to);
    }
    return result;
}

bool ChessPosition::isPromotion(int from, int to) const
{
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, *m_board);
    for (const chess::Move &move : moves) {
        if (move.from().index() == from && move.to().index() == to
                && move.typeOf() == chess::Move::PROMOTION)
            return true;
    }
    return false;
}

bool ChessPosition::playUci(const QString &uci)
{
    const chess::Move move = findLegalMove(*m_board, uci);
    if (move == chess::Move::NO_MOVE)
        return false;
    m_board->makeMove(move);
    return true;
}

int ChessPosition::squareFromName(const QString &name)
{
    if (name.size() != 2)
        return -1;
    const int file = name.at(0).toLatin1() - 'a';
    const int rank = name.at(1).toLatin1() - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;
    return rank * 8 + file;
}

QString ChessPosition::squareName(int square)
{
    if (square < 0 || square > 63)
        return QString();
    return QString(QChar('a' + square % 8)) + QChar('1' + square / 8);
}
