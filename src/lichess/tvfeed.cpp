// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tvfeed.h"

#include "chess/chessgame.h"
#include "chess/chessposition.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

// The feed sends a keep-alive line now and then; nothing for this long means
// the connection is dead.
const int IdleTimeoutMs = 90 * 1000;
const int MaxBackoffMs = 30 * 1000;

const QStringList Channels = {
    QStringLiteral("best"), QStringLiteral("bullet"), QStringLiteral("blitz"), QStringLiteral("rapid"),
    QStringLiteral("classical"), QStringLiteral("ultraBullet"), QStringLiteral("computer"),
    QStringLiteral("bot"),
};

QVariantMap featuredPlayer(const QJsonObject &player)
{
    const QJsonObject user = player.value(QStringLiteral("user")).toObject();
    QVariantMap result;
    result.insert(QStringLiteral("id"), user.value(QStringLiteral("id")).toString());
    result.insert(QStringLiteral("name"), user.value(QStringLiteral("name")).toString());
    result.insert(QStringLiteral("title"), user.value(QStringLiteral("title")).toString());
    result.insert(QStringLiteral("rating"), player.value(QStringLiteral("rating")).toInt());
    result.insert(QStringLiteral("provisional"), false);
    return result;
}

// The piece placement and side to move of a FEN: all the feed gives.
QString boardAndColor(const QString &fen)
{
    return fen.section(QLatin1Char(' '), 0, 1);
}

} // namespace

TvFeed::TvFeed(QObject *parent)
    : QObject(parent)
    , m_game(new ChessGame(this))
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &TvFeed::connectFeed);
    m_clockTimer.setInterval(200);
    connect(&m_clockTimer, &QTimer::timeout, this, &TvFeed::clockChanged);
}

TvFeed::~TvFeed()
{
    closeFeed();
}

void TvFeed::setChannel(const QString &channel)
{
    const QString wanted = Channels.contains(channel) ? channel : QStringLiteral("best");
    if (m_channel == wanted)
        return;
    m_channel = wanted;
    emit channelChanged();
    if (m_active) {
        closeFeed();
        m_failures = 0;
        connectFeed();
    }
}

void TvFeed::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (m_active) {
        m_failures = 0;
        connectFeed();
    } else {
        closeFeed();
        m_clockTimer.stop();
    }
    emit activeChanged();
}

int TvFeed::whiteTime() const
{
    if (runningClock() == QLatin1String("white"))
        return int(qMax<qint64>(0, m_whiteTime - m_clockStamp.elapsed()));
    return int(m_whiteTime);
}

int TvFeed::blackTime() const
{
    if (runningClock() == QLatin1String("black"))
        return int(qMax<qint64>(0, m_blackTime - m_clockStamp.elapsed()));
    return int(m_blackTime);
}

QString TvFeed::runningClock() const
{
    if (m_gameId.isEmpty() || !m_clockStamp.isValid())
        return QString();
    return m_game->sideToMove();
}

QString TvFeed::completeFen(const QString &boardAndColorFen)
{
    const QStringList parts = boardAndColorFen.split(QLatin1Char(' '));
    if (parts.size() < 2)
        return QString();
    // A right is kept while its king and rook are on their home squares.
    const QVector<QString> ranks = parts.at(0).split(QLatin1Char('/')).toVector();
    if (ranks.size() != 8)
        return QString();
    auto expand = [](const QString &rank) {
        QString out;
        for (QChar c : rank) {
            if (c.isDigit())
                out += QString(c.digitValue(), QLatin1Char('.'));
            else
                out += c;
        }
        return out;
    };
    const QString black = expand(ranks.at(0));
    const QString white = expand(ranks.at(7));
    if (black.size() != 8 || white.size() != 8)
        return QString();
    QString rights;
    if (white.at(4) == QLatin1Char('K')) {
        if (white.at(7) == QLatin1Char('R'))
            rights += QLatin1Char('K');
        if (white.at(0) == QLatin1Char('R'))
            rights += QLatin1Char('Q');
    }
    if (black.at(4) == QLatin1Char('k')) {
        if (black.at(7) == QLatin1Char('r'))
            rights += QLatin1Char('k');
        if (black.at(0) == QLatin1Char('r'))
            rights += QLatin1Char('q');
    }
    if (rights.isEmpty())
        rights = QStringLiteral("-");
    return parts.at(0) + QLatin1Char(' ') + parts.at(1) + QLatin1Char(' ') + rights + QStringLiteral(" - 0 1");
}

void TvFeed::connectFeed()
{
    if (!m_active || m_stream || !Services::api())
        return;
    const QString path = m_channel == QLatin1String("best")
            ? QStringLiteral("/api/tv/feed")
            : QStringLiteral("/api/tv/%1/feed").arg(m_channel);
    m_stream = Services::api()->openStream(path, QUrlQuery(), IdleTimeoutMs);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::opened, this, [this]() {
        m_failures = 0;
        setError(QString());
        setConnected(true);
    });
    connect(m_stream.data(), &NdjsonStream::message, this, &TvFeed::onMessage);
    connect(m_stream.data(), &NdjsonStream::finished, this, &TvFeed::onFinished);
    emit connectedChanged(); // loading
}

void TvFeed::closeFeed()
{
    m_reconnectTimer.stop();
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream.clear();
    }
    setConnected(false);
}

void TvFeed::onMessage(const QJsonObject &message)
{
    const QString type = message.value(QStringLiteral("t")).toString();
    const QJsonObject data = message.value(QStringLiteral("d")).toObject();
    if (type == QLatin1String("featured"))
        applyFeatured(data);
    else if (type == QLatin1String("fen"))
        applyFen(data);
}

void TvFeed::applyFeatured(const QJsonObject &data)
{
    const QString fen = completeFen(data.value(QStringLiteral("fen")).toString());
    m_gameId = data.value(QStringLiteral("id")).toString();
    m_orientation = data.value(QStringLiteral("orientation")).toString(QStringLiteral("white"));
    m_white.clear();
    m_black.clear();
    qint64 whiteSeconds = 0;
    qint64 blackSeconds = 0;
    for (const QJsonValue &value : data.value(QStringLiteral("players")).toArray()) {
        const QJsonObject player = value.toObject();
        const bool white = player.value(QStringLiteral("color")).toString() == QLatin1String("white");
        (white ? m_white : m_black) = featuredPlayer(player);
        (white ? whiteSeconds : blackSeconds) = player.value(QStringLiteral("seconds")).toInt();
    }
    m_game->reset(fen);
    m_whiteTime = whiteSeconds * 1000;
    m_blackTime = blackSeconds * 1000;
    m_clockStamp.restart();
    if (!m_clockTimer.isActive())
        m_clockTimer.start();
    emit gameChanged();
    emit clockChanged();
}

void TvFeed::applyFen(const QJsonObject &data)
{
    const QString fen = data.value(QStringLiteral("fen")).toString();
    const QString lastMove = data.value(QStringLiteral("lm")).toString();

    // Playing the move keeps the pieces' identities, so the board can slide
    // them; if the position does not come out as announced, take the
    // announced one.
    bool played = false;
    if (!lastMove.isEmpty() && m_game->atLatest()) {
        const QString normalized = m_game->position().normalizeUci(lastMove);
        played = !normalized.isEmpty() && m_game->playUci(normalized);
    }
    if (!played || boardAndColor(m_game->position().fen()) != boardAndColor(fen))
        m_game->reset(completeFen(fen));

    m_whiteTime = qint64(data.value(QStringLiteral("wc")).toInt()) * 1000;
    m_blackTime = qint64(data.value(QStringLiteral("bc")).toInt()) * 1000;
    m_clockStamp.restart();
    emit clockChanged();
}

void TvFeed::onFinished(int status, const QString &error)
{
    if (m_stream)
        m_stream->deleteLater();
    m_stream.clear();
    setConnected(false);
    if (!m_active)
        return;
    Q_UNUSED(error)
    ++m_failures;
    const int delay = status == 429 ? 60 * 1000 : qMin(MaxBackoffMs, 1000 << qMin(m_failures, 5));
    if (m_failures > 1)
        setError(status >= 400 && status < 500 && status != 429 ? tr("This channel is not available.")
                                                                 : tr("Connection lost. Reconnecting…"));
    m_reconnectTimer.start(delay);
}

void TvFeed::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged();
}

void TvFeed::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorStringChanged();
    emit connectedChanged(); // loading
}
