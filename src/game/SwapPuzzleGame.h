// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SWAPPUZZLEGAME_H
#define SWAPPUZZLEGAME_H

#include "PuzzleGame.h"
#include <optional>

class SwapPuzzleGame : public PuzzleGame
{
public:
    SwapPuzzleGame(const PuzzleConfig &puzzle, AIDifficulty difficulty);
    void init() override;

protected:
    void afterMove() override;
    void refreshPanel() override;
    void showGameResult(bool redWon) override;

private:
    void advance();
    void compareRounds(const ChessRecord &defense);

    const PuzzleConfig m_startingPuzzle;
    std::optional<ChessRecord> m_attack;
    QPushButton *m_advance = nullptr;
};

#endif
