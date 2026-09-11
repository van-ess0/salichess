// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGSTOKENSTORE_H
#define SETTINGSTOKENSTORE_H

#include "tokenstore.h"

// Stores the token in a QSettings file inside the app's Sailjail-private
// config directory. The file is not encrypted.
class SettingsTokenStore : public TokenStore
{
public:
    QString load() override;
    bool save(const QString &token) override;
    void clear() override;

private:
    static QString filePath();
};

#endif // SETTINGSTOKENSTORE_H
