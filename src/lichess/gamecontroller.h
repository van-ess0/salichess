// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GAMECONTROLLER_H
#define GAMECONTROLLER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantMap>

class ChatModel;
class ChessGame;
class NdjsonStream;

// Plays one Lichess game through the Board API: streams its state, sends
// moves and game actions, runs the clocks. Created in QML:
//     GameController { gameId: "abcdefgh" }
class GameController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString gameId READ gameId WRITE setGameId NOTIFY gameIdChanged)
    Q_PROPERTY(ChessGame *game READ game CONSTANT)
    Q_PROPERTY(ChatModel *chat READ chat CONSTANT)

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    Q_PROPERTY(QString myColor READ myColor NOTIFY infoChanged) // "white", "black", "" if not playing
    Q_PROPERTY(QVariantMap white READ white NOTIFY infoChanged) // id, name, rating, title, provisional
    Q_PROPERTY(QVariantMap black READ black NOTIFY infoChanged)
    Q_PROPERTY(QString speed READ speed NOTIFY infoChanged)
    Q_PROPERTY(QString perfName READ perfName NOTIFY infoChanged)
    Q_PROPERTY(bool rated READ rated NOTIFY infoChanged)
    Q_PROPERTY(QString variantName READ variantName NOTIFY infoChanged)
    Q_PROPERTY(bool hasClock READ hasClock NOTIFY infoChanged)
    Q_PROPERTY(int daysPerTurn READ daysPerTurn NOTIFY infoChanged)

    Q_PROPERTY(int whiteTime READ whiteTime NOTIFY clockChanged) // ms
    Q_PROPERTY(int blackTime READ blackTime NOTIFY clockChanged)
    Q_PROPERTY(QString runningClock READ runningClock NOTIFY clockChanged) // "white", "black" or ""

    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString winner READ winner NOTIFY stateChanged)
    Q_PROPERTY(bool gameOver READ gameOver NOTIFY stateChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY stateChanged)
    Q_PROPERTY(bool isMyTurn READ isMyTurn NOTIFY stateChanged)
    Q_PROPERTY(bool canAbort READ canAbort NOTIFY stateChanged)
    Q_PROPERTY(bool canTakeback READ canTakeback NOTIFY stateChanged)
    Q_PROPERTY(bool opponentOffersDraw READ opponentOffersDraw NOTIFY stateChanged)
    Q_PROPERTY(bool opponentProposesTakeback READ opponentProposesTakeback NOTIFY stateChanged)
    Q_PROPERTY(bool iOfferDraw READ iOfferDraw NOTIFY stateChanged)
    Q_PROPERTY(bool iProposeTakeback READ iProposeTakeback NOTIFY stateChanged)
    Q_PROPERTY(bool opponentGone READ opponentGone NOTIFY stateChanged)
    Q_PROPERTY(int claimWinInSeconds READ claimWinInSeconds NOTIFY stateChanged)

public:
    explicit GameController(QObject *parent = nullptr);
    ~GameController() override;

    QString gameId() const { return m_gameId; }
    void setGameId(const QString &id);
    ChessGame *game() const { return m_game; }
    ChatModel *chat() const { return m_chat; }

    bool loading() const { return m_loading; }
    bool connected() const { return m_connected; }
    QString errorString() const { return m_error; }

    QString myColor() const { return m_myColor; }
    QVariantMap white() const { return m_white; }
    QVariantMap black() const { return m_black; }
    QString speed() const { return m_speed; }
    QString perfName() const { return m_perfName; }
    bool rated() const { return m_rated; }
    QString variantName() const { return m_variantName; }
    bool hasClock() const { return m_hasClock; }
    int daysPerTurn() const { return m_daysPerTurn; }

    int whiteTime() const;
    int blackTime() const;
    QString runningClock() const;

    QString status() const { return m_status; }
    QString winner() const { return m_winner; }
    bool gameOver() const;
    QString resultText() const;
    bool isMyTurn() const;
    bool canAbort() const;
    bool canTakeback() const;
    bool opponentOffersDraw() const;
    bool opponentProposesTakeback() const;
    bool iOfferDraw() const;
    bool iProposeTakeback() const;
    bool opponentGone() const { return m_opponentGone; }
    int claimWinInSeconds() const { return m_claimWinInSeconds; }

    Q_INVOKABLE void move(const QString &uci);
    Q_INVOKABLE void resign();
    Q_INVOKABLE void abort();
    Q_INVOKABLE void offerDraw();
    Q_INVOKABLE void answerDraw(bool accept);
    Q_INVOKABLE void proposeTakeback();
    Q_INVOKABLE void answerTakeback(bool accept);
    Q_INVOKABLE void claimVictory();
    Q_INVOKABLE void sendChat(const QString &text);
    Q_INVOKABLE void reconnect();

signals:
    void gameIdChanged();
    void loadingChanged();
    void connectedChanged();
    void errorStringChanged();
    void infoChanged();
    void clockChanged();
    void stateChanged();
    void moveRejected(const QString &error);
    void actionFailed(const QString &error);

private:
    void connectStream();
    void closeStream();
    void onMessage(const QJsonObject &message);
    void onStreamFinished(int status, const QString &error);
    void applyGameFull(const QJsonObject &full);
    void applyState(const QJsonObject &state);
    void loadChatHistory();
    void post(const QString &action, const QUrlQuery &form = QUrlQuery());
    void updateClockTimer();
    void setLoading(bool loading);
    void setConnected(bool connected);
    void setError(const QString &error);
    bool whiteToMove() const;

    ChessGame *m_game;
    ChatModel *m_chat;
    QString m_gameId;
    QPointer<NdjsonStream> m_stream;
    QTimer m_reconnectTimer;
    int m_failures = 0;
    bool m_loading = false;
    bool m_connected = false;
    bool m_chatLoaded = false;
    QString m_error;

    QString m_myColor;
    QVariantMap m_white;
    QVariantMap m_black;
    QString m_speed;
    QString m_perfName;
    bool m_rated = false;
    QString m_variantName;
    bool m_hasClock = false;
    int m_daysPerTurn = 0;

    // Clock: times at the moment of the last update, plus elapsed time.
    qint64 m_whiteTime = 0;
    qint64 m_blackTime = 0;
    QElapsedTimer m_clockStamp;
    QTimer m_clockTimer;

    QStringList m_serverMoves;
    QString m_pendingMove; // sent but not yet confirmed by the server
    QString m_status;
    QString m_winner;
    bool m_wdraw = false;
    bool m_bdraw = false;
    bool m_wtakeback = false;
    bool m_btakeback = false;
    bool m_opponentGone = false;
    int m_claimWinInSeconds = 0;
};

#endif // GAMECONTROLLER_H
