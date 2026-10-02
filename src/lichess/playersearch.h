// SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PLAYERSEARCH_H
#define PLAYERSEARCH_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

// Looks players up by the start of their name (/api/player/autocomplete) as
// the user types. Created in QML:
//     PlayerSearch { term: field.text }
class PlayerSearch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString term READ term WRITE setTerm NOTIFY termChanged)
    // Maps of "id", "name", "title", "online" and "patron".
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    explicit PlayerSearch(QObject *parent = nullptr);

    QString term() const { return m_term; }
    void setTerm(const QString &term);
    QVariantList results() const { return m_results; }
    bool loading() const { return m_loading; }
    QString errorString() const { return m_error; }

signals:
    void termChanged();
    void resultsChanged();
    void loadingChanged();
    void errorStringChanged();

private:
    void search();
    void setLoading(bool loading);

    QString m_term;
    QVariantList m_results;
    bool m_loading = false;
    QString m_error;
    int m_generation = 0;
    QTimer m_debounce;
};

#endif // PLAYERSEARCH_H
