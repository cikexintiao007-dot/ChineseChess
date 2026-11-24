// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlayerProfiles.h"
#include "GameSession.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
constexpr int profileLimit = 100;
constexpr int nicknameLimit = 24;

QString profilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(QStringLiteral("players.json"));
}

bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}

QString nameKey(const QString &name)
{
    return name.normalized(QString::NormalizationForm_KC).toCaseFolded();
}

bool validNickname(const QString &name)
{
    return !name.isEmpty() && name.size() <= nicknameLimit && name == name.trimmed()
        && name != QStringLiteral("访客") && !name.contains('<') && !name.contains('>')
        && std::all_of(name.begin(), name.end(), [](QChar ch) { return ch.isPrint(); });
}
}

QString PlayerProfile::displayName() const
{
    return QStringLiteral("%1 · %2").arg(avatar, name);
}

QStringList PlayerProfiles::avatars()
{
    return {QStringLiteral("帅"), QStringLiteral("将"), QStringLiteral("车"),
            QStringLiteral("马"), QStringLiteral("炮"), QStringLiteral("相")};
}

bool PlayerProfiles::load(QVector<PlayerProfile> &profiles, QString *error)
{
    QFile file(profilePath());
    if (!file.exists()) {
        profiles.clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    const QByteArray data = file.read(128 * 1024 + 1);
    const auto document = QJsonDocument::fromJson(data);
    const auto object = document.object();
    const auto rows = object.value(QStringLiteral("profiles"));
    if (data.size() > 128 * 1024 || object.value(QStringLiteral("version")).toInt() != 1 ||
        !rows.isArray() || rows.toArray().size() > profileLimit)
        return fail(error, QStringLiteral("玩家档案文件格式无效"));

    QVector<PlayerProfile> loaded;
    QSet<QString> ids;
    QSet<QString> names;
    const auto choices = avatars();
    for (const auto &value : rows.toArray()) {
        const auto row = value.toObject();
        PlayerProfile profile{row.value(QStringLiteral("id")).toString(), row.value(QStringLiteral("name")).toString(),
                              row.value(QStringLiteral("avatar")).toString()};
        const QUuid uuid(profile.id);
        const QString key = nameKey(profile.name);
        if (uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != profile.id
            || !validNickname(profile.name) || !choices.contains(profile.avatar)
            || ids.contains(profile.id) || names.contains(key))
            return fail(error, QStringLiteral("玩家档案内容无效"));
        ids.insert(profile.id);
        names.insert(key);
        loaded.append(profile);
    }
    profiles = std::move(loaded);
    return true;
}

bool PlayerProfiles::save(PlayerProfile &profile, QString *error)
{
    PlayerProfile candidate = profile;
    candidate.name = candidate.name.trimmed();
    if (!validNickname(candidate.name))
        return fail(error, QStringLiteral("请输入 1–24 个可显示字符，昵称不能是“访客”或含尖括号。"));
    if (!avatars().contains(candidate.avatar))
        return fail(error, QStringLiteral("请选择一个棋子头像。"));

    const QString path = profilePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail(error, QStringLiteral("无法创建玩家档案目录"));
    QLockFile lock(path + QStringLiteral(".lock"));
    if (!lock.tryLock(100))
        return fail(error, QStringLiteral("另一实例正在保存玩家档案，请重试。"));
    QVector<PlayerProfile> profiles;
    if (!load(profiles, error)) return false;

    const QString key = nameKey(candidate.name);
    const auto duplicate = std::find_if(profiles.begin(), profiles.end(), [&](const PlayerProfile &other) {
        return other.id != candidate.id && nameKey(other.name) == key;
    });
    if (duplicate != profiles.end()) return fail(error, QStringLiteral("这个昵称已被使用。"));
    if (candidate.id.isEmpty()) {
        if (profiles.size() >= profileLimit) return fail(error, QStringLiteral("最多可保存 100 个玩家档案。"));
        candidate.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        profiles.append(candidate);
    } else {
        const auto existing = std::find_if(profiles.begin(), profiles.end(), [&](const PlayerProfile &other) {
            return other.id == candidate.id;
        });
        if (existing == profiles.end()) return fail(error, QStringLiteral("未找到要编辑的玩家档案。"));
        *existing = candidate;
    }

    QJsonArray rows;
    for (const auto &entry : profiles)
        rows.append(QJsonObject{{"id", entry.id}, {"name", entry.name}, {"avatar", entry.avatar}});
    const QByteArray data = QJsonDocument(QJsonObject{{"version", 1}, {"profiles", rows}}).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return fail(error, file.errorString());
    profile = candidate;
    return true;
}

QVector<PlayerMatch> PlayerProfiles::ratedMatches(const QVector<PlayerProfile> &profiles)
{
    QVector<PlayerMatch> matches;
    if (profiles.size() < 2) return matches;
    QSet<QString> ids;
    for (const auto &profile : profiles) ids.insert(profile.id);
    for (const auto &path : GameSession::files(true)) {
        ChessRecord record;
        if (ChessRecord::load(path, record) && record.outcome != 0 && !record.computer && !record.moves.isEmpty()
            && ids.contains(record.redPlayerId) && ids.contains(record.blackPlayerId))
            matches.append({path, QFileInfo(path).lastModified(), record.redPlayerId, record.blackPlayerId, record.outcome});
    }
    return matches;
}

QVector<PlayerStanding> PlayerProfiles::standings(const QVector<PlayerProfile> &profiles, const QVector<PlayerMatch> &matches)
{
    QVector<PlayerStanding> results;
    QHash<QString, int> indexes;
    for (const auto &profile : profiles) {
        indexes.insert(profile.id, results.size());
        results.append({profile});
    }
    for (const auto &match : matches) {
        const int redIndex = indexes.value(match.redId, -1);
        const int blackIndex = indexes.value(match.blackId, -1);
        if (redIndex < 0 || blackIndex < 0 || redIndex == blackIndex) continue;
        auto &red = results[redIndex];
        auto &black = results[blackIndex];
        if (match.outcome == 1) {
            ++red.wins;
            ++black.losses;
        } else if (match.outcome == 2) {
            ++black.wins;
            ++red.losses;
        } else if (match.outcome == 3) {
            ++red.draws;
            ++black.draws;
        }
    }
    std::sort(results.begin(), results.end(), [](const PlayerStanding &a, const PlayerStanding &b) {
        if ((a.games() == 0) != (b.games() == 0)) return a.games() != 0;
        if (a.points() != b.points()) return a.points() > b.points();
        return a.profile.name < b.profile.name;
    });
    return results;
}
