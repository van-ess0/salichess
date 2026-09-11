// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QScopedPointer>
#include <QtQml>

#include <sailfishapp.h>

#include "chess/chessgame.h"
#include "chess/piecesmodel.h"
#include "core/appsettings.h"
#include "core/appactivation.h"
#include "core/lichessapi.h"
#include "core/pieceimageprovider.h"
#include "core/services.h"
#include "core/session.h"
#include "core/secretstokenstore.h"
#include "lichess/challengesmodel.h"
#include "lichess/chatmodel.h"
#include "lichess/eventstream.h"
#include "lichess/friendsmodel.h"
#include "lichess/gamecontroller.h"
#include "lichess/ongoinggamesmodel.h"
#include "lichess/outgoingchallenge.h"
#include "lichess/outgoingchallenges.h"
#include "lichess/puzzlecontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match the [X-Sailjail] section of the .desktop file: these decide
    // the config/data paths the sandbox allows.
    app->setOrganizationName(QStringLiteral("io.github.vaness0"));
    app->setApplicationName(QStringLiteral("harbour-salichess"));
    app->setApplicationVersion(QStringLiteral("0.1"));

    const char *uri = "harbour.salichess";
    qmlRegisterType<GameController>(uri, 1, 0, "GameController");
    qmlRegisterType<PuzzleController>(uri, 1, 0, "PuzzleController");
    qmlRegisterUncreatableType<ChessGame>(uri, 1, 0, "ChessGame", QStringLiteral("Owned by controllers"));
    qmlRegisterUncreatableType<PiecesModel>(uri, 1, 0, "PiecesModel", QStringLiteral("Owned by ChessGame"));
    qmlRegisterUncreatableType<ChatModel>(uri, 1, 0, "ChatModel", QStringLiteral("Owned by GameController"));
    qmlRegisterUncreatableType<OutgoingChallenge>(uri, 1, 0, "OutgoingChallenge", QStringLiteral("Use outgoingChallenges.create()"));

    // Services. The access token lives in Sailfish Secrets.
    AppSettings settings;
    SecretsTokenStore tokenStore;
    LichessApi api;
    Session session(&api, &tokenStore);
    EventStream events(&api);
    ChallengesModel challenges(&api, &events);
    OngoingGamesModel ongoingGames(&api, &events);
    FriendsModel friends(&api);
    OutgoingChallenges outgoingChallenges(&api, &events);
    AppActivation activation;
    Services::init(&api, &session, &settings);

    auto onLoginChanged = [&]() {
        if (session.loggedIn()) {
            events.start();
            challenges.refresh();
            ongoingGames.refresh();
            ongoingGames.setPolling(true);
        } else {
            events.stop();
            ongoingGames.setPolling(false);
            ongoingGames.clear();
            challenges.clear();
            friends.clear();
            outgoingChallenges.cancelAll();
        }
    };
    QObject::connect(&session, &Session::loggedInChanged, onLoginChanged);
    QObject::connect(&outgoingChallenges, &OutgoingChallenges::challengeFinished,
                     &challenges, &ChallengesModel::removeChallenge);

    if (!activation.registerOnBus())
        qWarning("Could not register D-Bus service %s", qPrintable(AppActivation::serviceName()));

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->engine()->addImageProvider(QStringLiteral("pieces"),
            new PieceImageProvider(SailfishApp::pathTo(QStringLiteral("qml/images/pieces")).toLocalFile()));

    QQmlContext *context = view->rootContext();
    context->setContextProperty(QStringLiteral("appSettings"), &settings);
    context->setContextProperty(QStringLiteral("session"), &session);
    context->setContextProperty(QStringLiteral("eventStream"), &events);
    context->setContextProperty(QStringLiteral("challenges"), &challenges);
    context->setContextProperty(QStringLiteral("ongoingGames"), &ongoingGames);
    context->setContextProperty(QStringLiteral("friends"), &friends);
    context->setContextProperty(QStringLiteral("outgoingChallenges"), &outgoingChallenges);
    context->setContextProperty(QStringLiteral("appActivation"), &activation);

    session.restore();

    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
