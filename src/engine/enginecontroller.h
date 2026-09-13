// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENGINECONTROLLER_H
#define ENGINECONTROLLER_H

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class AppSettings;
class ChessGame;
class NnueWeights;
class QThread;
class StockfishEngine;

// Stockfish following the board. Exposed to QML as "engine"; there is one of
// it, because one phone can only usefully run one engine.
//
// A page hands it the game it is showing and takes it back again when it
// leaves, which is what starts and stops the analysis. The engine itself
// lives on a thread of its own: reading its networks and searching must not
// hold up the UI.
class EngineController : public QObject
{
    Q_OBJECT
    // The game to follow. Setting it to null stops the engine.
    Q_PROPERTY(ChessGame *game READ game WRITE setGame NOTIFY gameChanged)
    // The engine is switched on in the settings; a page can still be showing
    // without it running.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // The networks are on the phone, so the engine can be switched on.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY stateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY stateChanged)

    // The lines the engine is showing, best first. Each is a map of
    // "cp", "mate", "depth", "pv" (SAN) and "uci" (the move to play).
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(int depth READ depth NOTIFY linesChanged)
    // The best line's evaluation, from white's point of view, as the rest of
    // the app reports evaluations.
    Q_PROPERTY(bool hasEval READ hasEval NOTIFY linesChanged)
    Q_PROPERTY(int evalCp READ evalCp NOTIFY linesChanged)
    Q_PROPERTY(int evalMate READ evalMate NOTIFY linesChanged)
    Q_PROPERTY(qreal winPercent READ winPercent NOTIFY linesChanged)
    // The engine's own best move, for the arrow on the board.
    Q_PROPERTY(int bestMoveFrom READ bestMoveFrom NOTIFY linesChanged)
    Q_PROPERTY(int bestMoveTo READ bestMoveTo NOTIFY linesChanged)

public:
    EngineController(AppSettings *settings, NnueWeights *weights, QObject *parent = nullptr);
    ~EngineController() override;

    ChessGame *game() const { return m_game; }
    void setGame(ChessGame *game);
    bool enabled() const;
    void setEnabled(bool enabled);
    bool available() const;
    bool searching() const { return m_searching; }
    QString errorString() const { return m_error; }

    QVariantList lines() const { return m_lines; }
    int depth() const { return m_depth; }
    bool hasEval() const { return !m_lines.isEmpty(); }
    int evalCp() const;
    int evalMate() const;
    qreal winPercent() const;
    int bestMoveFrom() const;
    int bestMoveTo() const;

    // Downloading the networks and switching the engine on are one step for
    // the user; this is what the button on the analysis page calls.
    Q_INVOKABLE void enableWithDownload();

    // Searching in the background would drain the battery, so the engine
    // rests while the app is away and picks the same board up again when it
    // comes back. main() keeps this in step with the application state.
    void setApplicationActive(bool active);

signals:
    void gameChanged();
    void enabledChanged();
    void availableChanged();
    void stateChanged();
    void linesChanged();

private:
    void onPositionChanged();
    void restart();
    void stop();
    void applySettings();
    void onInfo(int depth, int multiPv, int scoreCp, int mateIn, const QString &pvUci);
    void clearLines();
    void setError(const QString &error);
    // The user wants the engine here, whether or not it can run yet.
    bool wantsEngine() const;
    // Whether the engine should be running right now.
    bool shouldSearch() const;

    AppSettings *m_settings;
    NnueWeights *m_weights;
    QThread *m_thread = nullptr;
    StockfishEngine *m_engine = nullptr;
    ChessGame *m_game = nullptr;

    QTimer m_debounce;
    QVariantList m_lines;
    // The position the lines belong to; results for anything else are stale.
    QString m_searchFen;
    QString m_pendingFen;
    QString m_error;
    int m_depth = 0;
    bool m_whiteToMove = true;
    bool m_searching = false;
    bool m_engineReady = false;
    bool m_appActive = true;
};

#endif // ENGINECONTROLLER_H
