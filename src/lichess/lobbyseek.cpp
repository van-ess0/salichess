// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lobbyseek.h"

#include "eventstream.h"
#include "core/lichessapi.h"
#include "core/ndjsonstream.h"
#include "core/services.h"
#include "core/session.h"

namespace {

// Lichess sends a blank line every 10 seconds while the seek waits.
const int IdleTimeoutMs = 30 * 1000;
// The seek request ends both when an opponent is found and when the seek is
// removed; a game starting on the event stream right after tells them apart.
const int GameWaitMs = 3000;

QString str(const QVariantMap &map, const char *key)
{
    return map.value(QString::fromLatin1(key)).toString();
}

} // namespace

LobbySeek::LobbySeek(LichessApi *api, EventStream *events, QObject *parent)
    : QObject(parent)
    , m_api(api)
{
    m_gameWait.setSingleShot(true);
    m_gameWait.setInterval(GameWaitMs);
    connect(&m_gameWait, &QTimer::timeout, this, [this]() {
        if (m_state == Seeking)
            setState(Expired);
    });
    connect(events, &EventStream::gameStarted, this, &LobbySeek::onGameStarted);
}

LobbySeek::~LobbySeek()
{
    closeStream();
}

QString LobbySeek::speedOf(int limitSeconds, int increment)
{
    // Lichess estimates a game's duration as initial time + 40 × increment.
    const int estimate = limitSeconds + 40 * increment;
    if (estimate < 180)
        return QStringLiteral("bullet");
    if (estimate < 480)
        return QStringLiteral("blitz");
    if (estimate < 1500)
        return QStringLiteral("rapid");
    return QStringLiteral("classical");
}

QString LobbySeek::description() const
{
    if (m_timeControl.isEmpty())
        return QString();
    QString speed;
    if (m_speed == QLatin1String("rapid"))
        speed = tr("Rapid");
    else if (m_speed == QLatin1String("classical"))
        speed = tr("Classical");
    else
        speed = tr("Correspondence");
    return QStringLiteral("%1 • %2 • %3").arg(m_timeControl, speed, m_rated ? tr("Rated") : tr("Casual"));
}

void LobbySeek::create(const QVariantMap &options)
{
    // Lichess allows only one real-time seek per user.
    if (m_state == Seeking)
        return;
    m_gameWait.stop();
    m_gameId.clear();
    m_opponent.clear();
    m_error.clear();
    m_correspondence = options.value(QStringLiteral("correspondence")).toBool();
    m_rated = options.value(QStringLiteral("rated")).toBool();

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("rated"), m_rated ? QStringLiteral("true") : QStringLiteral("false"));
    if (m_correspondence) {
        const int days = options.value(QStringLiteral("days"), 3).toInt();
        form.addQueryItem(QStringLiteral("days"), QString::number(days));
        m_speed = QStringLiteral("correspondence");
        m_timeControl = days == 1 ? tr("1 day") : tr("%1 days").arg(days);
    } else {
        const int minutes = options.value(QStringLiteral("minutes"), 10).toInt();
        const int increment = options.value(QStringLiteral("increment"), 0).toInt();
        form.addQueryItem(QStringLiteral("time"), QString::number(minutes));
        form.addQueryItem(QStringLiteral("increment"), QString::number(increment));
        m_speed = speedOf(minutes * 60, increment);
        m_timeControl = QStringLiteral("%1+%2").arg(minutes).arg(increment);
    }
    form.addQueryItem(QStringLiteral("color"), options.value(QStringLiteral("color"), QStringLiteral("random")).toString());
    form.addQueryItem(QStringLiteral("variant"), QStringLiteral("standard"));
    // Around my rating in this speed, if I have one.
    const int delta = options.value(QStringLiteral("ratingDelta")).toInt();
    const int rating = Services::session() ? Services::session()->rating(m_speed) : 0;
    if (delta > 0 && rating > 0)
        form.addQueryItem(QStringLiteral("ratingRange"),
                          QStringLiteral("%1-%2").arg(qMax(0, rating - delta)).arg(rating + delta));

    setState(Idle); // clears what the last seek showed
    if (m_speed == QLatin1String("bullet") || m_speed == QLatin1String("blitz")) {
        m_error = tr("Through the Lichess Board API, only rapid, classical and correspondence games can be played with random opponents.");
        setState(Failed);
        return;
    }
    setState(Seeking);

    if (m_correspondence) {
        m_api->postForm(QStringLiteral("/api/board/seek"), form, this, [this](const ApiResult &result) {
            if (m_state != Seeking)
                return;
            if (!result.ok()) {
                m_error = result.errorString;
                setState(Failed);
                return;
            }
            setState(Posted);
        });
        return;
    }

    // Lichess asks to open the event stream before seeking, so that the game
    // can't be missed; it is open while logged in.
    m_stream = m_api->openPostStream(QStringLiteral("/api/board/seek"), form, IdleTimeoutMs);
    m_stream->setParent(this);
    connect(m_stream.data(), &NdjsonStream::finished, this, [this](int status, const QString &error) {
        if (m_stream)
            m_stream->deleteLater();
        m_stream.clear();
        if (m_state != Seeking)
            return;
        if (status < 200 || status >= 300) {
            m_error = error.isEmpty() ? tr("Could not start the seek.") : error;
            setState(Failed);
        } else if (!error.isEmpty()) {
            m_error = tr("The connection to Lichess was lost.");
            setState(Failed);
        } else {
            m_gameWait.start();
        }
    });
}

void LobbySeek::cancel()
{
    // A correspondence seek can only be withdrawn on lichess.org.
    if (m_state != Seeking || m_correspondence)
        return;
    // Closing the request cancels the seek.
    closeStream();
    m_gameWait.stop();
    setState(Canceled);
}

void LobbySeek::onGameStarted(const QVariantMap &game)
{
    if (m_state != Seeking || m_correspondence)
        return;
    // Nothing links the game to the seek: take the first new game from the
    // lobby or a pool with the seek's speed and rating mode.
    const QString source = str(game, "source");
    const QString id = game.value(QStringLiteral("gameId"), game.value(QStringLiteral("id"))).toString();
    if (id.isEmpty() || (source != QLatin1String("lobby") && source != QLatin1String("pool"))
            || str(game, "speed") != m_speed
            || game.value(QStringLiteral("rated")).toBool() != m_rated
            || game.value(QStringLiteral("hasMoved")).toBool())
        return;
    m_gameId = id;
    m_opponent = str(game.value(QStringLiteral("opponent")).toMap(), "username");
    if (m_opponent.isEmpty())
        m_opponent = tr("Anonymous");
    m_gameWait.stop();
    closeStream();
    setState(Found);
    emit gameFound(m_gameId, m_opponent);
}

void LobbySeek::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void LobbySeek::closeStream()
{
    if (!m_stream)
        return;
    m_stream->abort();
    m_stream->deleteLater();
    m_stream.clear();
}
