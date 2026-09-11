// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "services.h"

namespace {
LichessApi *s_api = nullptr;
Session *s_session = nullptr;
AppSettings *s_settings = nullptr;
}

namespace Services {

void init(LichessApi *api, Session *session, AppSettings *settings)
{
    s_api = api;
    s_session = session;
    s_settings = settings;
}

LichessApi *api()
{
    return s_api;
}

Session *session()
{
    return s_session;
}

AppSettings *settings()
{
    return s_settings;
}

} // namespace Services
