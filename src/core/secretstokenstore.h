// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SECRETSTOKENSTORE_H
#define SECRETSTOKENSTORE_H

#include "tokenstore.h"

#include <QScopedPointer>

namespace Sailfish { namespace Secrets { class SecretManager; } }

// Keeps the token in Sailfish OS Secrets: an owner-only collection in the
// default encrypted storage plugin, unlocked together with the device.
// Needs the Sailjail permission "Secrets".
//
// Requests are made synchronously (waitForFinished()); they are quick and
// only happen at startup, login and logout.
class SecretsTokenStore : public TokenStore
{
public:
    SecretsTokenStore();
    ~SecretsTokenStore() override;

    QString load() override;
    bool save(const QString &token) override;
    void clear() override;

private:
    bool ensureCollection();

    QScopedPointer<Sailfish::Secrets::SecretManager> m_manager;
};

#endif // SECRETSTOKENSTORE_H
