// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#include "MachineGame.h"
#include "ChessSearch.h"
#include "ChessRules.h"
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QCloseEvent>
#include <QDebug>
#include <climits>
#include <algorithm>

MachineGame::MachineGame(AIDifficulty diff)
    : m_difficulty(diff), m_bAiIsRed(false)
{
    connect(this, &ChessBoard::toMenu, this, &MachineGame::cancelSearch);
    setPlayerRoles(QStringLiteral("玩家"), QStringLiteral("电脑"));
    QString level = QStringLiteral("中等");
    if (diff == AIDifficulty::Easy) level = QStringLiteral("简单");
    if (diff == AIDifficulty::Hard) level = QStringLiteral("困难");
    setGameDescription(QStringLiteral("人机对弈 · %1").arg(level),
                       QStringLiteral("你执红棋 · 电脑执黑"));
}

MachineGame::~MachineGame()
{
    cancelSearch();
}

int MachineGame::getSearchDepth() const
{
    switch (m_difficulty) {
    case AIDifficulty::Easy:   return 1;
    case AIDifficulty::Medium: return 3;
    case AIDifficulty::Hard:   return 5;
    }
    return 3;
}

void MachineGame::saveStep(int selectID, int checkedID, int row, int col, QVector<ChessStep *> &steps)
{
    ChessStep* step = new ChessStep;
    step->m_nRowFrom = m_ChessPieces[selectID].m_nRow;
    step->m_nColFrom = m_ChessPieces[selectID].m_nCol;
    step->m_nRowTo = row;
    step->m_nnColTo = col;
    step->m_nMoveID = selectID;
    step->m_nKillID = checkedID;
    steps.append(step);
}

void MachineGame::getAllMoves(QVector<ChessStep *> &steps, bool forRed)
{
    int start = forRed ? 16 : 0;
    int end   = forRed ? 32 : 16;

    for (int id = start; id < end; id++) {
        if (m_ChessPieces[id].m_bDead)
            continue;

        for (int row = 0; row < 10; row++) {
            for (int col = 0; col < 9; col++) {
                int targetId = getStoneId(row, col);
                if (targetId != -1 && sameColor(id, targetId))
                    continue;
                int killId = (targetId != -1) ? targetId : -1;
                if (canMove(id, killId, row, col))
                    saveStep(id, killId, row, col, steps);
            }
        }
    }
}

void MachineGame::mouseReleaseEvent(QMouseEvent *ev)
{
    if (m_searchCanceled || m_bIsRed == m_bAiIsRed)
        return;
    const bool wasRedTurn = m_bIsRed;
    ChessBoard::mouseReleaseEvent(ev);

    // AI 执黑时：红方刚走完（wasRedTurn=true, 现在m_bIsRed=false）触发AI
    // AI 执红时：黑方刚走完（wasRedTurn=false, 现在m_bIsRed=true）触发AI
    if (!m_bIsOver && wasRedTurn != m_bIsRed && m_bIsRed == m_bAiIsRed) {
        machineChooseAndMovePieces();
    }
}

void MachineGame::fakeMove(ChessStep *step)
{
    if (step->m_nKillID != -1)
        m_ChessPieces[step->m_nKillID].m_bDead = true;

    m_ChessPieces[step->m_nMoveID].m_nRow = step->m_nRowTo;
    m_ChessPieces[step->m_nMoveID].m_nCol = step->m_nnColTo;
    m_bIsRed = !m_bIsRed;
}

void MachineGame::unFakeMove(ChessStep *step)
{
    if (step->m_nKillID != -1)
        m_ChessPieces[step->m_nKillID].m_bDead = false;

    m_ChessPieces[step->m_nMoveID].m_nRow = step->m_nRowFrom;
    m_ChessPieces[step->m_nMoveID].m_nCol = step->m_nColFrom;
    m_bIsRed = !m_bIsRed;
}

int MachineGame::calcScore()
{
    return ChessSearch::evaluate(m_ChessPieces, m_bAiIsRed,
                                 m_difficulty >= AIDifficulty::Medium);
}

int MachineGame::alphaBeta(int depth, int alpha, int beta, bool isMaximizing)
{
    // 检查将帅是否已死
    if (m_ChessPieces[4].m_bDead)  // 黑将死
        return m_bAiIsRed ? 100000 + depth : -(100000 + depth);
    if (m_ChessPieces[20].m_bDead) // 红帅死
        return m_bAiIsRed ? -(100000 + depth) : 100000 + depth;

    if (depth == 0) {
        if (!ChessRules(m_ChessPieces).hasLegalMove(m_bIsRed))
            return isMaximizing ? -(99999 + depth) : 99999 + depth;
        return calcScore();
    }

    QVector<ChessStep*> steps;
    bool forRed = m_bIsRed;
    getAllMoves(steps, forRed);

    if (steps.isEmpty()) {
        return isMaximizing ? -(99999 + depth) : (99999 + depth);
    }

    // 走法排序：吃子走法优先
    std::sort(steps.begin(), steps.end(), [](ChessStep* a, ChessStep* b) {
        return (a->m_nKillID != -1) > (b->m_nKillID != -1);
    });

    int bestVal = isMaximizing ? INT_MIN : INT_MAX;

    for (auto* step : steps) {
        fakeMove(step);
        int val = alphaBeta(depth - 1, alpha, beta, !isMaximizing);
        unFakeMove(step);

        if (isMaximizing) {
            if (val > bestVal) bestVal = val;
            if (val > alpha) alpha = val;
        } else {
            if (val < bestVal) bestVal = val;
            if (val < beta) beta = val;
        }
        if (alpha >= beta)
            break;
    }

    for (auto* s : steps) delete s;
    return bestVal;
}

ChessStep* MachineGame::getBestMove()
{
    int depth = getSearchDepth();
    QVector<ChessStep*> steps;
    bool aiIsRedTurn = m_bIsRed;
    getAllMoves(steps, aiIsRedTurn);

    if (steps.isEmpty()) {
        whoWin();
        return nullptr;
    }

    // 走法排序：吃子走法优先
    std::sort(steps.begin(), steps.end(), [](ChessStep* a, ChessStep* b) {
        return (a->m_nKillID != -1) > (b->m_nKillID != -1);
    });

    int bestScore = INT_MIN;
    ChessStep* bestStep = nullptr;

    for (auto* step : steps) {
        fakeMove(step);
        int score = alphaBeta(depth - 1, INT_MIN, INT_MAX, false);
        unFakeMove(step);

        if (score > bestScore) {
            bestScore = score;
            bestStep = step;
        }
    }

    // 清理未使用的 step
    for (auto* s : steps) {
        if (s != bestStep) delete s;
    }

    if (!bestStep)
        whoWin();

    return bestStep;
}

void MachineGame::machineChooseAndMovePieces()
{
    if (m_searchCanceled || m_bIsOver || m_bIsRed != m_bAiIsRed)
        return;

    ChessSearch::Position position;
    std::copy(std::begin(m_ChessPieces), std::end(m_ChessPieces), position.begin());
    const bool redTurn = m_bIsRed;
    const bool aiRed = m_bAiIsRed;
    const bool usePositionScores = m_difficulty >= AIDifficulty::Medium;
    const int depth = getSearchDepth();
    const int timeLimitMs = m_difficulty == AIDifficulty::Hard ? 2000 : 700;
    const int historySize = m_ChessSteps.size();
    const auto canceled = std::make_shared<std::atomic_bool>(false);
    m_searchCanceled = canceled;
    setAiThinking(true);

    auto* watcher = new QFutureWatcher<SearchResult>(this);
    connect(watcher, &QFutureWatcher<SearchResult>::finished, this,
            [this, watcher, canceled, position, redTurn, aiRed, historySize]() {
        const SearchResult result = watcher->result();
        watcher->deleteLater();
        const auto applyResult = [this, canceled, position, redTurn, aiRed, historySize, result]() {
            if (canceled->load() || m_searchCanceled != canceled)
                return;
            m_searchCanceled.reset();
            setAiThinking(false);
            if (m_bIsOver || m_bIsRed != redTurn || m_bAiIsRed != aiRed ||
                m_ChessSteps.size() != historySize)
                return;
            // Reject results if a caller has replaced or edited the board meanwhile.
            for (int id = 0; id < 32; ++id) {
                const auto& before = position[id];
                const auto& now = m_ChessPieces[id];
                if (before.m_nRow != now.m_nRow || before.m_nCol != now.m_nCol ||
                    before.m_bDead != now.m_bDead || before.m_bRed != now.m_bRed ||
                    before.m_emType != now.m_emType)
                    return;
            }
            const auto& move = result.move;
            if (move.moveId == -1) {
                whoWin();
                return;
            }
            qDebug() << "AI search" << result.elapsedMs << "ms, depth"
                     << result.completedDepth << ", nodes" << result.nodes << ", cache hits" << result.cacheHits;
            doMoveStone(move.moveId, move.killId, move.rowTo, move.colTo);
            m_nSelectID = -1;
            update();
        };
        runAfterMoveAnimation(applyResult);
    });
    // The worker owns its entire position and never accesses a QWidget.
    watcher->setFuture(QtConcurrent::run([position, redTurn, aiRed, usePositionScores,
                                         canceled, depth, timeLimitMs]() {
        ChessSearch search(position, redTurn, aiRed, usePositionScores, canceled);
        return search.bestMove(depth, timeLimitMs);
    }));
}

void MachineGame::cancelSearch()
{
    if (m_searchCanceled) {
        m_searchCanceled->store(true);
        m_searchCanceled.reset();
        setAiThinking(false);
    }
}

bool MachineGame::resumeRecord(const ChessRecord &record, QString *error)
{
    if (!record.validate(error)) return false;
    cancelSearch();
    m_difficulty = static_cast<AIDifficulty>(record.difficulty);
    m_bAiIsRed = record.aiRed;
    if (!ChessBoard::resumeRecord(record, error)) return false;
    machineChooseAndMovePieces();
    return true;
}

void MachineGame::init()
{
    cancelSearch();
    ChessBoard::init();
    QTimer::singleShot(0, this, [this]() { machineChooseAndMovePieces(); });
}

void MachineGame::back()
{
    if (hasMatchClock()) return;
    cancelSearch();
    ChessBoard::back();
    // Restore the human's turn when undoing a completed human/AI exchange.
    if (m_bIsRed == m_bAiIsRed && !m_ChessSteps.isEmpty())
        ChessBoard::back();
    machineChooseAndMovePieces();
}

void MachineGame::closeEvent(QCloseEvent* event)
{
    ChessBoard::closeEvent(event);
    if (event->isAccepted()) cancelSearch();
}
