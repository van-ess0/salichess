// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chessgame.h"

#include "piecesmodel.h"

#include <QRegularExpression>

ChessGame::ChessGame(QObject *parent)
    : QObject(parent)
    , m_pieces(new PiecesModel(this))
{
    m_startSnapshot = snapshotOf(m_start);
    m_pieces->setPosition(m_startSnapshot.pieces);
}

QString ChessGame::fen() const
{
    return m_viewPly == 0 ? m_startSnapshot.fen : m_moves.at(m_viewPly - 1).fen;
}

int ChessGame::lastMoveFrom() const
{
    return m_viewPly == 0 ? -1 : m_moves.at(m_viewPly - 1).from;
}

int ChessGame::lastMoveTo() const
{
    return m_viewPly == 0 ? -1 : m_moves.at(m_viewPly - 1).to;
}

int ChessGame::checkSquare() const
{
    return m_viewPly == 0 ? m_startSnapshot.check : m_moves.at(m_viewPly - 1).check;
}

QString ChessGame::sideToMove() const
{
    return m_current.sideToMove() == ChessPosition::White ? QStringLiteral("white")
                                                          : QStringLiteral("black");
}

QStringList ChessGame::sanMoves() const
{
    QStringList result;
    result.reserve(m_moves.size());
    for (const Snapshot &move : m_moves)
        result.append(move.san);
    return result;
}

QStringList ChessGame::uciMoves() const
{
    QStringList result;
    result.reserve(m_moves.size());
    for (const Snapshot &move : m_moves)
        result.append(move.uci);
    return result;
}

QString ChessGame::outcome() const
{
    switch (m_current.outcome()) {
    case ChessPosition::Checkmate: return QStringLiteral("checkmate");
    case ChessPosition::Stalemate: return QStringLiteral("stalemate");
    case ChessPosition::InsufficientMaterial:
    case ChessPosition::FiftyMoveRule:
    case ChessPosition::ThreefoldRepetition: return QStringLiteral("draw");
    case ChessPosition::Ongoing: break;
    }
    return QString();
}

void ChessGame::setFirstViewablePly(int ply)
{
    ply = qBound(0, ply, this->ply());
    if (m_firstViewablePly == ply)
        return;
    m_firstViewablePly = ply;
    emit firstViewablePlyChanged();
    if (m_viewPly < ply)
        setView(ply);
}

void ChessGame::reset(const QString &fen)
{
    m_start = ChessPosition();
    if (!m_start.setFen(fen))
        m_start = ChessPosition();
    m_current = m_start;
    m_startSnapshot = snapshotOf(m_start);
    m_moves.clear();
    m_firstViewablePly = 0;
    m_viewPly = 0;
    m_pieces->setPosition(m_startSnapshot.pieces);
    emit firstViewablePlyChanged();
    emit movesChanged();
    emit positionChanged();
}

bool ChessGame::playSanMoves(const QString &moves)
{
    static const QRegularExpression separator(QStringLiteral("\\s+"));
    static const QRegularExpression moveNumber(QStringLiteral("^\\d+\\.+"));
    const QStringList tokens = moves.split(separator, QString::SkipEmptyParts);

    bool ok = true;
    const bool follow = atLatest();
    for (QString token : tokens) {
        token.remove(moveNumber); // "12." and "12.e4" style tokens
        if (token.isEmpty())
            continue;
        const QString uci = m_current.uciForSan(token);
        if (uci.isEmpty() || !appendMove(uci)) {
            ok = false;
            break;
        }
    }
    emit movesChanged();
    setView(follow ? ply() : m_viewPly);
    return ok;
}

bool ChessGame::playUci(const QString &uci)
{
    const bool follow = atLatest();
    if (!appendMove(uci))
        return false;
    emit movesChanged();
    if (follow)
        setView(ply());
    return true;
}

void ChessGame::undo()
{
    if (m_moves.isEmpty())
        return;
    QStringList moves = uciMoves();
    moves.removeLast();
    setUciMoves(moves);
}

bool ChessGame::setUciMoves(const QStringList &moves)
{
    int common = 0;
    while (common < m_moves.size() && common < moves.size()
           && m_moves.at(common).uci == moves.at(common))
        ++common;

    if (common == m_moves.size() && common == moves.size())
        return true;

    const bool follow = atLatest();
    if (common < m_moves.size()) {
        m_moves.resize(common);
        m_current = m_start;
        for (const Snapshot &move : m_moves)
            m_current.playUci(move.uci);
    }

    bool ok = true;
    for (int i = common; i < moves.size(); ++i) {
        if (!appendMove(moves.at(i))) {
            ok = false;
            break;
        }
    }

    if (m_firstViewablePly > ply()) {
        m_firstViewablePly = ply();
        emit firstViewablePlyChanged();
    }
    emit movesChanged();
    setView(follow ? ply() : qMin(m_viewPly, ply()));
    return ok;
}

QString ChessGame::pieceAt(int square) const
{
    const char code = m_current.pieceAt(square);
    if (code == '.')
        return QString();
    const bool white = code >= 'A' && code <= 'Z';
    return QString(QChar(white ? 'w' : 'b')) + QChar(code).toUpper();
}

QVariantList ChessGame::legalTargets(int from) const
{
    QVariantList result;
    for (int square : m_current.legalTargets(from))
        result.append(square);
    return result;
}

bool ChessGame::isPromotion(int from, int to) const
{
    return m_current.isPromotion(from, to);
}

QString ChessGame::uciForMove(int from, int to, const QString &promotion) const
{
    QString uci = ChessPosition::squareName(from) + ChessPosition::squareName(to);
    if (!promotion.isEmpty())
        uci += promotion.left(1).toLower();
    return m_current.normalizeUci(uci);
}

void ChessGame::viewFirst()
{
    setView(m_firstViewablePly);
}

void ChessGame::viewPrevious()
{
    setView(m_viewPly - 1);
}

void ChessGame::viewNext()
{
    setView(m_viewPly + 1);
}

void ChessGame::viewLatest()
{
    setView(ply());
}

void ChessGame::viewPly(int ply)
{
    setView(ply);
}

bool ChessGame::appendMove(const QString &uci)
{
    const QString normalized = m_current.normalizeUci(uci);
    if (normalized.isEmpty())
        return false;
    const QString san = m_current.sanForUci(normalized);
    m_current.playUci(normalized);

    Snapshot snapshot = snapshotOf(m_current);
    snapshot.uci = normalized;
    snapshot.san = san;
    snapshot.from = ChessPosition::squareFromName(normalized.mid(0, 2));
    snapshot.to = ChessPosition::squareFromName(normalized.mid(2, 2));
    m_moves.append(snapshot);
    return true;
}

ChessGame::Snapshot ChessGame::snapshotOf(const ChessPosition &position) const
{
    Snapshot snapshot;
    snapshot.fen = position.fen();
    snapshot.pieces = position.pieces();
    snapshot.from = -1;
    snapshot.to = -1;
    snapshot.check = position.inCheck() ? position.kingSquare(position.sideToMove()) : -1;
    return snapshot;
}

void ChessGame::setView(int ply)
{
    // Emits even if the ply number is unchanged: after setUciMoves() the
    // position at the same ply may differ.
    m_viewPly = qBound(qMin(m_firstViewablePly, this->ply()), ply, this->ply());
    m_pieces->setPosition(m_viewPly == 0 ? m_startSnapshot.pieces : m_moves.at(m_viewPly - 1).pieces);
    emit positionChanged();
}
