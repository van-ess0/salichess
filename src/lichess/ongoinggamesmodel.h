// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ONGOINGGAMESMODEL_H
#define ONGOINGGAMESMODEL_H

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

class EventStream;
class LichessApi;

// The user's ongoing games (/api/account/playing), most urgent first.
// Exposed to QML as "ongoingGames". Games that start or end are announced on
// the event stream, but an opponent's move is not, so the list is polled
// while a game waits for the opponent (to notice "your turn" in
// correspondence games), more slowly while nothing changes.
class OngoingGamesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int myTurnCount READ myTurnCount NOTIFY countChanged)
    // Opponents in the games where it is my turn, most urgent first.
    Q_PROPERTY(QStringList myTurnOpponents READ myTurnOpponents NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    enum Roles {
        GameIdRole = Qt::UserRole + 1,
        OpponentNameRole,
        OpponentRatingRole,
        OpponentTitleRole,
        ColorRole,          // my color
        FenRole,
        LastMoveRole,
        IsMyTurnRole,
        TurnDeadlineRole,   // correspondence: when the current move is due,
                            // in ms since the epoch; 0 if there is no limit
        SpeedRole,
        PerfRole,
        RatedRole,
        BoardCompatibleRole // playable through the Board API
    };

    OngoingGamesModel(LichessApi *api, EventStream *events, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_games.size(); }
    int myTurnCount() const;
    QStringList myTurnOpponents() const;
    bool loading() const { return m_loading; }
    // The current poll interval, 0 while no poll is scheduled.
    int pollIntervalMs() const { return m_pollTimer.isActive() ? m_pollTimer.interval() : 0; }

    Q_INVOKABLE void refresh();
    // For when the user looks at the games again: refreshes unless the list
    // is recent or a refresh is coming anyway, and polls at full speed again.
    Q_INVOKABLE void refreshIfStale();
    void setPolling(bool enabled);
    void clear();

signals:
    void countChanged();
    void loadingChanged();
    // It became my turn in a game (not reported for the initial load).
    void myTurn(const QString &gameId, const QString &opponentName);
    // A correspondence game from the lobby appeared: someone joined my seek
    // (or I joined one on lichess.org). Reported instead of myTurn.
    void newLobbyGame(const QString &gameId, const QString &opponentName);

private:
    // When the side to move runs out of time, and the move it belongs to.
    struct TurnDeadline {
        QString move;        // the game's lastMove this was worked out for
        QString pendingMove; // the lastMove a request is on its way for
        qint64 deadlineMs = 0;
        bool resolved = false; // a deadline of 0 means "no time limit"
    };

    // Returns whether anything changed.
    bool setGames(const QVector<QVariantMap> &games);
    // Works out when the current move is due in every correspondence game,
    // asking Lichess for the games where it is the opponent's turn.
    void updateDeadlines();
    void fetchDeadline(const QString &gameId, const QString &move);
    void setDeadline(const QString &gameId, const QString &move, qint64 deadlineMs);
    bool waitingForOpponent() const;
    void schedulePoll();

    LichessApi *m_api;
    EventStream *m_events;
    QVector<QVariantMap> m_games;
    QHash<QString, bool> m_lastMyTurn;
    QHash<QString, TurnDeadline> m_deadlines;
    QTimer m_pollTimer;
    QTimer m_refreshDebounce;
    QElapsedTimer m_lastLoad;
    int m_pollIntervalMs;
    int m_unchangedLoads = 0;
    bool m_polling = false;
    bool m_loading = false;
    bool m_loadedOnce = false;
};

#endif // ONGOINGGAMESMODEL_H
