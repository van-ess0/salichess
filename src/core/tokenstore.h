// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOKENSTORE_H
#define TOKENSTORE_H

#include <QString>

// Persists the Lichess OAuth access token. Session only talks to this
// interface, so the storage backend (SecretsTokenStore on the device, an
// in-memory one in tests) is chosen without touching anything else.
class TokenStore
{
public:
    virtual ~TokenStore() = default;

    virtual QString load() = 0;
    virtual bool save(const QString &token) = 0;
    virtual void clear() = 0;
};

#endif // TOKENSTORE_H
