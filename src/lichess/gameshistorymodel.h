// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GAMESHISTORYMODEL_H
#define GAMESHISTORYMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QVector>

class LichessApi;
class NdjsonStream;

// A player's finished games, newest first (/api/games/user/{username}).
// Created in QML:
//     GamesHistoryModel { username: session.username }
//
// The endpoint answers with an ndjson stream rather than a page, so one load
// is one stream that ends on its own. loadMore() asks for the games before
// the oldest one held, which is how the list grows as it is scrolled.
class GamesHistoryModel : public QAbstractListModel
{
    Q_OBJECT
    // Whose games; empty means the logged-in user.
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY usernameChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // More games can be fetched with loadMore().
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY hasMoreChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    // Filters. Setting one reloads the list.
    // Comma-separated perfs ("blitz,rapid"); empty means every speed.
    Q_PROPERTY(QString perfType READ perfType WRITE setPerfType NOTIFY filterChanged)
    // "white", "black", or empty for both.
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY filterChanged)
    Q_PROPERTY(RatedFilter rated READ rated WRITE setRated NOTIFY filterChanged)
    Q_PROPERTY(bool analysedOnly READ analysedOnly WRITE setAnalysedOnly NOTIFY filterChanged)
    // Only games against this opponent.
    Q_PROPERTY(QString opponent READ opponent WRITE setOpponent NOTIFY filterChanged)
    Q_PROPERTY(bool filtered READ filtered NOTIFY filterChanged)

public:
    enum RatedFilter { AnyGames, RatedOnly, CasualOnly };
    Q_ENUM(RatedFilter)

    enum Roles {
        GameIdRole = Qt::UserRole + 1,
        OpponentNameRole,
        OpponentTitleRole,
        OpponentRatingRole,
        ColorRole,          // the colour the player of this history had
        ResultRole,         // "win", "loss", "draw", or "" when aborted
        StatusRole,
        ResultTextRole,
        RatingDiffRole,
        FenRole,            // final position, for the mini board
        SpeedRole,
        PerfRole,
        RatedRole,
        VariantRole,
        OpeningRole,
        CreatedAtRole       // ms since the epoch
    };

    explicit GamesHistoryModel(QObject *parent = nullptr);
    ~GamesHistoryModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString username() const { return m_username; }
    void setUsername(const QString &username);
    int count() const { return m_games.size(); }
    bool loading() const { return m_loading; }
    bool hasMore() const { return m_hasMore; }
    QString errorString() const { return m_error; }

    QString perfType() const { return m_perfType; }
    void setPerfType(const QString &perfType);
    QString color() const { return m_color; }
    void setColor(const QString &color);
    RatedFilter rated() const { return m_rated; }
    void setRated(RatedFilter rated);
    bool analysedOnly() const { return m_analysedOnly; }
    void setAnalysedOnly(bool only);
    QString opponent() const { return m_opponent; }
    void setOpponent(const QString &opponent);
    bool filtered() const;

    // Loads the first page again, dropping what is held.
    Q_INVOKABLE void refresh();
    // Appends the page before the oldest game held.
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void clear();
    // Clears every filter at once, so the list reloads only the once.
    Q_INVOKABLE void clearFilters();

signals:
    void usernameChanged();
    void countChanged();
    void loadingChanged();
    void hasMoreChanged();
    void errorStringChanged();
    void filterChanged();

private:
    struct Game {
        QString id;
        QString opponentName;
        QString opponentTitle;
        int opponentRating = 0;
        QString color;
        QString result;
        QString status;
        QString resultText;
        int ratingDiff = 0;
        QString fen;
        QString speed;
        QString perf;
        bool rated = false;
        QString variant;
        QString opening;
        qint64 createdAt = 0;
    };

    // Property changes arrive one at a time while QML sets up the model;
    // the reload they ask for is collapsed into one.
    void reload();
    void doReload();
    void load(qint64 until);
    void closeStream();
    void appendPage();
    void setLoading(bool loading);
    void setHasMore(bool hasMore);
    void setError(const QString &error);
    // Turns one exported game into a row, from |me|'s point of view.
    static Game parseGame(const QJsonObject &json, const QString &me);

    QString m_username;
    QVector<Game> m_games;
    QSet<QString> m_ids;         // guards against a game arriving twice
    QVector<Game> m_pending;     // the page being streamed
    QPointer<NdjsonStream> m_stream;
    QTimer m_reloadDebounce;
    QString m_error;
    QString m_perfType;
    QString m_color;
    RatedFilter m_rated = AnyGames;
    QString m_opponent;
    bool m_analysedOnly = false;
    bool m_loading = false;
    bool m_hasMore = true;
};

#endif // GAMESHISTORYMODEL_H
