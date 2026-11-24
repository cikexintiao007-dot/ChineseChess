// SPDX-License-Identifier: GPL-3.0-or-later
#include "SwapPuzzleGame.h"
#include "ReplayGame.h"
#include "ui_ChessBoard.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int attackLimit = 3;
constexpr int defenseLimit = 2;
}

SwapPuzzleGame::SwapPuzzleGame(const PuzzleConfig &puzzle, AIDifficulty difficulty)
    : PuzzleGame(puzzle, difficulty), m_startingPuzzle(puzzle)
{
    setWindowTitle(QStringLiteral("攻守互换 · %1").arg(puzzle.name));
    for (const auto *name : {"pushButton_back", "drawButton"})
        findChild<QPushButton *>(QString::fromLatin1(name))->hide();
    ui->pushButton_restart->setText(QStringLiteral("重开本轮"));
    m_advance = new QPushButton(this);
    m_advance->setObjectName(QStringLiteral("swapAdvanceButton"));
    m_advance->setCursor(Qt::PointingHandCursor);
    ui->controlsLayout->insertWidget(0, m_advance);
    connect(m_advance, &QPushButton::clicked, this, &SwapPuzzleGame::advance);
    init();
}

void SwapPuzzleGame::init()
{
    const bool defending = m_attack.has_value();
    PuzzleConfig round = m_startingPuzzle;
    round.playerIsRed = !defending;
    round.description = defending
        ? QStringLiteral("第 2 / 2 轮 · 执黑守住红方前 %1 手。").arg(defenseLimit)
        : QStringLiteral("第 1 / 2 轮 · 执红在 %1 手内取胜。").arg(attackLimit);
    m_advance->setText(defending ? QStringLiteral("对照两轮回放") : QStringLiteral("换黑方防守"));
    m_advance->hide();
    loadPuzzle(round);
    setGameDescription(QStringLiteral("攻守互换 · %1").arg(round.name),
        defending ? QStringLiteral("你执黑 · 电脑执红") : QStringLiteral("你执红 · 电脑执黑"),
        round.description);
}

void SwapPuzzleGame::afterMove()
{
    const bool defending = m_attack.has_value();
    const int limit = defending ? defenseLimit : attackLimit;
    if (m_ChessSteps.size() == 2 * limit - 1)
        finishGame(Outcome::Draw, defending ? QStringLiteral("黑方守住了红方前两手")
                                            : QStringLiteral("红方三手内未取胜"));
}

void SwapPuzzleGame::refreshPanel()
{
    ChessBoard::refreshPanel();
    if (!m_advance) return;
    const bool defending = m_attack.has_value();
    const bool achieved = defending ? outcome() != Outcome::RedWon : outcome() == Outcome::RedWon;
    const int redMoves = (m_ChessSteps.size() + 1) / 2;
    ui->headerState->setText(m_bIsOver ? (achieved ? QStringLiteral("目标达成") : QStringLiteral("目标未达成"))
        : QStringLiteral("红方已走 %1 / %2 手").arg(redMoves).arg(defending ? defenseLimit : attackLimit));
    if (m_bIsOver) {
        if (outcome() == Outcome::Draw) ui->turnLabel->setText(QStringLiteral("轮次结束"));
        ui->labelHint->setText(resultReason());
    }
}

void SwapPuzzleGame::showGameResult(bool)
{
    m_advance->show();
    m_advance->setFocus();
}

void SwapPuzzleGame::advance()
{
    if (!m_bIsOver || !m_advance->isVisible()) return;
    if (m_attack) {
        compareRounds(gameRecord());
    } else {
        m_attack = gameRecord();
        init();
    }
}

void SwapPuzzleGame::compareRounds(const ChessRecord &defense)
{
    QDialog page(this);
    page.setObjectName(QStringLiteral("swapComparison"));
    page.setWindowTitle(QStringLiteral("攻守对照 · %1").arg(m_startingPuzzle.name));
    page.resize(800, 500);
    auto *layout = new QVBoxLayout(&page);
    auto *boards = new QHBoxLayout;
    boards->setSpacing(16);
    const auto addRound = [&](const ChessRecord &record, const QString &title, bool achieved) {
        auto *column = new QVBoxLayout;
        auto *heading = new QLabel(QStringLiteral("%1 · %2").arg(title, achieved
            ? QStringLiteral("达成") : QStringLiteral("未达成")), &page);
        heading->setAlignment(Qt::AlignCenter);
        column->addWidget(heading);
        auto *replay = new ReplayGame(record, &page);
        replay->setAccessibleName(title);
        column->addWidget(replay, 1);
        boards->addLayout(column, 1);
    };
    addRound(*m_attack, QStringLiteral("执红进攻"), m_attack->outcome == int(Outcome::RedWon));
    addRound(defense, QStringLiteral("执黑防守"), defense.outcome != int(Outcome::RedWon));
    layout->addLayout(boards, 1);
    auto *buttons = new QDialogButtonBox(&page);
    auto *close = buttons->addButton(QStringLiteral("关闭对照"), QDialogButtonBox::RejectRole);
    connect(close, &QPushButton::clicked, &page, &QDialog::reject);
    layout->addWidget(buttons);
    page.exec();
}
