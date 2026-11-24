// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameSession.h"
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {
QDir directory(bool completed)
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                + (completed ? QStringLiteral("/history") : QStringLiteral("/unfinished")));
}
}

QString GameSession::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool GameSession::save(const QString &id, const ChessRecord &record, QString *error)
{
    const bool completed = record.outcome != 0;
    const QDir target = directory(completed);
    if (!QDir().mkpath(target.absolutePath())) {
        if (error) *error = QStringLiteral("无法创建对局存档目录");
        return false;
    }
    const QString name = id + QStringLiteral(".cchess");
    QLockFile lock(QDir(target.absoluteFilePath(QStringLiteral(".."))).filePath(id + QStringLiteral(".lock")));
    if (!lock.tryLock(100)) {
        if (error) *error = QStringLiteral("另一实例正在保存这盘棋");
        return false;
    }
    const QString archive = directory(true).filePath(name);
    if (QFile::exists(archive)) {
        ChessRecord previous;
        if (completed && ChessRecord::load(archive, previous, error) && previous.outcome == record.outcome
            && previous.redPlayerId == record.redPlayerId && previous.blackPlayerId == record.blackPlayerId
            && std::equal(previous.moves.begin(), previous.moves.end(), record.moves.begin(), record.moves.end(),
                [](const RecordedMove &a, const RecordedMove &b) {
                    return a.id == b.id && a.captured == b.captured && a.row == b.row && a.col == b.col;
                }))
            return true;
        if (error) *error = QStringLiteral("这盘棋已归档，无法覆盖原有结果");
        return false;
    }
    if (!record.save(target.filePath(name), error)) return false;
    if (completed) QFile::remove(directory(false).filePath(name));
    return true;
}

QStringList GameSession::files(bool completed)
{
    QStringList paths;
    const auto entries = directory(completed).entryInfoList({QStringLiteral("*.cchess")}, QDir::Files, QDir::Time);
    for (const auto &entry : entries) {
        if (!completed && QFile::exists(directory(true).filePath(entry.fileName()))) continue;
        paths.append(entry.absoluteFilePath());
    }
    return paths;
}

QString GameSession::latest(ChessRecord &record)
{
    for (const auto &path : files(false)) {
        if (ChessRecord::load(path, record) && record.outcome == 0)
            return QFileInfo(path).completeBaseName();
    }
    return {};
}
