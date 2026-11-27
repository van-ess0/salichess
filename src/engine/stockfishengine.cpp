// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stockfishengine.h"
#include "core/appsettings.h"
#include "core/logging.h"

#include "nnueweights.h"
#include "chess/chessposition.h"

#include <QDebug>

#include "attacks.h"
#include "bitboard.h"
#include "engine.h"
#include "misc.h"
#include "position.h"
#include "search.h"
#include "tune.h"
#include "ucioption.h"

#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Stockfish parses "startpos" in its UCI layer, not in Position::set, which
// asserts on anything that is not a real FEN.
const char StartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// std::visit needs one callable with an overload per alternative.
template<typename... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template<typename... Ts>
Overload(Ts...) -> Overload<Ts...>;

// The move tables Stockfish looks moves up in are global and are filled in
// by its own main(), which this app does not have. Without them the very
// first position it is given walks off the end of them.
void initStockfishTables()
{
    static std::once_flag once;
    std::call_once(once, []() {
        Stockfish::Attacks::init();
        Stockfish::Position::init();
    });
}

} // namespace

void StockfishEngine::readScore(const Stockfish::Score &score, int &cp, int &mate)
{
    cp = 0;
    mate = 0;
    score.visit(Overload{
        [&](Stockfish::Score::Mate m) {
            mate = (m.plies > 0 ? (m.plies + 1) : m.plies) / 2;
            // A mate that is already on the board still needs a sign.
            if (mate == 0)
                mate = m.plies > 0 ? 1 : -1;
        },
        [&](Stockfish::Score::Tablebase tb) { cp = tb.win ? 20000 - tb.plies : -20000 - tb.plies; },
        [&](Stockfish::Score::InternalUnits units) { cp = units.value; },
    });
}

StockfishEngine::StockfishEngine(QObject *parent)
    : QObject(parent)
{
}

StockfishEngine::~StockfishEngine()
{
    unload();
}

bool StockfishEngine::ensureEngine()
{
    if (m_engine)
        return true;
    initStockfishTables();
    // Without a path Stockfish looks for its network in the working
    // directory, which is not where ours is; it is named in full later.
    m_engine.reset(new Stockfish::Engine());
    // Stockfish's search constants are tunable at run time, and the tuner
    // has to be told about the options once the engine owns them.
    Stockfish::Tune::init(m_engine->get_options());

    // Every callback Stockfish may reach for has to be set. It calls them
    // without checking, so an unset one is an empty std::function and throws
    // std::bad_function_call, which ends the whole app rather than the
    // search. onUpdateNoMoves is the one that bites: it is called whenever
    // the position has no legal moves, which is every checkmate and
    // stalemate, and the last position of a game that ended in mate is
    // exactly what the analysis board opens on.
    m_engine->set_on_update_full([this](const Stockfish::Engine::InfoFull &full) {
        int cp = 0;
        int mate = 0;
        readScore(full.score, cp, mate);
        // Called from a search thread: the signal is queued to whoever is
        // listening on another one.
        emit info(full.depth, int(full.multiPV), cp, mate,
                  QString::fromUtf8(full.pv.data(), int(full.pv.size())));
    });

    m_engine->set_on_update_no_moves([this](const Stockfish::Engine::InfoShort &short_) {
        int cp = 0;
        int mate = 0;
        readScore(short_.score, cp, mate);
        // No moves to give, but the score still says how it ended.
        emit info(short_.depth, 1, cp, mate, QString());
    });

    m_engine->set_on_iter([](const Stockfish::Engine::InfoIter &) {
        // Which move the search is on is of no use here, but it must not be
        // left unset.
    });

    m_engine->set_on_bestmove([this](std::string_view best, std::string_view) {
        // "(none)" is what Stockfish says when there is no legal move.
        const QString uci = QString::fromUtf8(best.data(), int(best.size()));
        emit bestMove(m_requestId.load(), uci == QLatin1String("(none)") ? QString() : uci);
        emit searchFinished();
    });

    // Called when a search starts, before anything else.
    m_engine->set_on_start([]() {});

    // Stockfish checks its network before every single search and says so
    // each time. Only the complaint matters: it ends the process right after
    // making it. The rest is the same few lines over and over, so they are
    // kept to once per load and out of the warnings.
    m_engine->set_on_verify_network([this](std::string_view message) {
        const QString text =
                QString::fromUtf8(message.data(), int(message.size())).trimmed();
        if (text.contains(QLatin1String("ERROR"))) {
            qCWarning(lcEngine, "Stockfish: %s", qPrintable(text));
            return;
        }
        if (m_verifyMessages < 3) {
            ++m_verifyMessages; // the network itself, and where it is held
            qCDebug(lcEngine, "Stockfish: %s", qPrintable(text));
        }
    });

    applyOption("Threads", m_threads);
    applyOption("Hash", m_hashMb);
    applyOption("MultiPV", m_multiPv);
    applyOption("Skill Level", m_skill);
    return true;
}

void StockfishEngine::applyOption(const char *name, const std::string &value)
{
    if (!m_engine)
        return;
    // Options are set the way a UCI "setoption" line would, which is what
    // makes Stockfish act on them (resizing the thread pool, reading a
    // network, ...).
    std::istringstream command("name " + std::string(name) + " value " + value);
    m_engine->get_options().setoption(command);
}

void StockfishEngine::applyOption(const char *name, int value)
{
    applyOption(name, std::to_string(value));
}

void StockfishEngine::load(const QString &network)
{
    // Stockfish calls exit() when a search starts without a network, so the
    // file is checked here before it ever sees it.
    if (!NnueWeights::looksUsable(network)) {
        m_ready = false;
        emit readyChanged(false);
        emit failed(tr("The engine network is missing"));
        return;
    }
    if (!ensureEngine())
        return;

    m_verifyMessages = 0; // say which network this is, once
    applyOption("EvalFile", network.toStdString());

    m_ready = true;
    emit readyChanged(true);
}

void StockfishEngine::unload()
{
    if (!m_engine)
        return;
    m_engine->stop();
    m_engine->wait_for_search_finished();
    m_engine.reset();
    m_searching = false;
    if (m_ready) {
        m_ready = false;
        emit readyChanged(false);
    }
}

void StockfishEngine::setThreads(int threads)
{
    m_threads = qBound(1, threads, 8);
    applyOption("Threads", m_threads);
}

void StockfishEngine::setHashSize(int megabytes)
{
    m_hashMb = qBound(1, megabytes, AppSettings::maxEngineHash());
    applyOption("Hash", m_hashMb);
}

void StockfishEngine::setMultiPv(int lines)
{
    m_multiPv = qBound(1, lines, 5);
    applyOption("MultiPV", m_multiPv);
}

void StockfishEngine::setSkillLevel(int level)
{
    m_skill = qBound(0, level, 20);
    applyOption("Skill Level", m_skill);
}

void StockfishEngine::search(const QString &fen, const QStringList &moves, int depth)
{
    startSearch(fen, moves, depth, 0);
}

void StockfishEngine::play(const QString &fen, const QStringList &moves, int depth, int movetimeMs,
                           int requestId)
{
    // Whatever was running is stopped first, and its answer has been given
    // by the time the new id is set.
    stop();
    m_requestId = requestId;
    startSearch(fen, moves, depth, movetimeMs);
}

void StockfishEngine::startSearch(const QString &fen, const QStringList &moves, int depth, int movetimeMs)
{
    if (!m_ready || !m_engine) {
        // Asked for a move before the networks are in: say so, rather than
        // leave the caller waiting for an answer that never comes.
        if (movetimeMs > 0)
            emit failed(tr("The engine is not ready"));
        return;
    }
    stop();

    // Stockfish trusts its input. A position the rules engine refuses, or
    // one with more pieces than a game can have (easy to set up by hand),
    // makes it write past its arrays and take the app down with it.
    const bool start = fen.isEmpty() || fen == QLatin1String("startpos");
    ChessPosition position;
    if (!start && (!position.setFen(fen) || !position.hasStandardMaterial())) {
        emit failed(tr("Stockfish cannot analyse this position"));
        emit searchFinished();
        return;
    }

    std::vector<std::string> uciMoves;
    uciMoves.reserve(moves.size());
    for (const QString &move : moves)
        uciMoves.push_back(move.toStdString());
    if (m_engine->set_position(start ? std::string(StartFen) : fen.toStdString(), uciMoves)) {
        // Stockfish 19 checks positions and moves itself and says no.
        emit failed(tr("Stockfish cannot analyse this position"));
        emit searchFinished();
        return;
    }

    Stockfish::Search::LimitsType limits;
    limits.startTime = Stockfish::now();
    limits.depth = qBound(1, depth, 40);
    if (movetimeMs > 0)
        limits.movetime = movetimeMs;
    m_searching = true;
    m_engine->go(limits);
}

void StockfishEngine::stop()
{
    if (!m_engine || !m_searching)
        return;
    m_engine->stop();
    m_engine->wait_for_search_finished();
    m_searching = false;
}
