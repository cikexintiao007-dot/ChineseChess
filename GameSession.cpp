// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameSession.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>

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
