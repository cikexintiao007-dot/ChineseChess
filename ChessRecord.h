// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CHESSRECORD_H
#define CHESSRECORD_H

#include "ChessPieces.h"
#include "ChessStep.h"
#include <array>
#include <QVector>

struct ChessRecord
{
    ChessRecord();
    std::array<ChessPieces, 32> initial;
    QVector<RecordedMove> moves;
    bool redFirst = true;
    bool flipped = false;
    bool computer = false;
    bool aiRed = false;
    int difficulty = 1;
    int seconds = 0;
    int clockLimitMs = 0;
    int redRemainingMs = 0;
    int blackRemainingMs = 0;
    int outcome = 0;
    QString reason;
    QString mode;
    QString redRole;
    QString blackRole;
    QString description;

    bool validate(QString *error = nullptr) const;
    bool save(const QString &path, QString *error = nullptr) const;
    static bool load(const QString &path, ChessRecord &record, QString *error = nullptr);
};

#endif
