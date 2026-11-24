// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#include "ChooseMainWindow.h"
#include "GameTheme.h"
#include "LanRoomDialog.h"
#include "network/NetworkGame.h"
#include "app/GameSettings.h"
#include "game/ReplayGame.h"
#include "game/SwapPuzzleGame.h"
#include "GamePages.h"
#include "PlayerPage.h"
#include "PositionEditor.h"
#include "game/GameSession.h"
#include <QPainter>
#include <QMessageBox>

#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QSizePolicy>

ChooseMainWindow::ChooseMainWindow(QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("gameMenu"));
    setWindowTitle(QStringLiteral("中国象棋"));
    setMinimumSize(420, 546);
    const QSize preferredSize(620, 680);
    const auto *display = screen();
    const QSize availableSize = display ? display->availableGeometry().size() - QSize(32, 56)
                                        : preferredSize;
    resize(preferredSize.boundedTo(availableSize).expandedTo(minimumSize()));
    setWindowIcon(QIcon(":/images/chess.svg"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(24);

    auto* title = new QLabel(QStringLiteral("中国象棋"), this);
    title->setObjectName(QStringLiteral("menuTitle"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(12);
    const QStringList texts = {
        QStringLiteral("本地双人对弈"),
        QStringLiteral("人机对弈"),
        QStringLiteral("残局挑战"),
        QStringLiteral("局域网联机")
    };

    for (int i = 0; i < texts.size(); ++i) {
        m_buttons[i] = new QPushButton(texts[i], this);
        m_buttons[i]->setMinimumHeight(64);
        m_buttons[i]->setFocusPolicy(Qt::StrongFocus);
        m_buttons[i]->setCursor(Qt::PointingHandCursor);
        m_buttons[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        buttonLayout->addWidget(m_buttons[i], 1);
    }
    layout->addLayout(buttonLayout, 1);
    m_continueButton = new QPushButton(QStringLiteral("继续上局"), this);
    m_continueButton->setObjectName(QStringLiteral("menuContinueButton"));
    m_continueButton->setCursor(Qt::PointingHandCursor);
    auto *secondaryActions = new QHBoxLayout;
    auto *swap = new QPushButton(QStringLiteral("攻守互换残局"), this);
    swap->setObjectName(QStringLiteral("menuSwapPuzzleButton"));
    swap->setCursor(Qt::PointingHandCursor);
    secondaryActions->addWidget(m_continueButton);
    secondaryActions->addWidget(swap);
    auto *profiles = new QPushButton(QStringLiteral("玩家与排行"), this);
    profiles->setObjectName(QStringLiteral("menuPlayersButton"));
    profiles->setCursor(Qt::PointingHandCursor);
    secondaryActions->addWidget(profiles);
    layout->addLayout(secondaryActions);
    connect(profiles, &QPushButton::clicked, this, [this] {
        ChessRecord record;
        if (choosePlayerReplay(this, record)) openGame(new ReplayGame(record));
    });
    connect(swap, &QPushButton::clicked, this, [this] {
        PuzzleConfig puzzle;
        AIDifficulty difficulty;
        if (choosePuzzle(this, puzzle, difficulty, true))
            openGame(new SwapPuzzleGame(puzzle, difficulty));
    });
    connect(m_continueButton, &QPushButton::clicked, this, [this] {
        ChessRecord record;
        const QString id = GameSession::latest(record);
        if (!id.isEmpty()) openMatch(record, QStringLiteral("继续对局"), id);
        else refreshContinue();
    });
    refreshContinue();
    auto* settings = new QPushButton(QStringLiteral("游戏设置"), this);
    settings->setObjectName(QStringLiteral("menuSettingsButton"));
    auto *footer = new QHBoxLayout;
    auto *openRecord = new QPushButton(QStringLiteral("棋谱库"), this);
    openRecord->setObjectName(QStringLiteral("menuOpenRecordButton"));
    auto *guide = new QPushButton(QStringLiteral("象棋指南"), this);
    guide->setObjectName(QStringLiteral("menuGuideButton"));
    auto *setup = new QPushButton(QStringLiteral("自由摆棋"), this);
    setup->setObjectName(QStringLiteral("menuSetupButton"));
    footer->addWidget(setup);
    footer->addWidget(openRecord);
    footer->addWidget(guide);
    footer->addWidget(settings);
    for (auto *button : {setup, openRecord, guide, settings})
        button->setCursor(Qt::PointingHandCursor);
    layout->addLayout(footer);
    connect(openRecord, &QPushButton::clicked, this, [this] {
        ChessRecord record;
        QString id;
        if (!chooseChessRecord(this, record, &id)) return;
        if (id.isEmpty()) openGame(new ReplayGame(record));
        else openMatch(record, QStringLiteral("继续对局"), id);
    });
    connect(guide, &QPushButton::clicked, this, [this] { showChessGuide(this); });
    connect(setup, &QPushButton::clicked, this, [this] {
        auto *game = editStartingPosition(this);
        if (!game) return;
        game->setWindowTitle(QStringLiteral("自定义对局"));
        game->enableSessionSaving();
        openGame(game);
    });
    connect(settings, &QPushButton::clicked, this, [this] {
        GameSettings dialog(this);
        dialog.exec();
    });

    connect(m_buttons[3], &QPushButton::clicked, this, [this] {
        LanRoomDialog lobby(this);
        if (lobby.exec() != QDialog::Accepted) return;
        openGame(new NetworkGame(lobby.takeRoom()));
    });

    connect(m_buttons[0], &QPushButton::clicked, this, [this] {
        ChessRecord record;
        if (configureMatch(this, false, record))
            openMatch(record, QStringLiteral("玩家自己对战"));
    });
    connect(m_buttons[1], &QPushButton::clicked, this, [this] {
        ChessRecord record;
        if (!configureMatch(this, true, record)) return;
        const QStringList levels{QStringLiteral("简单"), QStringLiteral("中等"), QStringLiteral("困难")};
        openMatch(record, QStringLiteral("玩家和AI对战 [%1]").arg(levels.at(record.difficulty)));
    });
    connect(m_buttons[2], &QPushButton::clicked, this, [this] {
        PuzzleConfig puzzle;
        AIDifficulty difficulty;
        if (!choosePuzzle(this, puzzle, difficulty)) return;
        const QStringList levels{QStringLiteral("简单"), QStringLiteral("中等"), QStringLiteral("困难")};
        auto *game = new PuzzleGame(puzzle, difficulty);
        game->setWindowTitle(QStringLiteral("残局挑战 - %1 [%2]").arg(puzzle.name, levels.at(int(difficulty))));
        game->enableSessionSaving();
        openGame(game);
    });
}

void ChooseMainWindow::refreshContinue()
{
    ChessRecord record;
    m_continueButton->setEnabled(!GameSession::latest(record).isEmpty());
}

void ChooseMainWindow::openMatch(const ChessRecord &record, const QString &title, const QString &sessionId)
{
    ChessBoard *game = record.computer
        ? new MachineGame(static_cast<AIDifficulty>(record.difficulty)) : new ChessBoard;
    QString error;
    if (!game->resumeRecord(record, &error)) {
        delete game;
        QMessageBox::warning(this, QStringLiteral("无法开始对局"), error);
        return;
    }
    const auto localSides = !record.redPlayerId.isEmpty() && !record.blackPlayerId.isEmpty()
        ? QStringLiteral("本机计分对局") : QStringLiteral("红黑双方轮流操作");
    game->setGameDescription(record.mode, !record.computer ? localSides
        : record.aiRed ? QStringLiteral("你执黑棋 · 电脑执红") : QStringLiteral("你执红棋 · 电脑执黑"),
        record.description);
    game->setWindowTitle(title);
    game->enableSessionSaving(sessionId);
    openGame(game);
}

void ChooseMainWindow::openGame(ChessBoard *game)
{
    game->setAttribute(Qt::WA_DeleteOnClose);
    game->setAttribute(Qt::WA_QuitOnClose, false);
    connect(game, &ChessBoard::toMenu, game, &QWidget::close);
    connect(game, &QObject::destroyed, this, [this] {
        refreshContinue();
        show();
    });
    hide();
    game->show();
}


void ChooseMainWindow::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);
    if (m_sceneCache.size() != size() * devicePixelRatioF())
        m_sceneCache = createSceneBackground(size(), devicePixelRatioF());
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_sceneCache);
}
