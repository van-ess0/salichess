// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATMODEL_H
#define CHATMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

// Chat lines of a game.
class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        UsernameRole = Qt::UserRole + 1,
        TextRole,
        MineRole,
        SystemRole, // messages from "lichess" (e.g. "White offers draw")
        TimeRole    // when the line arrived; undefined for chat history
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_lines.size(); }

    void setMyName(const QString &name) { m_myName = name; }
    // Lichess sends no timestamps, so live lines carry their arrival time
    // and lines from the history none.
    void append(const QString &username, const QString &text, const QDateTime &time = QDateTime());
    void clear();

signals:
    void countChanged();

private:
    struct Line {
        QString username;
        QString text;
        QDateTime time;
    };
    QVector<Line> m_lines;
    QString m_myName;
};

#endif // CHATMODEL_H
