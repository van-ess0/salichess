// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logging.h"

Q_LOGGING_CATEGORY(lcApi, "salichess.api")
Q_LOGGING_CATEGORY(lcSession, "salichess.session")
Q_LOGGING_CATEGORY(lcGame, "salichess.game")
Q_LOGGING_CATEGORY(lcPuzzle, "salichess.puzzle")
Q_LOGGING_CATEGORY(lcEngine, "salichess.engine")

namespace Logging {

void setLevel(Level level)
{
    // Qt enables everything but debug by default, so this only has to switch
    // things off. The app's own categories first; at ERROR the warnings of
    // everything else (Qt, QML) go too.
    QString rules;
    if (level > Debug)
        rules += QStringLiteral("salichess.*.debug=false\n");
    if (level > Info)
        rules += QStringLiteral("salichess.*.info=false\n");
    if (level > Warning)
        rules += QStringLiteral("*.warning=false\n");
    QLoggingCategory::setFilterRules(rules);

    // Time and area in front of each message; QT_MESSAGE_PATTERN still wins.
    qSetMessagePattern(QStringLiteral("%{time hh:mm:ss.zzz} %{category} %{message}"));
}

bool configure(const QStringList &arguments)
{
    QString value;
    bool valueIsNext = false; // after a bare --log-level
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        if (valueIsNext) {
            value = argument;
            valueIsNext = false;
        } else if (argument == QLatin1String("--log-level")) {
            valueIsNext = true;
        } else if (argument.startsWith(QLatin1String("--log-level="))) {
            value = argument.mid(int(qstrlen("--log-level=")));
        } else if (argument.startsWith(QLatin1String("log_level="))) {
            value = argument.mid(int(qstrlen("log_level=")));
        }
    }

    Level level = Warning;
    bool known = true;
    if (!value.isEmpty()) {
        const QString name = value.trimmed().toUpper();
        if (name == QLatin1String("DEBUG"))
            level = Debug;
        else if (name == QLatin1String("INFO"))
            level = Info;
        else if (name == QLatin1String("WARNING") || name == QLatin1String("WARN"))
            level = Warning;
        else if (name == QLatin1String("ERROR"))
            level = Error;
        else
            known = false;
    }
    setLevel(level);
    if (!known)
        qWarning("Unknown log level \"%s\" (use DEBUG, INFO, WARNING or ERROR)", qPrintable(value));
    return known;
}

}
