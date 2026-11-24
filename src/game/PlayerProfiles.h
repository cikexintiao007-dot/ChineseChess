// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PLAYERPROFILES_H
#define PLAYERPROFILES_H

#include <QDateTime>
#include <QStringList>
#include <QVector>

struct PlayerProfile
{
    QString id;
    QString name;
    QString avatar;
    QString displayName() const;
};

struct PlayerMatch
{
    QString path;
    QDateTime finishedAt;
    QString redId;
    QString blackId;
    int outcome;
};

struct PlayerStanding
{
    PlayerProfile profile;
    int wins = 0;
    int draws = 0;
    int losses = 0;
    int points() const { return 3 * wins + draws; }
    int games() const { return wins + draws + losses; }
};

namespace PlayerProfiles {
QStringList avatars();
bool load(QVector<PlayerProfile> &profiles, QString *error = nullptr);
bool save(PlayerProfile &profile, QString *error = nullptr);
QVector<PlayerMatch> ratedMatches(const QVector<PlayerProfile> &profiles);
QVector<PlayerStanding> standings(const QVector<PlayerProfile> &profiles, const QVector<PlayerMatch> &matches);
}

#endif
