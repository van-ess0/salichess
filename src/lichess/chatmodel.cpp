// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatmodel.h"

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return QVariant();
    const Line &line = m_lines.at(index.row());
    switch (role) {
    case UsernameRole: return line.username;
    case TextRole: return line.text;
    case MineRole: return !m_myName.isEmpty() && line.username.compare(m_myName, Qt::CaseInsensitive) == 0;
    case SystemRole: return line.username.compare(QLatin1String("lichess"), Qt::CaseInsensitive) == 0;
    case TimeRole: return line.time.isValid() ? QVariant(line.time) : QVariant();
    default: return QVariant();
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        { UsernameRole, "username" },
        { TextRole, "text" },
        { MineRole, "mine" },
        { SystemRole, "system" },
        { TimeRole, "time" },
    };
}

void ChatModel::append(const QString &username, const QString &text, const QDateTime &time)
{
    beginInsertRows(QModelIndex(), m_lines.size(), m_lines.size());
    m_lines.append(Line { username, text, time });
    endInsertRows();
    emit countChanged();
}

void ChatModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();
    emit countChanged();
}
