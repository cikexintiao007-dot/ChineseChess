// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H
#include <QDialog>

struct GamePreferences
{
    bool sound = true;
    int volume = 70;
    bool animations = true;
    int animationPercent = 100;
    bool traces = true;
    bool legalMoves = true;
    bool confirmExit = true;
    static GamePreferences load();
    void save() const;
};

class GameSettings : public QDialog
{
  public:
    explicit GameSettings(QWidget *parent = nullptr);
};
#endif
