// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GAMESESSION_H
#define GAMESESSION_H

#include "ChessRecord.h"
#include <QStringList>

namespace GameSession {
QString newId();
bool save(const QString &id, const ChessRecord &record, QString *error);
QStringList files(bool completed);
QString latest(ChessRecord &record);
}

#endif
