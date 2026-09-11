// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "outgoingchallenges.h"

#include "outgoingchallenge.h"

#include <QQmlEngine>

namespace {

bool isPending(const OutgoingChallenge *challenge)
{
    return challenge->state() == OutgoingChallenge::Creating
            || challenge->state() == OutgoingChallenge::Waiting;
}

} // namespace

OutgoingChallenges::OutgoingChallenges(LichessApi *api, EventStream *events, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_events(events)
{
}

int OutgoingChallenges::pendingCount() const
{
    int n = 0;
    for (const OutgoingChallenge *challenge : m_challenges) {
        if (isPending(challenge))
            ++n;
    }
    return n;
}

OutgoingChallenge *OutgoingChallenges::create(const QString &username, const QVariantMap &options)
{
    prune();
    OutgoingChallenge *challenge = new OutgoingChallenge(m_api, m_events, this);
    // Returned to QML, but owned (and deleted) here.
    QQmlEngine::setObjectOwnership(challenge, QQmlEngine::CppOwnership);
    m_challenges.append(challenge);
    connect(challenge, &OutgoingChallenge::stateChanged, this, [this, challenge]() { onStateChanged(challenge); });
    challenge->create(username, options);
    emit pendingCountChanged();
    return challenge;
}

OutgoingChallenge *OutgoingChallenges::find(const QString &challengeId) const
{
    for (OutgoingChallenge *challenge : m_challenges) {
        if (!challengeId.isEmpty() && challenge->challengeId() == challengeId && isPending(challenge))
            return challenge;
    }
    return nullptr;
}

void OutgoingChallenges::cancelAll()
{
    for (OutgoingChallenge *challenge : m_challenges) {
        if (isPending(challenge))
            challenge->cancel();
    }
}

void OutgoingChallenges::onStateChanged(OutgoingChallenge *challenge)
{
    switch (challenge->state()) {
    case OutgoingChallenge::Accepted:
        emit challengeAccepted(challenge->challengeId(), challenge->opponent());
        break;
    case OutgoingChallenge::Declined:
        emit challengeDeclined(challenge->challengeId(), challenge->opponent(), challenge->declineReason());
        break;
    case OutgoingChallenge::Failed:
        emit challengeFailed(challenge->opponent(), challenge->errorString());
        break;
    case OutgoingChallenge::Idle:
    case OutgoingChallenge::Creating:
    case OutgoingChallenge::Waiting:
    case OutgoingChallenge::Canceled:
        break;
    }
    if (!isPending(challenge) && !challenge->challengeId().isEmpty())
        emit challengeFinished(challenge->challengeId());
    emit pendingCountChanged();
}

void OutgoingChallenges::prune()
{
    // Finished challenges are kept until the next one is created, so a
    // waiting page can still show how its challenge ended.
    for (int i = m_challenges.size() - 1; i >= 0; --i) {
        if (!isPending(m_challenges.at(i)))
            m_challenges.takeAt(i)->deleteLater();
    }
}
