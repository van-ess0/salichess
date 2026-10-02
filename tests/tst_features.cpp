// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "chess/chessgame.h"
#include "core/appsettings.h"
#include "core/lichessapi.h"
#include "core/services.h"
#include "core/session.h"
#include "core/tokenstore.h"
#include "engine/computergame.h"
#include "engine/nnueweights.h"
#include "fakelichess.h"
#include "lichess/aichallenge.h"
#include "lichess/playersearch.h"
#include "lichess/puzzledashboard.h"
#include "lichess/tvfeed.h"
#include "lichess/userprofile.h"

namespace {

class MemoryStore : public TokenStore
{
public:
    QString load() override { return m_token; }
    bool save(const QString &token) override { m_token = token; return true; }
    void clear() override { m_token.clear(); }

private:
    QString m_token;
};

const char AccountJson[] =
        R"({"id":"me","username":"Me","perfs":{"puzzle":{"rating":1600},"blitz":{"rating":1500}}})";

} // namespace

// The play-against-the-computer, TV, profile and puzzle dashboard features.
class TestFeatures : public QObject
{
    Q_OBJECT

private:
    FakeLichess *server = nullptr;
    LichessApi *api = nullptr;
    MemoryStore *store = nullptr;
    Session *session = nullptr;
    AppSettings *settings = nullptr;
    NnueWeights *weights = nullptr;

    void logIn()
    {
        server->route("GET", "/api/account", 200, AccountJson);
        session->login(QStringLiteral("tok"));
        QTRY_VERIFY(session->loggedIn());
    }

    static QString netDir()
    {
        return QString::fromLocal8Bit(qgetenv("SALICHESS_NNUE_DIR"));
    }

    // Links the networks from SALICHESS_NNUE_DIR to where NnueWeights looks,
    // so the engine can be used without downloading anything. False if the
    // variable is not set.
    bool installNetworks()
    {
        if (netDir().isEmpty())
            return false;
        QDir().mkpath(QFileInfo(weights->bigPath()).absolutePath());
        for (const QString &path : {weights->bigPath(), weights->smallPath()}) {
            QFile::remove(path);
            if (!QFile::link(netDir() + QLatin1Char('/') + QFileInfo(path).fileName(), path))
                return false;
        }
        return weights->ready();
    }

    void removeNetworks()
    {
        QFile::remove(weights->bigPath());
        QFile::remove(weights->smallPath());
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        server = new FakeLichess;
        api = new LichessApi;
        api->setServerUrl(server->url());
        store = new MemoryStore;
        session = new Session(api, store);
        settings = new AppSettings;
        weights = new NnueWeights;
        removeNetworks();
        Services::init(api, session, settings, nullptr, weights);
    }

    void cleanup()
    {
        removeNetworks();
        Services::init(nullptr, nullptr, nullptr, nullptr);
        delete weights;
        delete settings;
        delete session;
        delete store;
        delete api;
        delete server;
    }

    // --- AiChallenge ---

    void aiChallengeStartsAGame()
    {
        logIn();
        server->route("POST", "/api/challenge/ai", 201, R"({"id":"aigame01","variant":{"key":"standard"}})");
        AiChallenge challenge;
        QSignalSpy started(&challenge, &AiChallenge::started);
        challenge.start(4, QStringLiteral("black"), 300, 3);
        QVERIFY(challenge.busy());
        QTRY_COMPARE(started.count(), 1);
        QCOMPARE(started.first().first().toString(), QStringLiteral("aigame01"));
        QVERIFY(!challenge.busy());

        const QUrlQuery form = server->lastRequest("POST", "/api/challenge/ai").form();
        QCOMPARE(form.queryItemValue("level"), QStringLiteral("4"));
        QCOMPARE(form.queryItemValue("color"), QStringLiteral("black"));
        QCOMPARE(form.queryItemValue("clock.limit"), QStringLiteral("300"));
        QCOMPARE(form.queryItemValue("clock.increment"), QStringLiteral("3"));
    }

    void aiChallengeReportsFailure()
    {
        logIn();
        server->route("POST", "/api/challenge/ai", 400, R"({"error":"Too many games"})");
        AiChallenge challenge;
        QSignalSpy failed(&challenge, &AiChallenge::failed);
        challenge.start(9, QStringLiteral("sideways"), 600, 0);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(challenge.errorString(), QStringLiteral("Too many games"));
        // The level and colour are put right rather than refused.
        const QUrlQuery form = server->lastRequest("POST", "/api/challenge/ai").form();
        QCOMPARE(form.queryItemValue("level"), QStringLiteral("8"));
        QCOMPARE(form.queryItemValue("color"), QStringLiteral("random"));
    }

    // --- ComputerGame ---

    // Without the networks the game must not touch Stockfish, which would end
    // the whole app: it waits and says so, and the player's moves, take-backs
    // and resigning still work.
    void computerGameWithoutNetworksWaits()
    {
        QVERIFY(!weights->ready());
        ComputerGame game;
        QVERIFY(!game.engineAvailable());
        game.start(QStringLiteral("white"));
        QCOMPARE(game.state(), ComputerGame::Playing);
        QVERIFY(game.isMyTurn());
        QVERIFY(!game.waitingForEngine());

        game.move(QStringLiteral("e2e4"));
        QCOMPARE(game.game()->ply(), 1);
        QVERIFY(game.waitingForEngine());
        QVERIFY(!game.thinking());
        QVERIFY(!game.isMyTurn());
        // Not my turn: the board is not to take moves.
        game.move(QStringLiteral("d2d4"));
        QCOMPARE(game.game()->ply(), 1);

        game.takeBack();
        QCOMPARE(game.game()->ply(), 0);
        QVERIFY(game.isMyTurn());
        QVERIFY(!game.waitingForEngine());

        game.move(QStringLiteral("g1f3"));
        game.resign();
        QCOMPARE(game.state(), ComputerGame::Finished);
        QCOMPARE(game.winner(), QStringLiteral("black"));
        QVERIFY(game.resultText().contains(QStringLiteral("You resigned")));
    }

    void computerGameAsBlackWaitsAtOnce()
    {
        ComputerGame game;
        QSignalSpy changed(&game, &ComputerGame::stateChanged);
        game.start(QStringLiteral("black"));
        QVERIFY(game.waitingForEngine());
        QVERIFY(!game.isMyTurn());
        QCOMPARE(game.game()->ply(), 0);
        // Nothing to take back: the computer has not moved.
        game.takeBack();
        QCOMPARE(game.game()->ply(), 0);
        QVERIFY(changed.count() > 0);
    }

    void computerGameNeedsLegalMoves()
    {
        ComputerGame game;
        game.start(QStringLiteral("white"));
        game.move(QStringLiteral("e2e5"));
        game.move(QStringLiteral("nonsense"));
        QCOMPARE(game.game()->ply(), 0);
    }

    void computerGameLevelsGetStronger()
    {
        for (int level = 2; level <= 8; ++level) {
            QVERIFY(ComputerGame::skillForLevel(level) >= ComputerGame::skillForLevel(level - 1));
            QVERIFY(ComputerGame::depthForLevel(level) >= ComputerGame::depthForLevel(level - 1));
            QVERIFY(ComputerGame::moveTimeForLevel(level) >= ComputerGame::moveTimeForLevel(level - 1));
        }
        QCOMPARE(ComputerGame::skillForLevel(99), ComputerGame::skillForLevel(8));
        QCOMPARE(ComputerGame::skillForLevel(-3), ComputerGame::skillForLevel(1));
    }

    // With the networks in place the computer answers, and a game that was
    // waiting for them carries on by itself.
    void computerGamePlays()
    {
        if (netDir().isEmpty())
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");

        ComputerGame game;
        game.setLevel(1);
        game.start(QStringLiteral("black"));
        QVERIFY(game.waitingForEngine()); // no networks yet

        QVERIFY(installNetworks());
        // The weights object only looks at the files when asked.
        QMetaObject::invokeMethod(weights, "readyChanged");
        QTRY_VERIFY_WITH_TIMEOUT(game.game()->ply() == 1, 60000);
        QVERIFY(game.isMyTurn());
        QVERIFY(!game.waitingForEngine());

        // A move and its answer; then take both back.
        game.move(QStringLiteral("g8f6"));
        QCOMPARE(game.game()->ply(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(game.game()->ply() == 3, 60000);
        game.takeBack();
        QCOMPARE(game.game()->ply(), 1);
        QVERIFY(game.isMyTurn());

        // Taking back while it thinks drops its answer.
        game.move(QStringLiteral("g8f6"));
        game.takeBack();
        QCOMPARE(game.game()->ply(), 1);
        QTest::qWait(300);
        QVERIFY(game.isMyTurn());
        QCOMPARE(game.game()->ply(), 1);
    }

    // The engine can find mate in one, whatever the level.
    void computerGameFinishesAGame()
    {
        if (netDir().isEmpty())
            QSKIP("set SALICHESS_NNUE_DIR to a directory holding the two Stockfish networks");
        QVERIFY(installNetworks());

        ComputerGame game;
        game.setLevel(8);
        game.start(QStringLiteral("white"));
        // Fool's mate: the computer is black and the player plays badly.
        game.move(QStringLiteral("f2f3"));
        QTRY_VERIFY_WITH_TIMEOUT(game.game()->ply() == 2, 60000);
        game.move(QStringLiteral("g2g4"));
        QTRY_VERIFY_WITH_TIMEOUT(game.gameOver() || game.game()->ply() == 4, 60000);
        // A level 8 engine does not miss mate in one.
        QTRY_VERIFY(game.gameOver());
        QCOMPARE(game.status(), QStringLiteral("mate"));
        QCOMPARE(game.winner(), QStringLiteral("black"));
    }

    // --- UserProfile / PlayerSearch ---

    void userProfileLoads()
    {
        server->route("GET", "/api/user/Magnus", 200, R"({
            "id":"magnus","username":"Magnus","title":"GM","online":true,"playing":false,
            "createdAt":1290415680000,"seenAt":1700000000000,"playTime":{"total":86400,"tv":100},
            "nbFollowers":1234,
            "profile":{"bio":"Chess player.","location":"Oslo"},
            "count":{"all":3000,"rated":2900,"win":1700,"loss":900,"draw":400,"ai":10},
            "perfs":{
              "bullet":{"games":100,"rating":3100,"rd":50,"prog":12,"prov":false},
              "blitz":{"games":0,"rating":1500,"rd":350,"prog":0,"prov":true},
              "rapid":{"games":20,"rating":2900,"rd":80,"prog":-4,"prov":true},
              "puzzle":{"games":5,"rating":2500,"rd":60,"prog":3}
            }})");
        server->route("GET", "/api/user/Magnus/rating-history", 200, R"([
            {"name":"Bullet","points":[[2020,0,31,2900],[2020,1,15,3000],[2021,11,1,3100]]},
            {"name":"Blitz","points":[]}])");

        UserProfile profile;
        QSignalSpy history(&profile, &UserProfile::historyChanged);
        profile.setUsername(QStringLiteral("Magnus"));
        QVERIFY(profile.loading());
        QTRY_VERIFY(profile.loaded());
        QCOMPARE(profile.name(), QStringLiteral("Magnus"));
        QCOMPARE(profile.title(), QStringLiteral("GM"));
        QVERIFY(profile.online());
        QVERIFY(!profile.playing());
        QCOMPARE(profile.bio(), QStringLiteral("Chess player."));
        QCOMPARE(profile.location(), QStringLiteral("Oslo"));
        QCOMPARE(profile.playTime(), 86400);
        QCOMPARE(profile.followers(), 1234);
        QCOMPARE(profile.counts().value("win").toInt(), 1700);

        // Ratings never played are left out; the puzzle one is kept.
        const QVariantList ratings = profile.ratings();
        QStringList names;
        for (const QVariant &rating : ratings)
            names.append(rating.toMap().value("name").toString());
        QCOMPARE(names, QStringList() << "Bullet" << "Rapid" << "Puzzles");
        QCOMPARE(ratings.first().toMap().value("rating").toInt(), 3100);
        QCOMPARE(ratings.first().toMap().value("prog").toInt(), 12);
        QVERIFY(ratings.at(1).toMap().value("provisional").toBool());

        // The history follows; a perf with no points is not offered.
        QTRY_COMPARE(profile.historyNames(), QStringList() << "Bullet");
        const QVariantList points = profile.historyPoints("Bullet");
        QCOMPARE(points.size(), 3);
        // Months count from 0: this is 31 January 2020.
        QCOMPARE(QDateTime::fromMSecsSinceEpoch(points.first().toList().first().toLongLong(), Qt::UTC).date(),
                 QDate(2020, 1, 31));
        QCOMPARE(points.last().toList().last().toInt(), 3100);
    }

    void userProfileErrors()
    {
        UserProfile profile;
        profile.setUsername(QStringLiteral("nobody-here"));
        QTRY_VERIFY(!profile.loading());
        QCOMPARE(profile.errorString(), QStringLiteral("No such player"));
        QVERIFY(!profile.loaded());

        profile.setUsername(QStringLiteral("x"));
        QVERIFY(!profile.errorString().isEmpty());
        QVERIFY(!profile.loading());
        QCOMPARE(server->requestCount("GET", "/api/user/x"), 0);

        profile.setUsername(QString());
        QVERIFY(profile.errorString().isEmpty());
    }

    void playerSearch()
    {
        server->route("GET", "/api/player/autocomplete", 200, R"({"result":[
            {"id":"magnus","name":"Magnus","title":"GM","online":true},
            {"id":"magnuss","name":"MagnusS","patron":true}]})");
        PlayerSearch search;
        search.setTerm(QStringLiteral("m"));
        QTest::qWait(500);
        QCOMPARE(server->requestCount("GET", "/api/player/autocomplete"), 0); // too short

        search.setTerm(QStringLiteral(" magn "));
        QTRY_COMPARE(search.results().size(), 2);
        QCOMPARE(search.results().first().toMap().value("name").toString(), QStringLiteral("Magnus"));
        QCOMPARE(search.results().first().toMap().value("title").toString(), QStringLiteral("GM"));
        QVERIFY(search.results().at(1).toMap().value("patron").toBool());
        const QUrlQuery query = server->lastRequest("GET", "/api/player/autocomplete").query;
        QCOMPARE(query.queryItemValue("term"), QStringLiteral("magn"));
        QCOMPARE(query.queryItemValue("object"), QStringLiteral("true"));

        // Clearing the box clears the list.
        search.setTerm(QString());
        QVERIFY(search.results().isEmpty());
    }

    // --- TvFeed ---

    void tvCompletesFens()
    {
        QCOMPARE(TvFeed::completeFen(QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w")),
                 QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
        // Kings and rooks that moved lose their rights.
        QCOMPARE(TvFeed::completeFen(QStringLiteral("r3k2r/8/8/8/8/8/8/R4K1R b")),
                 QStringLiteral("r3k2r/8/8/8/8/8/8/R4K1R b kq - 0 1"));
        QCOMPARE(TvFeed::completeFen(QStringLiteral("4k3/8/8/8/8/8/8/4K3 w")),
                 QStringLiteral("4k3/8/8/8/8/8/8/4K3 w - - 0 1"));
        QVERIFY(TvFeed::completeFen(QStringLiteral("garbage")).isEmpty());
        QVERIFY(TvFeed::completeFen(QStringLiteral("8/8/8 w")).isEmpty());
    }

    void tvFollowsTheFeed()
    {
        server->streamRoute("GET", "/api/tv/feed");
        TvFeed tv;
        QSignalSpy gameChanged(&tv, &TvFeed::gameChanged);
        QVERIFY(!tv.active());
        tv.setActive(true);
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 1);

        server->push("/api/tv/feed",
                     R"({"t":"featured","d":{"id":"abcd1234","orientation":"black","players":[{"color":"white","user":{"name":"Alice","id":"alice","title":"GM"},"rating":2700,"seconds":180},{"color":"black","user":{"name":"Bob","id":"bob"},"rating":2650,"seconds":175}],"fen":"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w"}})" "\n");
        QTRY_COMPARE(gameChanged.count(), 1);
        QVERIFY(tv.connected());
        QCOMPARE(tv.gameId(), QStringLiteral("abcd1234"));
        QCOMPARE(tv.orientation(), QStringLiteral("black"));
        QCOMPARE(tv.white().value("name").toString(), QStringLiteral("Alice"));
        QCOMPARE(tv.white().value("title").toString(), QStringLiteral("GM"));
        QCOMPARE(tv.white().value("rating").toInt(), 2700);
        QCOMPARE(tv.black().value("name").toString(), QStringLiteral("Bob"));
        QCOMPARE(tv.game()->ply(), 0);
        QVERIFY(tv.whiteTime() <= 180000 && tv.whiteTime() > 178000);
        QCOMPARE(tv.blackTime(), 175000);
        QCOMPARE(tv.runningClock(), QStringLiteral("white"));

        // A move is played on the board, so the pieces can slide.
        server->push("/api/tv/feed",
                     R"({"t":"fen","d":{"fen":"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b","lm":"e2e4","wc":179,"bc":175}})" "\n");
        QTRY_COMPARE(tv.game()->ply(), 1);
        QCOMPARE(tv.game()->lastMoveTo(), 28);
        QCOMPARE(tv.runningClock(), QStringLiteral("black"));
        QCOMPARE(tv.whiteTime(), 179000);

        // A position that does not follow from the last one replaces it.
        server->push("/api/tv/feed",
                     R"({"t":"fen","d":{"fen":"4k3/8/8/8/8/8/4P3/4K3 w","lm":"a1a2","wc":10,"bc":10}})" "\n");
        QTRY_COMPARE(tv.game()->ply(), 0);
        QVERIFY(tv.game()->fen().startsWith(QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 w")));

        // Castling is played even though the feed never sends rights.
        server->push("/api/tv/feed",
                     R"({"t":"featured","d":{"id":"zzzz9999","orientation":"white","players":[],"fen":"r3k2r/8/8/8/8/8/8/R3K2R w"}})" "\n");
        QTRY_COMPARE(tv.gameId(), QStringLiteral("zzzz9999"));
        server->push("/api/tv/feed",
                     R"({"t":"fen","d":{"fen":"r3k2r/8/8/8/8/8/8/R4RK1 b","lm":"e1g1","wc":10,"bc":10}})" "\n");
        QTRY_COMPARE(tv.game()->ply(), 1);
        QCOMPARE(tv.game()->lastMoveTo(), 6);
    }

    void tvChangesChannelAndStops()
    {
        server->streamRoute("GET", "/api/tv/feed");
        server->streamRoute("GET", "/api/tv/blitz/feed");
        TvFeed tv;
        tv.setActive(true);
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 1);

        tv.setChannel(QStringLiteral("blitz"));
        QTRY_COMPARE(server->openStreams("/api/tv/blitz/feed"), 1);
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 0);

        // Not a channel: falls back to the top rated one.
        tv.setChannel(QStringLiteral("../../account"));
        QCOMPARE(tv.channel(), QStringLiteral("best"));
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 1);

        tv.setActive(false);
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 0);
        QVERIFY(!tv.connected());
    }

    void tvReconnects()
    {
        server->streamRoute("GET", "/api/tv/feed");
        TvFeed tv;
        tv.setActive(true);
        QTRY_COMPARE(server->openStreams("/api/tv/feed"), 1);
        server->push("/api/tv/feed", "\n{}\n");
        QTRY_VERIFY(tv.connected());
        server->closeStreams("/api/tv/feed");
        QTRY_VERIFY(!tv.connected());
        QTRY_COMPARE_WITH_TIMEOUT(server->openStreams("/api/tv/feed"), 1, 10000);
    }

    // --- PuzzleDashboard ---

    void puzzleDashboard()
    {
        logIn();
        server->route("GET", "/api/puzzle/dashboard/30", 200, R"({"days":30,
            "global":{"nb":120,"firstWins":80,"replayWins":10,"performance":1650,"puzzleRatingAvg":1600},
            "themes":{
              "fork":{"theme":"fork","results":{"nb":20,"firstWins":19,"replayWins":1,"performance":1700}},
              "pin":{"theme":"pin","results":{"nb":30,"firstWins":9,"replayWins":3,"performance":1500}},
              "mate":{"theme":"mate","results":{"nb":10,"firstWins":8,"replayWins":0,"performance":1700}},
              "skewer":{"theme":"skewer","results":{"nb":2,"firstWins":0,"replayWins":0,"performance":1000}},
              "endgame":{"theme":"endgame","results":{"nb":8,"firstWins":2,"replayWins":1,"performance":1400}}}})");
        server->streamRoute("GET", "/api/puzzle/activity");

        PuzzleDashboard dashboard;
        dashboard.reload();
        QVERIFY(dashboard.loading());
        QTRY_COMPARE(dashboard.puzzleCount(), 120);
        QCOMPARE(dashboard.firstWins(), 80);
        QCOMPARE(dashboard.replayWins(), 10);
        QCOMPARE(dashboard.performance(), 1650);
        QVERIFY(!dashboard.loading());

        // Most played first.
        QCOMPARE(dashboard.themes().first().toMap().value("theme").toString(), QStringLiteral("pin"));
        QCOMPARE(dashboard.themes().first().toMap().value("solvedPercent").toInt(), 30);
        QCOMPARE(dashboard.themes().size(), 5);

        // The skewer was played too seldom to count; of the four left the two
        // best and the two worst are named.
        QCOMPARE(dashboard.strongest().size(), 2);
        QCOMPARE(dashboard.strongest().first().toMap().value("theme").toString(), QStringLiteral("fork"));
        QCOMPARE(dashboard.strongest().at(1).toMap().value("theme").toString(), QStringLiteral("mate"));
        QCOMPARE(dashboard.weakest().first().toMap().value("theme").toString(), QStringLiteral("endgame"));

        // The recent puzzles stream in afterwards.
        QSignalSpy recent(&dashboard, &PuzzleDashboard::recentChanged);
        QTRY_COMPARE(server->openStreams("/api/puzzle/activity"), 1);
        server->push("/api/puzzle/activity",
                     R"({"date":1700000000000,"win":true,"puzzle":{"id":"p1","rating":1500,"themes":["fork","short"]}})" "\n"
                     R"({"date":1699990000000,"win":false,"puzzle":{"id":"p2","rating":1700,"themes":[]}})" "\n");
        server->closeStreams("/api/puzzle/activity");
        QTRY_COMPARE(recent.count(), 1);
        QCOMPARE(dashboard.recent().size(), 2);
        QCOMPARE(dashboard.recent().first().toMap().value("id").toString(), QStringLiteral("p1"));
        QVERIFY(dashboard.recent().first().toMap().value("win").toBool());
        QVERIFY(!dashboard.recent().at(1).toMap().value("win").toBool());
        QCOMPARE(dashboard.recent().first().toMap().value("themes").toStringList(),
                 QStringList() << "fork" << "short");
        QCOMPARE(server->lastRequest("GET", "/api/puzzle/activity").query.queryItemValue("max"),
                 QStringLiteral("20"));
    }

    void puzzleDashboardNeedsLogin()
    {
        PuzzleDashboard dashboard;
        dashboard.reload();
        QVERIFY(!dashboard.errorString().isEmpty());
        QVERIFY(!dashboard.loading());
        QCOMPARE(server->requestCount("GET", "/api/puzzle/dashboard/30"), 0);
    }

    void puzzleDashboardFailure()
    {
        logIn();
        server->route("GET", "/api/puzzle/dashboard/7", 500, R"({"error":"Broken"})");
        server->route("GET", "/api/puzzle/activity", 500, R"({"error":"Broken"})");
        PuzzleDashboard dashboard;
        dashboard.setDays(7);
        QTRY_VERIFY(!dashboard.errorString().isEmpty());
        QCOMPARE(dashboard.errorString(), QStringLiteral("Broken"));
        QCOMPARE(dashboard.puzzleCount(), 0);
    }
};

int runFeatureTests(int argc, char *argv[])
{
    TestFeatures test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_features.moc"
