// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TVFEED_H
#define TVFEED_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

class ChessGame;
class NdjsonStream;

// Lichess TV: the game Lichess features in a channel, followed move by move
// (/api/tv/{channel}/feed). When the featured game changes, the feed moves
// on to the next one by itself. No account is needed. Created in QML:
//     TvFeed { channel: "best"; active: page.visible }
//
// The feed carries positions, not move lists, so the game knows the current
// position and the last move but not how it came about.
class TvFeed : public QObject
{
    Q_OBJECT
    // "best" (top rated), "bullet", "blitz", "rapid", "classical",
    // "ultraBullet", "computer" or "bot".
    Q_PROPERTY(QString channel READ channel WRITE setChannel NOTIFY channelChanged)
    // The feed is only open while this is set, so a page that is out of
    // sight costs nothing.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(ChessGame *game READ game CONSTANT)
    Q_PROPERTY(QString gameId READ gameId NOTIFY gameChanged)
    Q_PROPERTY(QString orientation READ orientation NOTIFY gameChanged)
    Q_PROPERTY(QVariantMap white READ white NOTIFY gameChanged) // id, name, rating, title
    Q_PROPERTY(QVariantMap black READ black NOTIFY gameChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY connectedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(int whiteTime READ whiteTime NOTIFY clockChanged) // ms
    Q_PROPERTY(int blackTime READ blackTime NOTIFY clockChanged)
    Q_PROPERTY(QString runningClock READ runningClock NOTIFY clockChanged)

public:
    explicit TvFeed(QObject *parent = nullptr);
    ~TvFeed() override;

    QString channel() const { return m_channel; }
    void setChannel(const QString &channel);
    bool active() const { return m_active; }
    void setActive(bool active);
    ChessGame *game() const { return m_game; }
    QString gameId() const { return m_gameId; }
    QString orientation() const { return m_orientation; }
    QVariantMap white() const { return m_white; }
    QVariantMap black() const { return m_black; }
    bool connected() const { return m_connected; }
    bool loading() const { return m_active && !m_connected && m_error.isEmpty(); }
    QString errorString() const { return m_error; }
    int whiteTime() const;
    int blackTime() const;
    QString runningClock() const;

    // Completes the "board and side to move" FEN the feed sends with the
    // castling rights its pieces still allow, so that moves can be played on
    // it. Public for the tests.
    static QString completeFen(const QString &boardAndColor);

signals:
    void channelChanged();
    void activeChanged();
    void gameChanged();
    void connectedChanged();
    void errorStringChanged();
    void clockChanged();

private:
    void connectFeed();
    void closeFeed();
    void onMessage(const QJsonObject &message);
    void applyFeatured(const QJsonObject &data);
    void applyFen(const QJsonObject &data);
    void onFinished(int status, const QString &error);
    void setConnected(bool connected);
    void setError(const QString &error);

    ChessGame *m_game;
    QString m_channel = QStringLiteral("best");
    bool m_active = false;
    QPointer<NdjsonStream> m_stream;
    QTimer m_reconnectTimer;
    QTimer m_clockTimer;
    QElapsedTimer m_clockStamp;
    int m_failures = 0;
    bool m_connected = false;
    QString m_error;

    QString m_gameId;
    QString m_orientation = QStringLiteral("white");
    QVariantMap m_white;
    QVariantMap m_black;
    qint64 m_whiteTime = 0;
    qint64 m_blackTime = 0;
};

#endif // TVFEED_H
