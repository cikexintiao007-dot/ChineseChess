// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#ifndef MACHINEGAME_H
#define MACHINEGAME_H

#include "ui/ChessBoard.h"
#include "core/ChessStep.h"
#include <QVector>
#include <atomic>
#include <memory>

enum class AIDifficulty { Easy = 0, Medium = 1, Hard = 2 };

class MachineGame : public ChessBoard
{
public:
    MachineGame(AIDifficulty diff = AIDifficulty::Medium);
    ~MachineGame();

    void saveStep(int moveID, int checkedID, int row, int col, QVector<ChessStep*>& steps);
    void getAllMoves(QVector<ChessStep*>& steps, bool forRed);

    void mouseReleaseEvent(QMouseEvent *ev) override;
    void init() override;
    void back() override;

    void fakeMove(ChessStep* step);
    void unFakeMove(ChessStep* step);
    int calcScore();
    int alphaBeta(int depth, int alpha, int beta, bool isMaximizing);
    ChessStep* getBestMove();
    void machineChooseAndMovePieces();

    int getSearchDepth() const;
    AIDifficulty difficulty() const { return m_difficulty; }
    bool resumeRecord(const ChessRecord &record, QString *error = nullptr) override;
    bool isAiRed() const { return m_bAiIsRed; }
    void setAiIsRed(bool red) { m_bAiIsRed = red; }

protected:
    bool humanIsRed() const override { return !m_bAiIsRed; }
    void cancelSearch();
    void closeEvent(QCloseEvent* event) override;
    AIDifficulty m_difficulty;
    bool m_bAiIsRed = false;  // AI 默认执黑

private:
    std::shared_ptr<std::atomic_bool> m_searchCanceled;
};

#endif // MACHINEGAME_H
