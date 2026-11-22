// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GAMEPAGES_H
#define GAMEPAGES_H

#include "ChessRecord.h"
#include "MachineGame.h"
#include "PuzzleData.h"

class QWidget;

void rememberChessRecord(const QString &path);
bool chooseChessRecord(QWidget *parent, ChessRecord &record, QString *sessionId = nullptr);
bool configureMatch(QWidget *parent, bool computer, ChessRecord &record);
bool choosePuzzle(QWidget *parent, PuzzleConfig &puzzle, AIDifficulty &difficulty);
void showChessGuide(QWidget *parent);

#endif
