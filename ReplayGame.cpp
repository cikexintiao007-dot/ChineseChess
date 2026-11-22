// SPDX-License-Identifier: GPL-3.0-or-later
#include "ReplayGame.h"
#include "MachineGame.h"
#include "ui_ChessBoard.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QMessageBox>

ReplayGame::ReplayGame(const ChessRecord &record) : m_record(record)
{
    setWindowTitle(QStringLiteral("棋谱复盘"));
    setPlayerRoles(record.redRole, record.blackRole);
    setGameDescription(QStringLiteral("棋谱复盘"), record.mode, record.description);
    for (const auto &name : {"pushButton_start", "pushButton_back", "pushButton_restart",
                             "resignButton", "drawButton", "reviewRecordButton"})
        findChild<QPushButton *>(QString::fromLatin1(name))->hide();
    ui->checkAnimations->hide();
    auto *navigation = new QHBoxLayout;
    navigation->setContentsMargins(20, 0, 20, 12);
    navigation->setSpacing(8);
    m_first = new QPushButton(QStringLiteral("回到开局"), this);
    m_previous = new QPushButton(QStringLiteral("上一步"), this);
    m_next = new QPushButton(QStringLiteral("下一步"), this);
    m_last = new QPushButton(QStringLiteral("最后一步"), this);
    m_continue = new QPushButton(QStringLiteral("从此处续局"), this);
    m_continue->setObjectName(QStringLiteral("replayContinueButton"));
    m_continue->setToolTip(QStringLiteral("人机棋谱沿用原难度和执棋方，联机棋谱转为本地双人。中途续局重新计时。"));
    m_first->setObjectName(QStringLiteral("replayFirstButton"));
    m_previous->setObjectName(QStringLiteral("replayPreviousButton"));
    m_next->setObjectName(QStringLiteral("replayNextButton"));
    m_last->setObjectName(QStringLiteral("replayLastButton"));
    for (auto *button : {m_first, m_previous, m_next, m_last, m_continue})
        button->setCursor(Qt::PointingHandCursor);
    navigation->addWidget(m_first);
    navigation->addWidget(m_previous);
    navigation->addWidget(m_next);
    navigation->addWidget(m_last);
    ui->outerLayout->addLayout(navigation);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("replaySlider"));
    m_slider->setAccessibleName(QStringLiteral("复盘步数"));
    m_slider->setRange(0, record.moves.size());
    ui->controlsLayout->insertWidget(0, m_slider);
    ui->controlsLayout->insertWidget(1, m_continue);
    connect(m_first, &QPushButton::clicked, this, [this] { seek(0); });
    connect(m_previous, &QPushButton::clicked, this, [this] { seek(m_step - 1); });
    connect(m_next, &QPushButton::clicked, this, [this] { seek(m_step + 1); });
    connect(m_last, &QPushButton::clicked, this, [this] { seek(m_record.moves.size()); });
    connect(m_slider, &QSlider::valueChanged, this, &ReplayGame::seek);
    connect(m_continue, &QPushButton::clicked, this, &ReplayGame::continueGame);
    for (const auto &entry : {std::pair<int, QPushButton *>(Qt::Key_Left, m_previous),
                             {Qt::Key_Right, m_next}, {Qt::Key_Home, m_first}, {Qt::Key_End, m_last}}) {
        auto *shortcut = new QShortcut(QKeySequence(entry.first), this);
        connect(shortcut, &QShortcut::activated, entry.second, &QPushButton::click);
    }
    m_ready = true;
    seek(record.moves.size());
}

void ReplayGame::seek(int step)
{
    m_step = qBound(0, step, int(m_record.moves.size()));
    applyRecordPosition(m_record, m_step, true);
    setElapsedSeconds(m_record.seconds, false);
}

void ReplayGame::init()
{
    if (m_ready) seek(0);
    else ChessBoard::init();
}

void ReplayGame::refreshPanel()
{
    ChessBoard::refreshPanel();
    if (!m_ready) return;
    ui->headerState->setText(QStringLiteral("复盘中"));
    ui->labelHint->setText(QStringLiteral("第 %1 / %2 步 · 方向键切换").arg(m_step).arg(m_record.moves.size()));
    m_first->setEnabled(m_step > 0);
    m_previous->setEnabled(m_step > 0);
    m_next->setEnabled(m_step < m_record.moves.size());
    m_last->setEnabled(m_step < m_record.moves.size());
    m_continue->setEnabled(m_step < m_record.moves.size() || m_record.outcome == 0);
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(m_step);
}

void ReplayGame::continueGame()
{
    ChessRecord branch = m_record;
    branch.moves.resize(m_step);
    branch.seconds = m_step == m_record.moves.size() ? m_record.seconds : 0;
    branch.outcome = 0;
    branch.reason.clear();
    if (m_step != m_record.moves.size())
        branch.redRemainingMs = branch.blackRemainingMs = branch.clockLimitMs;
    ChessBoard *game = branch.computer
        ? new MachineGame(static_cast<AIDifficulty>(branch.difficulty)) : new ChessBoard;
    QString error;
    if (!game->resumeRecord(branch, &error)) {
        delete game;
        QMessageBox::warning(this, QStringLiteral("无法续局"), error);
        return;
    }
    game->setWindowTitle(QStringLiteral("棋谱续局"));
    game->setAttribute(Qt::WA_DeleteOnClose);
    connect(game, &ChessBoard::toMenu, game, &QWidget::close);
    connect(game, &QObject::destroyed, this, [this] { show(); });
    hide();
    game->show();
}
