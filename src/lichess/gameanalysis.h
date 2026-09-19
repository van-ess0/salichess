// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GAMEANALYSIS_H
#define GAMEANALYSIS_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class ChessGame;

// One finished game, for reviewing it: the moves, and what Lichess knows
// about them. Created in QML:
//     GameAnalysis { gameId: "abcdefgh" }
// or, for a position that is not from a Lichess game (the analysis board):
//     GameAnalysis { startFen: "..." }
//
// This is the finished-game counterpart of GameController: no stream, no
// clocks running, but the computer analysis Lichess stored with the game
// (evaluations, Inaccuracy/Mistake/Blunder judgments, accuracy and average
// centipawn loss). Games Lichess never analysed fall back to the cloud
// evaluation of the position being looked at, where there is one.
class GameAnalysis : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString gameId READ gameId WRITE setGameId NOTIFY gameIdChanged)
    // Where the board starts when there is no gameId. Only the cloud and the
    // engine have anything to say about such a position.
    Q_PROPERTY(QString startFen READ startFen WRITE setStartFen NOTIFY startFenChanged)
    Q_PROPERTY(ChessGame *game READ game CONSTANT)

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

    Q_PROPERTY(QVariantMap white READ white NOTIFY infoChanged)
    Q_PROPERTY(QVariantMap black READ black NOTIFY infoChanged)
    Q_PROPERTY(QString speed READ speed NOTIFY infoChanged)
    Q_PROPERTY(QString perfName READ perfName NOTIFY infoChanged)
    Q_PROPERTY(bool rated READ rated NOTIFY infoChanged)
    Q_PROPERTY(QString variantName READ variantName NOTIFY infoChanged)
    Q_PROPERTY(QString status READ status NOTIFY infoChanged)
    Q_PROPERTY(QString winner READ winner NOTIFY infoChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY infoChanged)
    Q_PROPERTY(QString openingName READ openingName NOTIFY infoChanged)
    Q_PROPERTY(QString openingEco READ openingEco NOTIFY infoChanged)
    Q_PROPERTY(qint64 createdAt READ createdAt NOTIFY infoChanged)
    Q_PROPERTY(bool hasClocks READ hasClocks NOTIFY infoChanged)
    // Lichess has a computer analysis of this game.
    Q_PROPERTY(bool hasServerAnalysis READ hasServerAnalysis NOTIFY infoChanged)
    // Lichess is working one out because the user asked for it.
    Q_PROPERTY(bool requestingAnalysis READ requestingAnalysis NOTIFY requestChanged)
    Q_PROPERTY(QString requestError READ requestError NOTIFY requestChanged)

    // What is known about the position being viewed. These follow the game's
    // view ply.
    Q_PROPERTY(bool hasEval READ hasEval NOTIFY viewChanged)
    Q_PROPERTY(int evalCp READ evalCp NOTIFY viewChanged)      // centipawns, white's view
    Q_PROPERTY(int evalMate READ evalMate NOTIFY viewChanged)  // moves to mate, 0 if none
    Q_PROPERTY(qreal winPercent READ winPercent NOTIFY viewChanged) // 0..100, white's view
    Q_PROPERTY(QString evalSource READ evalSource NOTIFY viewChanged) // "server", "cloud" or ""
    // The judgment of the move that led here: "Inaccuracy", "Mistake",
    // "Blunder", or empty.
    Q_PROPERTY(QString judgment READ judgment NOTIFY viewChanged)
    Q_PROPERTY(QString judgmentComment READ judgmentComment NOTIFY viewChanged)
    // What should have been played instead of the move that led here, which
    // belongs to the position before this one.
    Q_PROPERTY(QString bestMove READ bestMove NOTIFY viewChanged)   // UCI
    Q_PROPERTY(QString bestVariation READ bestVariation NOTIFY viewChanged) // SAN
    // What Lichess would play in the position on the board. This is the
    // other half of the same entries: the alternative to move n is a move
    // from the position at ply n - 1, so the one belonging here comes from
    // the entry of the move that followed.
    Q_PROPERTY(QString nextBestMove READ nextBestMove NOTIFY viewChanged)   // UCI
    Q_PROPERTY(QString nextBestVariation READ nextBestVariation NOTIFY viewChanged) // SAN
    Q_PROPERTY(int clockMs READ clockMs NOTIFY viewChanged)  // -1 without clocks
    // What each clock read at the position being viewed, in ms; -1 when the
    // game was played without clocks, or that side has not moved yet.
    Q_PROPERTY(int whiteClockMs READ whiteClockMs NOTIFY viewChanged)
    Q_PROPERTY(int blackClockMs READ blackClockMs NOTIFY viewChanged)
    // Lichess judgments, one per ply, so the move list can mark them.
    Q_PROPERTY(QStringList judgments READ judgments NOTIFY infoChanged)

public:
    explicit GameAnalysis(QObject *parent = nullptr);

    QString gameId() const { return m_gameId; }
    void setGameId(const QString &id);
    QString startFen() const { return m_startFen; }
    void setStartFen(const QString &fen);
    ChessGame *game() const { return m_game; }

    bool loading() const { return m_loading; }
    QString errorString() const { return m_error; }

    QVariantMap white() const { return m_white; }
    QVariantMap black() const { return m_black; }
    QString speed() const { return m_speed; }
    QString perfName() const { return m_perf; }
    bool rated() const { return m_rated; }
    QString variantName() const { return m_variant; }
    QString status() const { return m_status; }
    QString winner() const { return m_winner; }
    QString resultText() const;
    QString openingName() const { return m_openingName; }
    QString openingEco() const { return m_openingEco; }
    qint64 createdAt() const { return m_createdAt; }
    bool hasClocks() const { return !m_clocks.isEmpty(); }
    bool hasServerAnalysis() const { return m_hasServerAnalysis; }

    bool hasEval() const;
    int evalCp() const;
    int evalMate() const;
    qreal winPercent() const;
    QString evalSource() const;
    QString judgment() const;
    QString judgmentComment() const;
    QString bestMove() const;
    QString bestVariation() const;
    QString nextBestMove() const;
    QString nextBestVariation() const;
    int clockMs() const;
    int whiteClockMs() const { return clockFor(true); }
    int blackClockMs() const { return clockFor(false); }
    QStringList judgments() const;

    // One entry per ply that has an evaluation: {"ply", "cp", "mate",
    // "win", "judgment"}. Drawn as the evaluation graph.
    Q_INVOKABLE QVariantList evalPoints() const;
    Q_INVOKABLE void reload();
    // Asks Lichess to analyse the game, then waits for it to appear. Lichess
    // has no documented endpoint for this; the route its own apps use is the
    // one taken here, so it can refuse.
    Q_INVOKABLE void requestAnalysis();

    bool requestingAnalysis() const { return m_requesting; }
    QString requestError() const { return m_requestError; }

    // Chances of winning from an evaluation, as Lichess draws them: 0..100
    // from white's point of view.
    static qreal winPercentFromCp(int centipawns);

signals:
    void gameIdChanged();
    void startFenChanged();
    void loadingChanged();
    void errorStringChanged();
    void infoChanged();
    void viewChanged();
    void requestChanged();

private:
    // What Lichess says about the position after one move.
    struct PlyInfo {
        bool hasEval = false;
        int cp = 0;
        int mate = 0;
        QString judgment;
        QString comment;
        QString best;
        QString variation;
    };

    void fetch();
    void applyGame(const QJsonObject &json);
    void resetGameData();
    // The position being viewed, 0 for the starting position.
    int viewPly() const;
    const PlyInfo *viewInfo() const;
    // The entry of the move played from the position being viewed.
    const PlyInfo *nextInfo() const;
    int clockFor(bool white) const;
    void requestCloudEval();
    void pollForAnalysis();
    void endRequest(const QString &error);
    void setLoading(bool loading);
    void setError(const QString &error);

    ChessGame *m_game;
    QString m_gameId;
    QString m_startFen;
    bool m_loading = false;
    QString m_error;

    QVariantMap m_white;
    QVariantMap m_black;
    QString m_speed;
    QString m_perf;
    QString m_variant;
    QString m_status;
    QString m_winner;
    QString m_openingName;
    QString m_openingEco;
    qint64 m_createdAt = 0;
    bool m_rated = false;
    bool m_hasServerAnalysis = false;

    QVector<PlyInfo> m_plies;   // index 0 is the position after the first move
    QVector<int> m_clocks;      // ms left after each move

    // Cloud evaluations of the positions looked at, by FEN. Games Lichess
    // never analysed are the only ones that ask for them.
    struct CloudEval {
        bool known = false;     // false while the answer is still coming
        bool hasEval = false;
        int cp = 0;
        int mate = 0;
    };
    QHash<QString, CloudEval> m_cloudEvals;
    QTimer m_cloudTimer;

    // Waiting for an analysis that was asked for.
    QTimer m_pollTimer;
    QString m_requestError;
    int m_pollsLeft = 0;
    bool m_requesting = false;
};

#endif // GAMEANALYSIS_H
