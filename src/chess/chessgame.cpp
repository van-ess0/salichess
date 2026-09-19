// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chessgame.h"

#include "piecesmodel.h"

#include <QRegularExpression>

#include <algorithm>

ChessGame::ChessGame(QObject *parent)
    : QObject(parent)
    , m_pieces(new PiecesModel(this))
{
    clearTree(ChessPosition());
    m_pieces->setPosition(m_nodes.at(0).pieces);
}

// --- The tree ---

int ChessGame::allocNode()
{
    if (!m_free.isEmpty()) {
        const int id = m_free.takeLast();
        m_nodes[id] = Node();
        m_nodes[id].alive = true;
        return id;
    }
    m_nodes.append(Node());
    m_nodes.last().alive = true;
    return m_nodes.size() - 1;
}

void ChessGame::freeSubtree(int node)
{
    if (!nodeExists(node) || node == rootNode())
        return;
    const QVector<int> children = m_nodes.at(node).children;
    for (int child : children)
        freeSubtree(child);
    m_nodes[node] = Node();
    m_free.append(node);
}

void ChessGame::clearTree(const ChessPosition &start)
{
    m_nodes.clear();
    m_free.clear();
    m_line.clear();
    m_start = start;
    m_current = start;
    m_viewed = start;
    m_viewPly = 0;

    Node root;
    root.alive = true;
    root.fen = start.fen();
    root.pieces = start.pieces();
    root.check = start.inCheck() ? start.kingSquare(start.sideToMove()) : -1;
    m_nodes.append(root);
}

bool ChessGame::nodeExists(int node) const
{
    return node >= 0 && node < m_nodes.size() && m_nodes.at(node).alive;
}

QString ChessGame::nodeSan(int node) const
{
    return nodeExists(node) ? m_nodes.at(node).san : QString();
}

QString ChessGame::nodeUci(int node) const
{
    return nodeExists(node) ? m_nodes.at(node).uci : QString();
}

int ChessGame::nodeParent(int node) const
{
    return nodeExists(node) ? m_nodes.at(node).parent : -1;
}

int ChessGame::startPly() const
{
    const QStringList fields = m_start.fen().split(QLatin1Char(' '));
    const int moveNumber = qMax(1, fields.value(5).toInt());
    return (moveNumber - 1) * 2 + (fields.value(1) == QLatin1String("b") ? 1 : 0);
}

int ChessGame::nodeDepth(int node) const
{
    int depth = 0;
    for (int at = node; nodeExists(at) && at != rootNode(); at = m_nodes.at(at).parent)
        ++depth;
    return depth;
}

QVector<int> ChessGame::nodeChildren(int node) const
{
    return nodeExists(node) ? m_nodes.at(node).children : QVector<int>();
}

int ChessGame::childWithMove(int node, const QString &uci) const
{
    if (!nodeExists(node))
        return -1;
    for (int child : m_nodes.at(node).children) {
        if (m_nodes.at(child).uci == uci)
            return child;
    }
    return -1;
}

int ChessGame::addChild(int parent, const QString &uci, bool asMainLine)
{
    if (!nodeExists(parent))
        return -1;
    ChessPosition position(m_nodes.at(parent).fen);
    const QString normalized = position.normalizeUci(uci);
    if (normalized.isEmpty())
        return -1;

    const int existing = childWithMove(parent, normalized);
    if (existing >= 0) {
        if (asMainLine) {
            QVector<int> &children = m_nodes[parent].children;
            children.move(children.indexOf(existing), 0);
        }
        return existing;
    }

    const QString san = position.sanForUci(normalized);
    position.playUci(normalized);

    const int id = allocNode();
    Node &node = m_nodes[id];
    node.uci = normalized;
    node.san = san;
    node.fen = position.fen();
    node.pieces = position.pieces();
    node.from = ChessPosition::squareFromName(normalized.mid(0, 2));
    node.to = ChessPosition::squareFromName(normalized.mid(2, 2));
    node.check = position.inCheck() ? position.kingSquare(position.sideToMove()) : -1;
    node.parent = parent;
    if (asMainLine)
        m_nodes[parent].children.prepend(id);
    else
        m_nodes[parent].children.append(id);
    return id;
}

QVector<int> ChessGame::pathTo(int node) const
{
    QVector<int> path;
    for (int at = node; nodeExists(at) && at != rootNode(); at = m_nodes.at(at).parent)
        path.prepend(at);
    return path;
}

QVector<int> ChessGame::mainLine() const
{
    QVector<int> line;
    int at = rootNode();
    while (!m_nodes.at(at).children.isEmpty()) {
        at = m_nodes.at(at).children.first();
        line.append(at);
    }
    return line;
}

void ChessGame::setLineTo(int node)
{
    m_line = pathTo(node);
    // Past the node the line follows the moves that were played on from it.
    int at = nodeExists(node) ? node : rootNode();
    while (!m_nodes.at(at).children.isEmpty()) {
        at = m_nodes.at(at).children.first();
        m_line.append(at);
    }
    rebuildCurrent();
}

void ChessGame::rebuildCurrent()
{
    m_current = m_line.isEmpty() ? m_start : ChessPosition(m_nodes.at(m_line.last()).fen);
    // The tip is replayed from the start so that the position knows the
    // moves before it, which is what repetition draws are counted from.
    if (!m_line.isEmpty()) {
        ChessPosition replayed = m_start;
        bool ok = true;
        for (int node : m_line) {
            if (!replayed.playUci(m_nodes.at(node).uci)) {
                ok = false;
                break;
            }
        }
        if (ok)
            m_current = replayed;
    }
}

int ChessGame::variationStartPly() const
{
    // The first ply where the line does not take its parent's first child.
    for (int i = 0; i < m_line.size(); ++i) {
        const int parent = i == 0 ? rootNode() : m_line.at(i - 1);
        if (m_nodes.at(parent).children.first() != m_line.at(i))
            return i + 1;
    }
    return 0;
}

void ChessGame::setAllowVariations(bool allow)
{
    if (m_allowVariations == allow)
        return;
    m_allowVariations = allow;
    emit allowVariationsChanged();
}

// --- The position on show ---

QString ChessGame::fen() const
{
    return m_viewPly == 0 ? m_nodes.at(0).fen : m_nodes.at(m_line.at(m_viewPly - 1)).fen;
}

int ChessGame::lastMoveFrom() const
{
    return m_viewPly == 0 ? -1 : m_nodes.at(m_line.at(m_viewPly - 1)).from;
}

int ChessGame::lastMoveTo() const
{
    return m_viewPly == 0 ? -1 : m_nodes.at(m_line.at(m_viewPly - 1)).to;
}

int ChessGame::checkSquare() const
{
    return m_viewPly == 0 ? m_nodes.at(0).check : m_nodes.at(m_line.at(m_viewPly - 1)).check;
}

QString ChessGame::viewSideToMove() const
{
    return m_viewed.sideToMove() == ChessPosition::White ? QStringLiteral("white")
                                                         : QStringLiteral("black");
}

namespace {

// Piece types in display order, with their usual values.
const char MaterialTypes[] = { 'Q', 'R', 'B', 'N', 'P' };
const int MaterialValues[] = { 9, 5, 3, 3, 1 };

int countPieces(const std::array<char, 64> &pieces, char code)
{
    return int(std::count(pieces.begin(), pieces.end(), code));
}

} // namespace

QStringList ChessGame::materialAdvantage(bool white) const
{
    const std::array<char, 64> &pieces = viewedPieces();
    QStringList result;
    for (char type : MaterialTypes) {
        const char own = white ? type : char(type + ('a' - 'A'));
        const char other = white ? char(type + ('a' - 'A')) : type;
        const int extra = countPieces(pieces, own) - countPieces(pieces, other);
        for (int i = 0; i < extra; ++i)
            result.append(QString(QChar(white ? 'b' : 'w')) + QChar(type));
    }
    return result;
}

int ChessGame::materialScore() const
{
    const std::array<char, 64> &pieces = viewedPieces();
    int score = 0;
    for (int i = 0; i < 5; ++i) {
        const char type = MaterialTypes[i];
        score += MaterialValues[i] * (countPieces(pieces, type) - countPieces(pieces, char(type + ('a' - 'A'))));
    }
    return score;
}

const std::array<char, 64> &ChessGame::viewedPieces() const
{
    return m_viewPly == 0 ? m_nodes.at(0).pieces : m_nodes.at(m_line.at(m_viewPly - 1)).pieces;
}

QString ChessGame::sideToMove() const
{
    return m_current.sideToMove() == ChessPosition::White ? QStringLiteral("white")
                                                          : QStringLiteral("black");
}

QStringList ChessGame::sanMoves() const
{
    QStringList result;
    result.reserve(m_line.size());
    for (int node : m_line)
        result.append(m_nodes.at(node).san);
    return result;
}

QStringList ChessGame::uciMoves() const
{
    QStringList result;
    result.reserve(m_line.size());
    for (int node : m_line)
        result.append(m_nodes.at(node).uci);
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

// --- Playing moves ---

void ChessGame::reset(const QString &fen)
{
    ChessPosition start;
    if (!start.setFen(fen))
        start = ChessPosition();
    clearTree(start);
    m_firstViewablePly = 0;
    m_pieces->setPosition(m_nodes.at(0).pieces);
    emit firstViewablePlyChanged();
    emit movesChanged();
    emit treeChanged();
    emit positionChanged();
}

bool ChessGame::playSanMoves(const QString &moves)
{
    static const QRegularExpression separator(QStringLiteral("\\s+"));
    static const QRegularExpression moveNumber(QStringLiteral("^\\d+\\.+"));
    const QStringList tokens = moves.split(separator, QString::SkipEmptyParts);

    bool ok = true;
    const bool follow = atLatest();
    int at = m_line.isEmpty() ? rootNode() : m_line.last();
    for (QString token : tokens) {
        token.remove(moveNumber); // "12." and "12.e4" style tokens
        if (token.isEmpty())
            continue;
        const ChessPosition position(m_nodes.at(at).fen);
        const QString uci = position.uciForSan(token);
        const int next = uci.isEmpty() ? -1 : addChild(at, uci, true);
        if (next < 0) {
            ok = false;
            break;
        }
        at = next;
    }
    setLineTo(at);
    emit movesChanged();
    emit treeChanged();
    setView(follow ? ply() : m_viewPly);
    return ok;
}

bool ChessGame::playUci(const QString &uci)
{
    // Without side lines a move is always the next one of the game, wherever
    // the user happens to be looking.
    const bool branching = m_allowVariations && m_viewPly < m_line.size();
    const int parent = branching ? currentNode()
                                 : (m_line.isEmpty() ? rootNode() : m_line.last());
    const bool follow = branching || atLatest();

    const int node = addChild(parent, uci, !branching);
    if (node < 0)
        return false;

    setLineTo(node);
    emit movesChanged();
    emit treeChanged();
    if (follow)
        setView(nodeDepth(node));
    else
        setView(m_viewPly);
    return true;
}

void ChessGame::undo()
{
    if (m_line.isEmpty())
        return;
    const int last = m_line.last();
    const int parent = m_nodes.at(last).parent;
    const bool follow = atLatest();

    m_nodes[parent].children.removeAll(last);
    freeSubtree(last);
    setLineTo(parent);

    if (m_firstViewablePly > ply()) {
        m_firstViewablePly = ply();
        emit firstViewablePlyChanged();
    }
    emit movesChanged();
    emit treeChanged();
    setView(follow ? ply() : qMin(m_viewPly, ply()));
}

bool ChessGame::setUciMoves(const QStringList &moves)
{
    const QVector<int> before = mainLine();
    int common = 0;
    while (common < before.size() && common < moves.size()
           && m_nodes.at(before.at(common)).uci == moves.at(common))
        ++common;

    if (common == before.size() && common == moves.size())
        return true;

    const bool follow = atLatest();
    int at = common == 0 ? rootNode() : before.at(common - 1);

    // Anything the game no longer has is gone, side lines included: this is
    // the game itself being rewritten, by a takeback or a reconnect.
    const QVector<int> stale = m_nodes.at(at).children;
    for (int child : stale)
        freeSubtree(child);
    m_nodes[at].children.clear();

    bool ok = true;
    for (int i = common; i < moves.size(); ++i) {
        const int next = addChild(at, moves.at(i), true);
        if (next < 0) {
            ok = false;
            break;
        }
        at = next;
    }
    setLineTo(at);

    if (m_firstViewablePly > ply()) {
        m_firstViewablePly = ply();
        emit firstViewablePlyChanged();
    }
    emit movesChanged();
    emit treeChanged();
    setView(follow ? ply() : qMin(m_viewPly, ply()));
    return ok;
}

// --- Board input, on the position being viewed ---

QString ChessGame::pieceAt(int square) const
{
    const char code = m_viewed.pieceAt(square);
    if (code == '.')
        return QString();
    const bool white = code >= 'A' && code <= 'Z';
    return QString(QChar(white ? 'w' : 'b')) + QChar(code).toUpper();
}

QVariantList ChessGame::legalTargets(int from) const
{
    QVariantList result;
    for (int square : m_viewed.legalTargets(from))
        result.append(square);
    return result;
}

bool ChessGame::isPromotion(int from, int to) const
{
    return m_viewed.isPromotion(from, to);
}

QString ChessGame::uciForMove(int from, int to, const QString &promotion) const
{
    QString uci = ChessPosition::squareName(from) + ChessPosition::squareName(to);
    if (!promotion.isEmpty())
        uci += promotion.left(1).toLower();
    return m_viewed.normalizeUci(uci);
}

// --- Browsing ---

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

void ChessGame::goToPly(int ply)
{
    setView(ply);
}

void ChessGame::goToNode(int node)
{
    if (node == rootNode()) {
        setView(0);
        return;
    }
    if (!nodeExists(node))
        return;
    if (!m_line.contains(node)) {
        setLineTo(node);
        emit movesChanged();
    }
    setView(nodeDepth(node));
}

ChessPosition ChessGame::positionAt(int ply) const
{
    if (ply <= 0)
        return m_start;
    return ChessPosition(m_nodes.at(m_line.at(ply - 1)).fen);
}

void ChessGame::setView(int ply)
{
    // Emits even if the ply number is unchanged: after the line changed the
    // position at the same ply may differ.
    m_viewPly = qBound(qMin(m_firstViewablePly, this->ply()), ply, this->ply());
    m_viewed = m_viewPly == this->ply() ? m_current : positionAt(m_viewPly);
    m_pieces->setPosition(viewedPieces());
    emit positionChanged();
}

// --- Side lines ---

void ChessGame::promoteVariation()
{
    if (!inVariation())
        return;
    for (int i = 0; i < m_line.size(); ++i) {
        const int parent = i == 0 ? rootNode() : m_line.at(i - 1);
        QVector<int> &children = m_nodes[parent].children;
        const int index = children.indexOf(m_line.at(i));
        if (index > 0)
            children.move(index, 0);
    }
    emit movesChanged();
    emit treeChanged();
}

void ChessGame::deleteVariation()
{
    const int start = variationStartPly();
    if (start == 0)
        return;
    const int branch = m_line.at(start - 1);
    const int parent = m_nodes.at(branch).parent;

    m_nodes[parent].children.removeAll(branch);
    freeSubtree(branch);
    setLineTo(parent);

    if (m_firstViewablePly > ply()) {
        m_firstViewablePly = ply();
        emit firstViewablePlyChanged();
    }
    emit movesChanged();
    emit treeChanged();
    // The position the line branched off from is where the user was before
    // the line existed, and the only ply that still means the same thing.
    setView(qMin(start - 1, ply()));
}

void ChessGame::exitVariation()
{
    const int start = variationStartPly();
    if (start == 0)
        return;
    const int parent = m_nodes.at(m_line.at(start - 1)).parent;
    setLineTo(parent);
    emit movesChanged();
    setView(qMin(start - 1, ply()));
}
