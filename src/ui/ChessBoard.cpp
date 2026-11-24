// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#include "ChessBoard.h"
#include "core/ChessRules.h"
#include "GameTheme.h"
#include "GameResultDialog.h"
#include "ui_ChessBoard.h"
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QCloseEvent>
#include <QLineF>
#include <QPainterPath>
#include <utility>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include "game/MachineGame.h"
#include "game/ReplayGame.h"
#include "GamePages.h"
#include "game/GameSession.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <algorithm>

ChessBoard::ChessBoard(QWidget *parent) :
    QMainWindow(parent), ui(new Ui::ChessBoard)
{
    m_nD = 90.0;
    m_nOffSet = 120.0;
    m_nR = 36.0;
    m_bReverseView = false;
    init();
    m_timer = new QTimer(this);
    m_timeRecord = new QTime(0, 0, 0);
    m_bIsStart = false;
    connect(m_timer, &QTimer::timeout, this, &ChessBoard::updateTime);
    ui->setupUi(this);
    m_uiReady = true;
    m_matchTimer = new QTimer(this);
    m_matchTimer->setInterval(100);
    connect(m_matchTimer, &QTimer::timeout, this, [this] { settleMatchClock(); });
    auto *clocks = new QHBoxLayout;
    m_redClock = new QLabel(this);
    m_blackClock = new QLabel(this);
    m_redClock->setObjectName(QStringLiteral("redMatchClock"));
    m_blackClock->setObjectName(QStringLiteral("blackMatchClock"));
    for (auto *label : {m_redClock, m_blackClock}) {
        label->setAlignment(Qt::AlignCenter);
        clocks->addWidget(label, 1);
    }
    ui->controlCardLayout->insertLayout(2, clocks);
    refreshMatchClock();
    setWindowIcon(QIcon(":/images/chess.svg"));
    ui->label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->labelPuzzleDescription->hide();
    ui->lcdNumber->ensurePolished();
    ui->lcdNumber->setMinimumWidth(ui->lcdNumber->fontMetrics().horizontalAdvance(QStringLiteral("00:00:00")) + 2);
    ui->headerState->setMaximumHeight(34);
    ui->pushButton_start->setToolTip(QStringLiteral("只控制本局用时的记录"));
    ui->pushButton_back->setToolTip(QStringLiteral("撤回上一步，人机模式撤回一轮"));
    for (auto* button : findChildren<QPushButton*>())
        button->setCursor(Qt::PointingHandCursor);

    m_noticeTimer = new QTimer(this);
    m_noticeTimer->setSingleShot(true);
    connect(m_noticeTimer, &QTimer::timeout, this, &ChessBoard::refreshPanel);
    m_moveAnimation = new QVariantAnimation(this);
    m_moveAnimation->setStartValue(0.0);
    m_moveAnimation->setEndValue(1.0);
    m_moveAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_moveAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_moveProgress = value.toReal();
        const QRect canvas(ui->label->mapTo(this, QPoint()), ui->label->size());
        update(canvas);
    });
    connect(m_moveAnimation, &QVariantAnimation::finished, this, [this]() {
        m_animatedPiece = -1;
        m_capturedPiece = -1;
        m_moveProgress = 1.0;
        refreshPanel();
        update();
        emit moveAnimationSettled();
    });
    connect(ui->checkAnimations, &QCheckBox::toggled, this, [this](bool enabled) {
        m_preferences.animations = enabled;
        m_preferences.save();
        if (!enabled) {
            stopMoveAnimation();
            refreshPanel();
            update();
        }
    });
    auto* settings = new QPushButton(QStringLiteral("设置"), this);
    settings->setObjectName(QStringLiteral("gameSettingsButton"));
    ui->headerLayout->insertWidget(ui->headerLayout->count() - 1, settings);
    connect(settings, &QPushButton::clicked, this, [this] {
        GameSettings dialog(this);
        if (dialog.exec() == QDialog::Accepted) applyPreferences();
    });
    auto* endingActions = new QHBoxLayout;
    m_resignButton = new QPushButton(QStringLiteral("认输"), this);
    m_resignButton->setObjectName(QStringLiteral("resignButton"));
    m_drawButton = new QPushButton(QStringLiteral("求和"), this);
    m_drawButton->setObjectName(QStringLiteral("drawButton"));
    endingActions->addWidget(m_resignButton);
    endingActions->addWidget(m_drawButton);
    ui->controlsLayout->insertLayout(ui->controlsLayout->count() - 1, endingActions);
    connect(m_resignButton, &QPushButton::clicked, this, [this] { resign(); });
    connect(m_drawButton, &QPushButton::clicked, this, [this] { offerDraw(); });
    auto *recordActions = new QHBoxLayout;
    auto *saveRecord = new QPushButton(QStringLiteral("保存棋谱"), this);
    saveRecord->setObjectName(QStringLiteral("saveRecordButton"));
    auto *reviewRecord = new QPushButton(QStringLiteral("复盘"), this);
    reviewRecord->setObjectName(QStringLiteral("reviewRecordButton"));
    saveRecord->setCursor(Qt::PointingHandCursor);
    reviewRecord->setCursor(Qt::PointingHandCursor);
    recordActions->addWidget(saveRecord);
    recordActions->addWidget(reviewRecord);
    ui->historyLayout->addLayout(recordActions);
    connect(saveRecord, &QPushButton::clicked, this, [this] {
        const QString suggested = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/象棋棋谱-%1.cchess").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存棋谱"), suggested,
                                                        QStringLiteral("象棋棋谱 (*.cchess)"));
        if (path.isEmpty()) return;
        QString error;
        if (!gameRecord().save(path, &error))
            QMessageBox::warning(this, QStringLiteral("保存失败"), error);
        else {
            rememberChessRecord(path);
            showNotice(QStringLiteral("棋谱已保存"));
        }
    });
    connect(reviewRecord, &QPushButton::clicked, this, [this] {
        auto *replay = new ReplayGame(gameRecord());
        replay->setAttribute(Qt::WA_DeleteOnClose);
        connect(replay, &ChessBoard::toMenu, replay, &QWidget::close);
        replay->show();
    });
    applyPreferences();
    const QSize available = QGuiApplication::primaryScreen()->availableGeometry().size();
    setMinimumSize(qMin(760, available.width() - 32), qMin(500, available.height() - 48));
    ui->actionRow->setDirection(available.height() < 740 ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    resize(qMin(1280, int(available.width() * 0.90)), qMin(800, int(available.height() * 0.88)));
    buildPieceCache();
    ui->redAvatar->setPixmap(m_pieceCache[1][0]);
    ui->blackAvatar->setPixmap(m_pieceCache[0][0]);
    refreshPanel();
}

ChessBoard::~ChessBoard()
{
    qDeleteAll(m_ChessSteps);
    delete m_timeRecord;
    delete ui;
}

void ChessBoard::init()
{
    if (m_matchTimer) m_matchTimer->stop();
    m_turnClock.invalidate();
    m_redRemainingMs = m_blackRemainingMs = m_clockLimitMs;
    m_Chessvoice.stop();
    dismissGameResult();
    stopMoveAnimation();
    qDeleteAll(m_ChessSteps);
    m_ChessSteps.clear();
    for (int i = 0; i < 32; ++i)
        m_ChessPieces[i].init(i);
    if (m_loadedRecord)
        std::copy(m_loadedRecord->initial.begin(), m_loadedRecord->initial.end(), std::begin(m_ChessPieces));
    textStepRecord.clear();
    m_nSelectID = -1;
    m_nCheckedID = -1;
    m_bIsRed = m_loadedRecord ? m_loadedRecord->redFirst : true;
    m_bIsOver = false;
    m_allowExit = false;
    m_outcome = Outcome::Ongoing;
    m_resultReason.clear();
    m_bIsShowStep = GamePreferences::load().traces;
    m_bTimerAutoStarted = false;
    m_aiThinking = false;
    if (m_noticeTimer)
        m_noticeTimer->stop();
    m_displayedSteps = -1;
    m_repetitionSteps = -1;
    if (m_uiReady) {
        resetGameTimer();
        refreshPanel();
    }
}

bool ChessBoard:: isRed(int id)
{
    return m_ChessPieces[id].m_bRed;
}


void ChessBoard:: killStone(int id)
{
    if(id== -1)
        return;
    m_ChessPieces[id].m_bDead= true;
}

void ChessBoard:: reliveStone(int id)
{
    if(id== -1)
        return;
    m_ChessPieces[id].m_bDead= false;
}

void ChessBoard:: moveStone(int moveid, int row, int col)
{
    m_ChessPieces[moveid].m_nRow= row;
    m_ChessPieces[moveid].m_nCol= col;

    m_bIsRed= !m_bIsRed;   //换边
}

bool ChessBoard::sameColor(int moveId,int killId)
{
    if(moveId== -1 || killId== -1)
        return false;

    return isRed(moveId)== isRed(killId);
}

bool ChessBoard::isDead(int id)
{
    if(id == -1)
        return true;

    return m_ChessPieces[id].m_bDead;
}

int ChessBoard::getStoneId(int row, int col)
{
    return ChessRules(m_ChessPieces).getStoneId(row, col);
}

int ChessBoard::getStoneCountAtLine(int row1, int col1, int row2, int col2)
{
    return ChessRules(m_ChessPieces).getStoneCountAtLine(row1, col1, row2, col2);
}

void ChessBoard::whoWin()
{
    if (m_bIsOver) return;
    if (m_ChessPieces[4].m_bDead) {
        finishGame(Outcome::RedWon, QStringLiteral("黑将被吃"));
        return;
    }
    if (m_ChessPieces[20].m_bDead) {
        finishGame(Outcome::BlackWon, QStringLiteral("红帅被吃"));
        return;
    }
    const ChessRules rules(m_ChessPieces);
    if (!rules.hasLegalMove(m_bIsRed))
        finishGame(m_bIsRed ? Outcome::BlackWon : Outcome::RedWon,
                   rules.isInCheck(m_bIsRed) ? QStringLiteral("将死") : QStringLiteral("困毙"));
}

void ChessBoard::finishGame(Outcome outcome, const QString& reason)
{
    if (m_bIsOver || outcome == Outcome::Ongoing) return;
    m_outcome = outcome;
    m_resultReason = reason;
    if (m_turnClock.isValid()) {
        const int remaining = remainingTime(m_bIsRed);
        (m_bIsRed ? m_redRemainingMs : m_blackRemainingMs) = remaining;
        m_turnClock.invalidate();
    }
    m_matchTimer->stop();
    const bool redWon = outcome == Outcome::RedWon;
    const quint64 generation = m_gameGeneration;
    reset();
    refreshPanel();
    checkpointSession();
    runAfterMoveAnimation([this, redWon, generation] {
        if (generation == m_gameGeneration && m_bIsOver) showGameResult(redWon);
    });
}

void ChessBoard::resign()
{
    if (m_bIsOver) return;
    if (confirmAction(QStringLiteral("认输"), QStringLiteral("确认认输并结束本局？")))
        finishGame(humanIsRed() ? Outcome::BlackWon : Outcome::RedWon,
                   humanIsRed() ? QStringLiteral("红方认输") : QStringLiteral("黑方认输"));
}

void ChessBoard::offerDraw()
{
    if (m_bIsOver) return;
    if (confirmAction(QStringLiteral("和棋"), QStringLiteral("是否同意以和棋结束本局？")))
        finishGame(Outcome::Draw, QStringLiteral("双方同意和棋"));
}

int ChessBoard:: relation(int row1,int col1,int row2,int col2)
{
    return ChessRules(m_ChessPieces).relation(row1, col1, row2, col2);
}

//是否选中该枚棋子。pt为输入参数; row， col为输出参数
bool ChessBoard::isChecked(QPointF pt, int &row, int &col)
{
    for (row = 0; row <= 9; row++) {
        for (col = 0; col <= 8; col++) {
            QPointF temp = center(row, col);
            qreal dx = temp.x() - pt.x();  // 使用 qreal
            qreal dy = temp.y() - pt.y();  // 使用 qreal
            if (dx * dx + dy * dy < m_nR * m_nR) {
                return true;
            }
        }
    }
    return false;
}


//象棋的棋盘的坐标转换成界面坐标
QPointF ChessBoard::center(int row, int col)
{
    const int displayRow = m_bReverseView ? 9 - row : row;
    const int displayCol = m_bReverseView ? 8 - col : col;
    return QPointF(m_nOffSet + displayCol * m_nD, 75.0 + displayRow * m_nD);
}

//重载:坐标转换
QPointF ChessBoard::center(int id)
{
    return center(m_ChessPieces[id].m_nRow, m_ChessPieces[id].m_nCol);
}

void ChessBoard::setPerspectiveFlipped(bool flipped)
{
    if (m_bReverseView == flipped)
        return;

    stopMoveAnimation();
    m_bReverseView = flipped;
    update();
}

bool ChessBoard::boardTransform(QPointF& origin, qreal& side) const
{
    if (!ui || !ui->label)
        return false;

    const QPoint topLeft = ui->label->mapTo(this, QPoint(0, 0));
    const QSize boardSize = ui->label->size();
    if (boardSize.isEmpty())
        return false;

    const qreal width = boardSize.width();
    const qreal height = boardSize.height();
    // The visible wood frame occupies x=40..920 in the 960-unit canvas.
    const qreal scale = qMin(width / 880.0, height / 960.0);
    side = scale * 960.0;
    if (side <= 0.0)
        return false;
    origin = QPointF(topLeft);
    origin.rx() += (width - 880.0 * scale) / 2.0 - 40.0 * scale;
    origin.ry() += (height - side) / 2.0;
    return true;
}

void ChessBoard::paintEvent(QPaintEvent *event)
{
    QMainWindow::paintEvent(event);
    if (m_sceneCache.size() != size() * devicePixelRatioF())
        m_sceneCache = createSceneBackground(size(), devicePixelRatioF());
    {
        QPainter background(this);
        background.setClipRegion(event->region());
        background.drawPixmap(0, 0, m_sceneCache);
    }
    QPointF origin;
    qreal side = 0;
    if (!boardTransform(origin, side))
        return;
    if (m_boardCache.isNull())
        buildBoardCache();
    if (m_pieceCache[0][0].isNull())
        buildPieceCache();
    QPainter painter(this);
    painter.setClipRegion(event->region());
    painter.setClipRect(QRect(ui->label->mapTo(this, QPoint()), ui->label->size()), Qt::IntersectClip);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.translate(origin);
    painter.scale(side / 960.0, side / 960.0);
    painter.drawPixmap(QPointF(0, 0), m_boardCache);
    if (m_bIsShowStep)
        drawLastStep(painter, m_ChessSteps);
    if (m_preferences.legalMoves && m_nSelectID >= 0 && !m_bIsOver && !isMoveAnimating()) {
        const ChessRules rules(m_ChessPieces);
        for (int row = 0; row < 10; ++row) {
            for (int col = 0; col < 9; ++col) {
                const int target = getStoneId(row, col);
                if (!rules.canMove(m_nSelectID, target, row, col)) continue;
                painter.setPen(QPen(QColor("#487046"), target == -1 ? 2 : 4));
                painter.setBrush(target == -1 ? QBrush(QColor(63, 107, 63, 155)) : Qt::NoBrush);
                painter.drawEllipse(center(row, col), target == -1 ? 8 : 40, target == -1 ? 8 : 40);
            }
        }
    }
    for (int id = 0; id < 32; ++id) {
        if (id != m_animatedPiece)
            drawChessPieces(painter, id);
    }
    if (m_animatedPiece != -1)
        drawChessPieces(painter, m_animatedPiece);
}

void ChessBoard::drawChessPieces(QPainter &painter, int id)   //绘画单个具体的棋子
{
    const bool fadingCapture = id == m_capturedPiece && isMoveAnimating();
    if (isDead(id) && !fadingCapture)
        return;
    QPointF position = center(id);
    if (id == m_animatedPiece)
        position = m_animationFrom + (m_animationTo - m_animationFrom) * m_moveProgress;
    painter.save();
    if (fadingCapture)
        painter.setOpacity(1.0 - m_moveProgress);
    if (m_nSelectID == id) {
        painter.setPen(QPen(QColor("#386451"), 3.5));
        painter.setBrush(QColor(56, 100, 81, 25));
        painter.drawEllipse(position, 42, 42);
    }
    const auto& piece = m_ChessPieces[id];
    painter.drawPixmap(position - QPointF(48, 48),
                       m_pieceCache[piece.m_bRed ? 1 : 0][piece.m_emType]);
    painter.restore();
}

void ChessBoard::drawStarMarks(QPainter &painter)
{
    // 星位标记位置：炮位(row=2,col=1/7; row=7,col=1/7) 和 兵/卒位
    // 兵位: row=3,col=0/2/4/6/8; row=6,col=0/2/4/6/8
    struct StarPos { int row; int col; };
    StarPos positions[] = {
        {2,1},{2,7},{7,1},{7,7},  // 炮位
        {3,0},{3,2},{3,4},{3,6},{3,8},  // 黑方兵位
        {6,0},{6,2},{6,4},{6,6},{6,8}   // 红方卒位
    };

    const qreal len = m_nD * 0.15;  // 标记线长度
    const qreal gap = m_nD * 0.08;  // 与交叉点的间距
    painter.setPen(QPen(QColor(101, 67, 33), 1.5));

    for (const auto& pos : positions) {
        qreal cx = m_nOffSet + pos.col * m_nD;
        qreal cy = 75.0 + pos.row * m_nD;

        // 右下角标记（边界列除外右侧）
        if (pos.col < 8) {
            painter.drawLine(QPointF(cx+gap, cy+gap), QPointF(cx+gap+len, cy+gap));
            painter.drawLine(QPointF(cx+gap, cy+gap), QPointF(cx+gap, cy+gap+len));
            painter.drawLine(QPointF(cx+gap, cy-gap), QPointF(cx+gap+len, cy-gap));
            painter.drawLine(QPointF(cx+gap, cy-gap), QPointF(cx+gap, cy-gap-len));
        }
        // 左侧标记（边界列除外左侧）
        if (pos.col > 0) {
            painter.drawLine(QPointF(cx-gap, cy+gap), QPointF(cx-gap-len, cy+gap));
            painter.drawLine(QPointF(cx-gap, cy+gap), QPointF(cx-gap, cy+gap+len));
            painter.drawLine(QPointF(cx-gap, cy-gap), QPointF(cx-gap-len, cy-gap));
            painter.drawLine(QPointF(cx-gap, cy-gap), QPointF(cx-gap, cy-gap-len));
        }
    }
}

void ChessBoard:: drawLastStep(QPainter &painter,QVector<ChessStep*>& steps)
{
    if (steps.isEmpty())
        return;
    const auto* step = steps.last();
    const QPointF from = center(step->m_nRowFrom, step->m_nColFrom);
    const QPointF to = center(step->m_nRowTo, step->m_nnColTo);
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#a47c46"));
    painter.drawEllipse(from, 5.0, 5.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(143, 103, 49, 175), 2, Qt::DashLine));
    painter.drawEllipse(to, 41.0, 41.0);
    painter.restore();
}

void ChessBoard::drawTextStep()
{
    const int count = m_ChessSteps.size();
    if (m_displayedSteps == count)
        return;
    if (m_displayedSteps <= 0)
        ui->moveList->clear();
    while (ui->moveList->count() > count)
        delete ui->moveList->takeItem(ui->moveList->count() - 1);
    while (ui->moveList->count() < count) {
        const int index = ui->moveList->count();
        const auto* step = m_ChessSteps[index];
        const bool red = isRed(step->m_nMoveID);
        auto* item = new QListWidgetItem(QStringLiteral("%1   %2   %3")
            .arg(index + 1, 2).arg(red ? QStringLiteral("红") : QStringLiteral("黑"))
            .arg(step->m_notation), ui->moveList);
        item->setForeground(QColor(red ? "#9d3d31" : "#35473c"));
    }
    if (count == 0) {
        auto* empty = new QListWidgetItem(QStringLiteral("等待第一步落子"), ui->moveList);
        empty->setForeground(QColor("#73766a"));
        empty->setFlags(Qt::NoItemFlags);
    } else {
        ui->moveList->scrollToBottom();
    }
    m_displayedSteps = count;
}

// true 产生"对将" 情景了；false 无"对将"情况
bool ChessBoard::hongMenFeast()
{
    if (m_ChessPieces[4].m_bDead || m_ChessPieces[20].m_bDead)
        return false;

    int colBlack = m_ChessPieces[4].m_nCol;
    int colRed = m_ChessPieces[20].m_nCol;
    int rowBlack = m_ChessPieces[4].m_nRow;
    int rowRed = m_ChessPieces[20].m_nRow;

    bool bColEmpty = true;
    if (colBlack == colRed){
        for (int row = rowBlack + 1; row < rowRed ; ++row) {
            if (havePieces(row, colBlack))
                bColEmpty = false;  // 将之间有棋子；非此列为空
        }
    } else {
        bColEmpty = false;
    }

    return bColEmpty;
}

// 判断某格子是否有棋子在其上
bool ChessBoard::havePieces(int row, int col)
{
    for (auto pieces : m_ChessPieces) {
        if (pieces.m_bDead)
            continue;

        if (pieces.m_nRow == row && pieces.m_nCol == col)
            return true;
    }

    return false;
}

// 胜负已分，重置
void ChessBoard::reset()
{
    if (m_outcome == Outcome::Draw) m_Chessvoice.voiceDraw();
    else m_Chessvoice.voiceWin();
    m_bIsOver = true;
    //游戏结束 则计时停止 & 计时控制按钮不再可用 直到用户重新游戏
    if(m_bIsStart)
    {
        pauseGameTimer();
    }

    ui->pushButton_start->setText("开始计时");
    ui->pushButton_start->setEnabled(false);
    m_bTimerAutoStarted = false;
}

void ChessBoard::startGameTimer()
{
    if (!m_timer || m_bIsStart)
        return;

    m_timer->start(1000);
    m_bIsStart = true;
    ui->pushButton_start->setText("暂停计时");
}

void ChessBoard::pauseGameTimer()
{
    if (!m_timer || !m_bIsStart)
        return;

    m_timer->stop();
    m_bIsStart = false;
    ui->pushButton_start->setText("继续计时");
}

void ChessBoard::autoStartTimerIfNeeded()
{
    if (m_bIsOver || m_bTimerAutoStarted)
        return;

    startGameTimer();
    m_bTimerAutoStarted = true;
}

void ChessBoard::showGameResult(bool redWon)
{
    if (m_resultDialog || !m_bIsOver)
        return;
    const quint64 generation = m_gameGeneration;
    const QString winnerRole = redWon ? ui->labelRedRole->text() : ui->labelBlackRole->text();
    auto* dialog = new GameResultDialog(redWon, winnerRole, ui->labelMode->text(),
        m_timeRecord->toString(QStringLiteral("hh:mm:ss")), m_ChessSteps.size(),
        m_pieceCache[redWon ? 1 : 0][ChessPieces::JIANG], this,
        m_resultReason, m_outcome == Outcome::Draw);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_resultDialog = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog, generation](int action) {
        if (m_resultDialog == dialog)
            m_resultDialog = nullptr;
        if (generation != m_gameGeneration || !m_bIsOver)
            return;
        if (action == GameResultDialog::PlayAgain)
            on_pushButton_restart_clicked();
        else if (action == GameResultDialog::ReturnToMenu)
            on_pushButton_toMenu_clicked();
    });
    dialog->open();
}

void ChessBoard::dismissGameResult()
{
    ++m_gameGeneration;
    if (m_resultDialog)
        m_resultDialog->reject();
}

QPointF ChessBoard::getRealPoint(QPointF pt)
{
    QPointF origin;
    qreal side = 0.0;
    if (!boardTransform(origin, side) || side <= 0.0)
        return QPointF(-1.0, -1.0);

    const QPointF relative = pt - origin;
    QPointF realPt;
    realPt.setX(relative.x() * 960.0 / side);
    realPt.setY(relative.y() * 960.0 / side);
    return realPt;
}

bool ChessBoard::isGeneral()
{
    return ChessRules(m_ChessPieces).isInCheck(m_bIsRed);
}

//鼠标按下事件
//void ChessBoard::mousePressEvent(QMouseEvent *ev)
//{
//    //只响应鼠标左键的单击操作 防止游戏结束重复弹框
//    if(ev->button() != Qt::LeftButton || ev->type() != QEvent::Type::MouseButtonPress)
//        return;

//    QPoint pt = ev->pos();
//    pt = getRealPoint(pt);
//    //将pt转化成棋盘的像行列值
//    //判断这个行列值上面有没有棋子
//    int row, col;

//    //点击棋盘外面就不做处理
//    if(!isChecked(pt, row, col))
//        return;

//    if(m_bIsOver)
//    {
//        QMessageBox message(QMessageBox::Information, "提示", "本局已结束，请重新开始.");
//        message.setIconPixmap(QPixmap(":/images/win.jpg"));
//        message.setFont(QFont("FangSong",16,QFont::Bold));
//        message.exec();
//        return;
//    }

//    //判断是哪一个棋子被选中，根据ID（这里的局部i）来记录下来
//    int i;
//    m_nCheckedID = -1;

//    for(i = 0; i <= 31; i++)
//    {
//        if(m_ChessPieces[i].m_nRow == row && m_ChessPieces[i].m_nCol == col && !m_ChessPieces[i].m_bDead)
//            break;
//    }

//    if(0<=i && i<32)
//        m_nCheckedID = i;  //选中的棋子的ID

//    bool newbIsRed = m_bIsRed;
//    clickPieces(m_nCheckedID, row, col);

//    // 刚执棋落子完成，出现对将
//    if (hongMenFeast() && m_nSelectID == -1 && newbIsRed != m_bIsRed)
//    {
//        winMessageBox("提示", "可将军，直接取胜");
//        // TODO: 可将军，直接提示直接取胜的音效
//    }

//    update();
//    whoWin();
//}

//void ChessBoard::clickPieces(int checkedID, int& row, int& col)
//{
//    m_nCheckedID = checkedID;

//    if(m_nSelectID == -1) //选中棋子
//    {
//       // whoPlay(m_nCheckedID);

//        if(m_nCheckedID != -1)
//        {
//            if(m_bIsRed == m_ChessPieces[m_nCheckedID].m_bRed)
//            {
//                m_nSelectID = m_nCheckedID;
//                m_Chessvoice.voiceSelect();   //选棋音效
//            }
//        }
//    }
//    else//走棋子
//    {
//        if(canMove(m_nSelectID, m_nCheckedID, row, col ))
//        {
//            //m_nSelectID为第一次点击选中的棋子，
//            //m_nCheckedID为第二次点击||被杀的棋子ID，准备选中棋子下子的地方
//            m_ChessPieces[m_nSelectID].m_nRow = row;
//            m_ChessPieces[m_nSelectID].m_nCol = col;
//            if(m_nCheckedID != -1)
//            {
//                m_ChessPieces[m_nCheckedID].m_bDead = true;
//                m_Chessvoice.voiceEat();  //吃子音效
//            }
//            m_Chessvoice.voiceMove(); //移动音效

//            m_nSelectID = -1;
//            m_bIsRed = !m_bIsRed;
//        }
//    }
//}


//总的移动规则，选中准备下的棋子，被杀的棋子， 准备移动到的目的行列值
//bool ChessBoard::canMove(int moveId, int killId, int row, int col)
//{
//    //1.确定是选择其它棋子还是走棋
//    //2.是否需要使用到canMoveXXX()来做限制
//    //3.罗列出所有情况，和需要的得到的结果值 ==>  然后进行中间的逻辑层判断※不要受到别人的代码框架的束缚※

//        if(isRed(moveId) == m_ChessPieces[killId].m_bRed)  //选择其它棋子，返回false
//        {
//            if(killId == -1)  //其中有一个特殊情况，黑+m_ChessPieces[-1].m_bRed ==> 也需要判断能否
//            {
//                switch (m_ChessPieces[moveId].m_emType)
//                {
//                case ChessPieces::JIANG:
//                    return canMoveJIANG(moveId, killId, row, col);
//                case ChessPieces::SHI:
//                    return canMoveSHI(moveId, killId, row, col);
//                case ChessPieces::XIANG:
//                    return canMoveXIANG(moveId, killId, row, col);
//                case ChessPieces::MA:
//                    return canMoveMA(moveId, killId, row, col);
//                case ChessPieces::CHE:
//                    return canMoveCHE(moveId, killId, row, col);
//                case ChessPieces::PAO:
//                    return canMovePAO(moveId, killId, row, col);
//                case ChessPieces::BING:
//                    return canMoveBING(moveId, killId, row, col);
//                }
//            }
//            m_nSelectID = killId;

//            return false;
//        }
//        else  //选择其走棋，返回true
//        {
//            switch (m_ChessPieces[moveId].m_emType)
//            {
//            case ChessPieces::JIANG:
//                return canMoveJIANG(moveId, killId, row, col);
//            case ChessPieces::SHI:
//                return canMoveSHI(moveId, killId, row, col);
//            case ChessPieces::XIANG:
//                return canMoveXIANG(moveId, killId, row, col);
//            case ChessPieces::MA:
//                return canMoveMA(moveId, killId, row, col);
//            case ChessPieces::CHE:
//                return canMoveCHE(moveId, killId, row, col);
//            case ChessPieces::PAO:
//                return canMovePAO(moveId, killId, row, col);
//            case ChessPieces::BING:
//                return canMoveBING(moveId, killId, row, col);
//            }

//            return true;

//        }
//}

//总的移动规则
bool ChessBoard::canMove(int moveId, int killId, int row, int col)
{
    if (moveId < 0 || moveId >= 32 || killId < -1 || killId >= 32)
        return false;
    if (sameColor(moveId, killId)) {
        m_nSelectID = killId;
        update();
        return false;
    }
    return ChessRules(m_ChessPieces).canMove(moveId, killId, row, col);
}

bool ChessBoard::canMoveJIANG(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveJIANG(moveId, killId, row, col);
}

bool ChessBoard::canMoveSHI(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveSHI(moveId, killId, row, col);
}

bool ChessBoard::canMoveXIANG(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveXIANG(moveId, killId, row, col);
}

bool ChessBoard::canMoveMA(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveMA(moveId, killId, row, col);
}

bool ChessBoard::canMoveCHE(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveCHE(moveId, killId, row, col);
}

bool ChessBoard::canMovePAO(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMovePAO(moveId, killId, row, col);
}

bool ChessBoard::canMoveBING(int moveId, int killId, int row, int col)
{
    return ChessRules(m_ChessPieces).canMoveBING(moveId, killId, row, col);
}

bool ChessBoard:: canSelect(int id)
{
    return m_bIsRed== m_ChessPieces[id].m_bRed;
}

void ChessBoard::mouseReleaseEvent(QMouseEvent *ev)
{
    if (ev->button() != Qt::LeftButton || m_bIsOver || isMoveAnimating()) { // 排除鼠标右键点击 游戏已结束则直接返回
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF globalPos = ev->globalPosition();
#else
    const QPointF globalPos = ev->globalPos();
#endif
    const QPointF windowTopLeft = mapToGlobal(QPoint(0, 0));
    const QPointF mousePos = globalPos - windowTopLeft;  // 统一转换为 ChessBoard 坐标系

    QPointF pt = getRealPoint(mousePos);  // 转换为虚拟坐标
    click(pt);
}

void ChessBoard::click(QPointF pt)
{
    // 看有没有点中象棋
    // 将pt转化成象棋的行列值
    // 判断这个行列值上面有没有棋子
    int row, col;
    bool bClicked = isChecked(pt, row, col);
    if (!bClicked) {
        return;
    }

    int id = getStoneId(row, col);
    clickPieces(id, row, col);

}

void ChessBoard::clickPieces(int id, int &row, int &col)
{
    if (this->m_nSelectID == -1) { // 如果点中的棋子之前未被选中
        trySelectStone(id);
    }
    else {
        tryMoveStone(id, row, col);
    }

}

void ChessBoard::trySelectStone(int id)
{
    if (id == -1) {
        return;
    }

    if (!canSelect(id)) {
        return;
    }

    m_nSelectID = id;
    m_noticeTimer->stop();
    refreshPanel();
    update();
    m_Chessvoice.voiceSelect();
}

void ChessBoard::tryMoveStone(int killid, int row, int col)
{
    if (killid != -1 && sameColor(killid, m_nSelectID)) {
        trySelectStone(killid);
        return;
    }

    bool ret = canMove(m_nSelectID, killid, row, col);
    if (ret) {
        doMoveStone(m_nSelectID, killid, row, col);
        m_nSelectID = -1;
        update();
    } else {
        showNotice(QStringLiteral("此处无法落子，请重新选择"));
    }
}

void ChessBoard::doMoveStone(int moveid, int killid, int row, int col)
{
    if (m_bIsOver || !settleMatchClock()) return;
    m_noticeTimer->stop();
    autoStartTimerIfNeeded();
    const QPointF from = center(moveid);
    const QPointF to = center(row, col);
    saveStep(moveid, killid, row, col, m_ChessSteps);

    killStone(killid);
    moveStone(moveid, row, col);
    if (m_turnClock.isValid()) m_turnClock.restart();
    m_nSelectID = -1;
    animateMove(moveid, killid, from, to);
    refreshPanel();
    whoWin();
    if (!m_bIsOver) afterMove();

    if (m_bIsOver) return;
    checkpointSession();

    if(killid== -1)
        m_Chessvoice.voiceMove(); //移动音效
    else
        m_Chessvoice.voiceEat(); //吃子音效

    if(isGeneral())
        m_Chessvoice.voiceGeneral();    //将军音效
}

void ChessBoard::saveStep(int moveid, int killid, int row, int col, QVector<ChessStep*>& steps)
{
    ChessStep* step = new ChessStep;
    step->m_nColFrom = m_ChessPieces[moveid].m_nCol;
    step->m_nnColTo = col;
    step->m_nRowFrom = m_ChessPieces[moveid].m_nRow;
    step->m_nRowTo = row;
    step->m_nMoveID = moveid;
    step->m_nKillID = killid;

    steps.append(step);
    textStepRecord = textStep(moveid, row, col);
    step->m_notation = textStepRecord;
}

QString ChessBoard::textStep(int id, int row, int col)
{
    int rowFrom= m_ChessPieces[id].m_nRow;
    int rowTo= row;
    int colFrom= m_ChessPieces[id].m_nCol;
    int colTo= col;

    QString temp="";
    QString name=m_ChessPieces[id].getnName(m_ChessPieces[id].m_bRed);
    QString textCol= m_ChessPieces[id].getColText(colFrom);
    QString textRow= m_ChessPieces[id].getRowText(rowTo);
    temp.append(name).append(textCol).append(textRow);

    //兵炮车将
    if(m_ChessPieces[id].m_emType==6 || m_ChessPieces[id].m_emType==5 || m_ChessPieces[id].m_emType==4 || m_ChessPieces[id].m_emType==0)
    {
        //行相等
        if(rowFrom== rowTo)
        {
            temp.append(m_ChessPieces[id].getColText(colTo));
            return temp;
        }
        //移动的格数
        temp.append(m_ChessPieces[id].getMoveText(rowFrom, rowTo));
    }
    else    //马相士
    {
        //移动后所在列
        temp.append(m_ChessPieces[id].getColText(colTo));
    }
    return temp;

}

void ChessBoard::backOne()
{
    if (this->m_ChessSteps.size() == 0 || m_bIsOver || hasMatchClock()) {
        return;
    }

    stopMoveAnimation();
    m_nSelectID = -1;
    ChessStep* step = this->m_ChessSteps.last();
    m_ChessSteps.removeLast();
    back(step);

    update();
    delete step;
    textStepRecord = m_ChessSteps.isEmpty() ? QString() : m_ChessSteps.last()->m_notation;
    refreshPanel();
    m_Chessvoice.voiceBack();

    // 悔棋到第0步时停止计时器
    if (m_ChessSteps.size() == 0 && m_bTimerAutoStarted) {
        pauseGameTimer();
        m_bTimerAutoStarted = false;
        ui->pushButton_start->setText("开始计时");
    }
    checkpointSession();
}

void ChessBoard::back(ChessStep* step)
{
    reliveStone(step->m_nKillID);
    moveStone(step->m_nMoveID, step->m_nRowFrom, step->m_nColFrom);
}

void ChessBoard::back()
{
    backOne();
}

//刷新时间
void ChessBoard::updateTime()
{
    *m_timeRecord = m_timeRecord->addSecs(1);
    ui->lcdNumber->setText(m_timeRecord->toString("hh:mm:ss"));
}

int ChessBoard::remainingTime(bool red) const
{
    const int stored = red ? m_redRemainingMs : m_blackRemainingMs;
    const qint64 spent = m_turnClock.isValid() && red == m_bIsRed ? m_turnClock.elapsed() : 0;
    return int(qMax<qint64>(0, stored - spent));
}

bool ChessBoard::settleMatchClock()
{
    if (!m_turnClock.isValid()) return true;
    int &remaining = m_bIsRed ? m_redRemainingMs : m_blackRemainingMs;
    remaining = int(qMax<qint64>(0, remaining - m_turnClock.restart()));
    if (remaining == 0) {
        m_turnClock.invalidate();
        finishGame(m_bIsRed ? Outcome::BlackWon : Outcome::RedWon,
                   m_bIsRed ? QStringLiteral("红方超时") : QStringLiteral("黑方超时"));
        return false;
    }
    refreshMatchClock();
    return true;
}

void ChessBoard::startMatchClock()
{
    if (!hasMatchClock() || m_bIsOver || m_turnClock.isValid()) return;
    m_turnClock.start();
    m_matchTimer->start();
    startGameTimer();
    refreshMatchClock();
}

void ChessBoard::refreshMatchClock()
{
    m_redClock->setVisible(hasMatchClock());
    m_blackClock->setVisible(hasMatchClock());
    if (!hasMatchClock()) return;
    ui->pushButton_start->setEnabled(false);
    for (const bool red : {true, false}) {
        auto *label = red ? m_redClock : m_blackClock;
        const int seconds = (remainingTime(red) + 999) / 1000;
        const bool active = m_turnClock.isValid() && red == m_bIsRed && !m_bIsOver;
        const QString display = QStringLiteral("%1 %2:%3")
            .arg(red ? QStringLiteral("红") : QStringLiteral("黑"))
            .arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
        if (label->text() != display) label->setText(display);
        if (label->property("clockActive") != active) {
            label->setProperty("clockActive", active);
            label->style()->unpolish(label);
            label->style()->polish(label);
        }
    }
}

void ChessBoard::enableSessionSaving(const QString &id)
{
    m_sessionId = id.isEmpty() ? GameSession::newId() : id;
    checkpointSession();
}

void ChessBoard::checkpointSession()
{
    if (m_sessionId.isEmpty()) return;
    QString error;
    if (!GameSession::save(m_sessionId, gameRecord(), &error))
        showNotice(QStringLiteral("自动保存失败 · %1").arg(error));
}

void ChessBoard::on_pushButton_start_clicked()
{
    if (hasMatchClock()) return;
    if(!m_bIsStart) //尚未开始 开始计时
    {
        startGameTimer();
        m_bTimerAutoStarted = true;
    }
    else //已经开始，暂停
    {
        pauseGameTimer();
    }
}

void ChessBoard::resetGameTimer()
{
    pauseGameTimer();    //计时器停止
    m_timeRecord->setHMS(0,0,0); //时间设为0
    ui->lcdNumber->setText(m_timeRecord->toString("hh:mm:ss")); //显示00:00:00
    ui->pushButton_start->setText("开始计时");
    ui->pushButton_start->setEnabled(true);
    m_bTimerAutoStarted = false;
}

int ChessBoard::elapsedSeconds() const
{
    return QTime(0, 0).secsTo(*m_timeRecord);
}

void ChessBoard::setElapsedSeconds(int seconds, bool running)
{
    pauseGameTimer();
    *m_timeRecord = QTime(0, 0).addSecs(qBound(0, seconds, 86399));
    ui->lcdNumber->setText(m_timeRecord->toString(QStringLiteral("hh:mm:ss")));
    m_bTimerAutoStarted = !m_ChessSteps.isEmpty();
    if (running && !m_bIsOver) startGameTimer();
}

bool ChessBoard::restoreMoves(const QVector<RecordedMove>& moves, int seconds)
{
    ChessPieces pieces[32];
    for (int i = 0; i < 32; ++i) pieces[i].init(i);
    bool red = true;
    for (const auto& move : moves) {
        ChessRules rules(pieces);
        if (move.id < 0 || move.id >= 32 || move.row < 0 || move.row > 9
            || move.col < 0 || move.col > 8 || move.captured < -1 || move.captured >= 32
            || pieces[4].m_bDead || pieces[20].m_bDead || pieces[move.id].m_bDead
            || pieces[move.id].m_bRed != red
            || rules.getStoneId(move.row, move.col) != move.captured
            || !rules.canMove(move.id, move.captured, move.row, move.col)) return false;
        if (move.captured != -1) pieces[move.captured].m_bDead = true;
        pieces[move.id].m_nRow = move.row;
        pieces[move.id].m_nCol = move.col;
        red = !red;
    }
    const bool showSteps = m_bIsShowStep;
    ChessBoard::init();
    m_bIsShowStep = showSteps;
    for (const auto& move : moves) {
        saveStep(move.id, move.captured, move.row, move.col, m_ChessSteps);
        killStone(move.captured);
        moveStone(move.id, move.row, move.col);
    }
    setElapsedSeconds(seconds, !moves.isEmpty());
    refreshPanel();
    whoWin();
    update();
    return true;
}

ChessRecord ChessBoard::gameRecord() const
{
    ChessRecord record;
    std::copy(std::begin(m_ChessPieces), std::end(m_ChessPieces), record.initial.begin());
    for (auto it = m_ChessSteps.crbegin(); it != m_ChessSteps.crend(); ++it) {
        const auto *step = *it;
        record.initial[step->m_nMoveID].m_nRow = step->m_nRowFrom;
        record.initial[step->m_nMoveID].m_nCol = step->m_nColFrom;
        if (step->m_nKillID != -1) record.initial[step->m_nKillID].m_bDead = false;
    }
    for (const auto *step : m_ChessSteps)
        record.moves.append({step->m_nMoveID, step->m_nKillID, step->m_nRowTo, step->m_nnColTo});
    record.redFirst = m_bIsRed != (m_ChessSteps.size() % 2 != 0);
    record.flipped = m_bReverseView;
    record.seconds = elapsedSeconds();
    record.clockLimitMs = m_clockLimitMs;
    record.redRemainingMs = remainingTime(true);
    record.blackRemainingMs = remainingTime(false);
    record.outcome = static_cast<int>(m_outcome);
    record.reason = m_resultReason;
    record.mode = ui->labelMode->text();
    record.redRole = ui->labelRedRole->text();
    record.blackRole = ui->labelBlackRole->text();
    if (m_loadedRecord) {
        record.redPlayerId = m_loadedRecord->redPlayerId;
        record.blackPlayerId = m_loadedRecord->blackPlayerId;
    }
    record.description = ui->labelPuzzleDescription->text();
    if (const auto *machine = dynamic_cast<const MachineGame *>(this)) {
        record.computer = true;
        record.aiRed = machine->isAiRed();
        record.difficulty = static_cast<int>(machine->difficulty());
    }
    return record;
}

int ChessBoard::repetitionCount() const
{
    const auto current = ChessRules::positionKey(m_ChessPieces, m_bIsRed);
    if (m_repetitionSteps == m_ChessSteps.size() && m_repetitionPosition == current)
        return m_repetitionCount;
    m_repetitionPosition = current;
    m_repetitionSteps = m_ChessSteps.size();
    m_repetitionCount = 1;
    std::array<ChessPieces, 32> pieces;
    std::copy(std::begin(m_ChessPieces), std::end(m_ChessPieces), pieces.begin());
    bool red = m_bIsRed;
    for (auto it = m_ChessSteps.crbegin(); it != m_ChessSteps.crend(); ++it) {
        const auto *step = *it;
        // A capture permanently changes material, so earlier positions cannot match.
        if (step->m_nKillID != -1) break;
        pieces[step->m_nMoveID].m_nRow = step->m_nRowFrom;
        pieces[step->m_nMoveID].m_nCol = step->m_nColFrom;
        red = !red;
        if (ChessRules::positionKey(pieces.data(), red) == current) ++m_repetitionCount;
    }
    return m_repetitionCount;
}

void ChessBoard::applyRecordPosition(const ChessRecord &record, int steps, bool restoreResult)
{
    const bool clockWasEnabled = hasMatchClock();
    m_loadedRecord = std::make_unique<ChessRecord>(record);
    ChessBoard::init();
    for (int index = 0; index < steps; ++index) {
        const auto &move = record.moves[index];
        saveStep(move.id, move.captured, move.row, move.col, m_ChessSteps);
        killStone(move.captured);
        moveStone(move.id, move.row, move.col);
    }
    if (restoreResult && steps == record.moves.size()) {
        m_outcome = static_cast<Outcome>(record.outcome);
        m_bIsOver = m_outcome != Outcome::Ongoing;
        m_resultReason = record.reason;
    }
    m_clockLimitMs = record.clockLimitMs;
    m_redRemainingMs = steps == record.moves.size() ? record.redRemainingMs : m_clockLimitMs;
    m_blackRemainingMs = steps == record.moves.size() ? record.blackRemainingMs : m_clockLimitMs;
    ui->elapsedCaption->setVisible(!hasMatchClock());
    ui->lcdNumber->setVisible(!hasMatchClock());
    if (clockWasEnabled != hasMatchClock())
        ui->pushButton_start->setVisible(!hasMatchClock());
    setElapsedSeconds(steps == record.moves.size() ? record.seconds : 0, false);
    ui->pushButton_start->setEnabled(!m_bIsOver);
    setPerspectiveFlipped(record.flipped);
    refreshPanel();
    update();
}

bool ChessBoard::resumeRecord(const ChessRecord &record, QString *error)
{
    if (!record.validate(error)) return false;
    applyRecordPosition(record, record.moves.size(), true);
    setPlayerRoles(record.redRole, record.blackRole);
    setGameDescription(record.mode, QStringLiteral("棋谱续局"), record.description);
    if (!record.computer) {
        const bool hasProfile = !record.redPlayerId.isEmpty() || !record.blackPlayerId.isEmpty();
        if (!hasProfile) setPlayerRoles(QStringLiteral("玩家"), QStringLiteral("玩家"));
        setGameDescription(record.mode.isEmpty() ? QStringLiteral("本地双人") : record.mode,
                           QStringLiteral("红黑双方轮流操作"), record.description);
    }
    setElapsedSeconds(record.seconds, !record.moves.isEmpty());
    startMatchClock();
    return true;
}

void ChessBoard::on_pushButton_restart_clicked()
{
    if (m_bIsOver && !m_sessionId.isEmpty()) {
        checkpointSession();
        m_sessionId = GameSession::newId();
    }
    init();
    startMatchClock();
    checkpointSession();
    update();
}

void ChessBoard::on_pushButton_back_clicked()
{
    back();
    update();
}

void ChessBoard::on_pushButton_showStep_clicked()
{
    m_bIsShowStep = ui->pushButton_showStep->isChecked();
    m_preferences.traces = m_bIsShowStep;
    m_preferences.save();
    update();
}

void ChessBoard::on_pushButton_toMenu_clicked()
{
    if (!confirmLeaving()) return;
    m_allowExit = true;
    stopMoveAnimation();
    emit this->toMenu();
}



void ChessBoard::buildBoardCache()
{
    m_boardCache = QPixmap(1920, 1920);
    m_boardCache.setDevicePixelRatio(2.0);
    m_boardCache.fill(Qt::transparent);
    QPainter painter(&m_boardCache);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF board(60, 12, 840, 936);
    painter.setPen(Qt::NoPen);
    for (int spread = 5; spread >= 1; --spread) {
        painter.setBrush(QColor(66, 53, 30, 5));
        painter.drawRoundedRect(board.adjusted(-spread, -spread, spread, spread).translated(0, 5), 19, 19);
    }
    QLinearGradient wood(board.topLeft(), board.bottomRight());
    wood.setColorAt(0, QColor("#b97a46"));
    wood.setColorAt(0.5, QColor("#92572f"));
    wood.setColorAt(1, QColor("#643d27"));
    painter.setBrush(wood);
    painter.setPen(QPen(QColor("#b79769"), 1.5));
    painter.drawRoundedRect(board, 16, 16);
    QLinearGradient surface(board.topLeft(), board.bottomRight());
    surface.setColorAt(0, QColor("#f2d3a0"));
    surface.setColorAt(1, QColor("#e4b97b"));
    painter.setBrush(surface);
    painter.setPen(QPen(QColor("#d7ac70"), 1.4));
    painter.drawRoundedRect(board.adjusted(20, 20, -20, -20), 7, 7);
    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(board.adjusted(2, 2, -2, -2), 15, 15);
    painter.setClipPath(clip);
    painter.setPen(QPen(QColor(121, 91, 49, 9), 0.7));
    for (int y = 25; y < 940; y += 13)
        painter.drawLine(QPointF(60, y), QPointF(900, y + 4));
    painter.restore();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor("#a78a5f"), 1.2));
    painter.drawRoundedRect(board.adjusted(12, 12, -12, -12), 9, 9);
    painter.setPen(QPen(QColor("#786348"), 1.6));
    for (int row = 0; row < 10; ++row)
        painter.drawLine(QPointF(120, 75 + row * 90), QPointF(840, 75 + row * 90));
    for (int col = 0; col < 9; ++col) {
        const qreal x = 120 + col * 90;
        if (col == 0 || col == 8) {
            painter.drawLine(QPointF(x, 75), QPointF(x, 885));
        } else {
            painter.drawLine(QPointF(x, 75), QPointF(x, 435));
            painter.drawLine(QPointF(x, 525), QPointF(x, 885));
        }
    }
    for (int row : {0, 7}) {
        painter.drawLine(QPointF(390, 75 + row * 90), QPointF(570, 255 + row * 90));
        painter.drawLine(QPointF(570, 75 + row * 90), QPointF(390, 255 + row * 90));
    }
    drawStarMarks(painter);
    QFont river(gameSerifFontFamily());
    river.setPixelSize(34);
    painter.setFont(river);
    painter.setPen(QColor("#80613c"));
    painter.drawText(QRectF(210, 438, 230, 84), Qt::AlignCenter, QStringLiteral("楚 河"));
    painter.drawText(QRectF(520, 438, 230, 84), Qt::AlignCenter, QStringLiteral("漢 界"));
}

void ChessBoard::buildPieceCache()
{
    for (int side = 0; side < 2; ++side) {
        for (int type = 0; type < 7; ++type) {
            auto& sprite = m_pieceCache[side][type];
            sprite = QPixmap(192, 192);
            sprite.setDevicePixelRatio(2.0);
            sprite.fill(Qt::transparent);
            QPainter painter(&sprite);
            painter.setRenderHint(QPainter::Antialiasing);
            const QPointF c(48, 48);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(65, 49, 28, 24));
            painter.drawEllipse(c + QPointF(0, 3), 37, 37);
            QRadialGradient ivory(c - QPointF(9, 11), 58);
            ivory.setColorAt(0, QColor("#fffdf2"));
            ivory.setColorAt(0.65, QColor("#f6e6c7"));
            ivory.setColorAt(1, QColor("#d8bd8c"));
            painter.setBrush(ivory);
            painter.setPen(QPen(QColor("#b49361"), 1.6));
            painter.drawEllipse(c, 36, 36);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor("#c4a77b"), 1));
            painter.drawEllipse(c, 30.5, 30.5);
            QFont font(gameSerifFontFamily());
            font.setPixelSize(39);
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(QColor(side ? "#a0372d" : "#293e34"));
            ChessPieces piece;
            piece.m_emType = static_cast<ChessPieces::m_emTYPE>(type);
            painter.drawText(QRectF(10, 9, 76, 76), Qt::AlignCenter, piece.getnName(side != 0));
        }
    }
}

bool ChessBoard::isMoveAnimating() const
{
    return m_moveAnimation && m_moveAnimation->state() == QAbstractAnimation::Running;
}

void ChessBoard::stopMoveAnimation()
{
    const bool wasRunning = isMoveAnimating();
    if (m_moveAnimation)
        m_moveAnimation->stop();
    m_animatedPiece = -1;
    m_capturedPiece = -1;
    m_moveProgress = 1.0;
    if (wasRunning)
        emit moveAnimationSettled();
}

void ChessBoard::runAfterMoveAnimation(std::function<void()> action)
{
    if (!isMoveAnimating()) {
        QTimer::singleShot(0, this, std::move(action));
        return;
    }
    auto* completion = new QObject(this);
    connect(this, &ChessBoard::moveAnimationSettled, completion,
            [this, completion, action = std::move(action)]() {
        QObject::disconnect(this, nullptr, completion, nullptr);
        completion->deleteLater();
        QTimer::singleShot(0, this, action);
    });
}

void ChessBoard::animateMove(int id, int capturedId, const QPointF& from, const QPointF& to)
{
    stopMoveAnimation();
    if (!isVisible() || !ui->checkAnimations->isChecked())
        return;
    m_animatedPiece = id;
    m_capturedPiece = capturedId;
    m_animationFrom = from;
    m_animationTo = to;
    m_moveProgress = 0.0;
    const qreal distance = QLineF(from, to).length() / m_nD;
    m_moveAnimation->setDuration(qBound(160, int(160 + distance * 14), 260) * m_preferences.animationPercent / 100);
    m_moveAnimation->start();
}

void ChessBoard::setGameDescription(const QString& mode, const QString& sides, const QString& description)
{
    m_noticeTimer->stop();
    ui->labelMode->setText(mode);
    ui->labelSides->setText(sides);
    ui->labelPuzzleDescription->setText(description);
    ui->labelPuzzleDescription->setVisible(!description.isEmpty());
    refreshPanel();
}

void ChessBoard::setAiThinking(bool thinking)
{
    m_aiThinking = thinking;
    refreshPanel();
}

void ChessBoard::refreshPanel()
{
    if (!m_uiReady)
        return;
    QString state = m_bIsRed ? QStringLiteral("red") : QStringLiteral("black");
    QString turn = m_bIsRed ? QStringLiteral("红方走棋") : QStringLiteral("黑方走棋");
    if (!m_bIsOver && ChessRules(m_ChessPieces).isInCheck(m_bIsRed))
        turn += QStringLiteral(" · 将军");
    if (m_bIsOver) {
        state = m_outcome == Outcome::RedWon ? QStringLiteral("red") : QStringLiteral("black");
        turn = m_outcome == Outcome::Draw ? QStringLiteral("双方和棋")
            : m_outcome == Outcome::RedWon ? QStringLiteral("红方获胜") : QStringLiteral("黑方获胜");
    } else if (m_aiThinking) {
        state = QStringLiteral("thinking");
        turn = QStringLiteral("电脑思考中…");
    }
    if (ui->turnLabel->property("state").toString() != state) {
        ui->turnLabel->setProperty("state", state);
        ui->turnLabel->style()->unpolish(ui->turnLabel);
        ui->turnLabel->style()->polish(ui->turnLabel);
    }
    ui->turnLabel->setText(turn);
    QString progress = m_ChessSteps.isEmpty() ? QStringLiteral("准备就绪") : QStringLiteral("对局中");
    const int repetitions = repetitionCount();
    if (repetitions >= 3) progress = QStringLiteral("重复局面");
    if (m_bIsOver) progress = QStringLiteral("对局结束");
    else if (m_aiThinking) progress = QStringLiteral("电脑思考中");
    ui->headerState->setText(progress);
    auto capturedSummary = [this](bool redPieces) {
        int counts[7] = {};
        for (const auto* step : m_ChessSteps) {
            if (step->m_nKillID == -1)
                continue;
            const auto& piece = m_ChessPieces[step->m_nKillID];
            if (piece.m_bRed == redPieces)
                ++counts[piece.m_emType];
        }
        QStringList groups;
        for (int type = 0; type < 7; ++type) {
            if (counts[type] == 0) continue;
            ChessPieces piece;
            piece.m_emType = static_cast<ChessPieces::m_emTYPE>(type);
            groups.append(piece.getnName(redPieces) + QStringLiteral("×%1").arg(counts[type]));
        }
        return groups.isEmpty() ? QStringLiteral("已吃 · 暂无") : QStringLiteral("已吃  ") + groups.join(' ');
    };
    ui->capturedByRed->setText(capturedSummary(false));
    ui->capturedByBlack->setText(capturedSummary(true));
    if (!m_noticeTimer || !m_noticeTimer->isActive()) {
        QString hint = QStringLiteral("请选择%1方棋子并落子").arg(m_bIsRed ? "红" : "黑");
        if (m_bIsOver)
            hint = QStringLiteral("本局已结束，可以重新开始");
        else if (m_aiThinking)
            hint = QStringLiteral("正在计算下一步，可以悔棋或重新开始");
        else if (repetitions >= 3)
            hint = QStringLiteral("局面重复 %1 次，可求和或变招").arg(repetitions);
        else if (isMoveAnimating())
            hint = QStringLiteral("棋子移动中…");
        else if (m_nSelectID != -1)
            hint = QStringLiteral("已选中%1，请选择落点").arg(m_ChessPieces[m_nSelectID].getnName(isRed(m_nSelectID)));
        ui->labelHint->setText(hint);
    }
    ui->labelMoveCount->setText(QStringLiteral("共 %1 步").arg(m_ChessSteps.size()));
    ui->pushButton_back->setEnabled(!m_ChessSteps.isEmpty() && !m_bIsOver && !hasMatchClock());
    refreshMatchClock();
    if (m_resignButton) m_resignButton->setEnabled(!m_bIsOver);
    if (m_drawButton) m_drawButton->setEnabled(!m_bIsOver);
    ui->pushButton_showStep->setChecked(m_bIsShowStep);
    drawTextStep();
}

void ChessBoard::showNotice(const QString& text)
{
    ui->labelHint->setText(text);
    m_noticeTimer->start(1800);
}

void ChessBoard::closeEvent(QCloseEvent* event)
{
    if (!confirmLeaving()) {
        event->ignore();
        return;
    }
    settleMatchClock();
    checkpointSession();
    dismissGameResult();
    m_turnClock.invalidate();
    m_matchTimer->stop();
    stopMoveAnimation();
    pauseGameTimer();
    m_Chessvoice.stop();
    QMainWindow::closeEvent(event);
}

bool ChessBoard::confirmLeaving()
{
    if (m_allowExit || !m_preferences.confirmExit || m_bIsOver || m_ChessSteps.isEmpty()) return true;
    return confirmAction(QStringLiteral("退出对局"), m_sessionId.isEmpty()
        ? QStringLiteral("对局仍在进行，确认离开？当前对局将结束。")
        : QStringLiteral("确认离开？可从主菜单的“继续上局”恢复进度。"));
}

bool ChessBoard::confirmAction(const QString& title, const QString& text)
{
    QMessageBox dialog(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, this);
    dialog.button(QMessageBox::Yes)->setText(QStringLiteral("确认"));
    dialog.button(QMessageBox::No)->setText(QStringLiteral("取消"));
    dialog.setDefaultButton(QMessageBox::No);
    return dialog.exec() == QMessageBox::Yes;
}

void ChessBoard::applyPreferences()
{
    m_preferences = GamePreferences::load();
    m_bIsShowStep = m_preferences.traces;
    const QSignalBlocker animations(ui->checkAnimations);
    ui->checkAnimations->setChecked(m_preferences.animations);
    m_Chessvoice.setVolume(m_preferences.sound ? m_preferences.volume / 100.0 : 0);
    if (!m_preferences.animations) stopMoveAnimation();
    refreshPanel();
    update();
}

void ChessBoard::setPlayerRoles(const QString& redRole, const QString& blackRole)
{
    ui->labelRedRole->setText(redRole);
    ui->labelBlackRole->setText(blackRole);
}

void ChessBoard::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_uiReady) {
        ui->leftPanel->setFixedWidth(qBound(180, width() / 5, 240));
        ui->verticalWidget->setFixedWidth(qBound(210, width() / 5, 236));
        ui->actionRow->setDirection(height() < 650 ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    }
}
