// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#ifndef CHESSBOARD_H
#define CHESSBOARD_H

/***
 *      ┌─┐       ┌─┐ + +
 *   ┌──┘ ┴───────┘ ┴──┐++
 *   │                 │
 *   │       ───       │++ + + +
 *   ███████───███████ │+
 *   │                 │+
 *   │       ─┴─       │
 *   │                 │
 *   └───┐         ┌───┘
 *       │         │
 *       │         │   + +
 *       │         │
 *       │         └──────────────┐
 *       │                        │
 *       │                        ├─┐
 *       │                        ┌─┘
 *       │                        │
 *       └─┐  ┐  ┌───────┬──┐  ┌──┘  + + + +
 *         │ ─┤ ─┤       │ ─┤ ─┤
 *         └──┴──┘       └──┴──┘  + + + +
 *                神兽保佑
 *               代码无BUG!
 */

#include <QMainWindow>
#include <QFrame>
#include <QPainter>
#include <QPoint>
#include <QMouseEvent>
#include <QTimer>
#include <QTime>
#include <QElapsedTimer>
#include <QDialog>
#include <QtGlobal>
#include <QVariantAnimation>
#include <QPixmap>
#include <QPointer>
#include <functional>
#include "core/ChessPieces.h"
#include "app/ChessVoice.h"
#include "core/ChessStep.h"
#include "app/GameSettings.h"
#include "core/ChessRecord.h"
#include <memory>

namespace Ui {
class ChessBoard;
}
class QPushButton;
class QLabel;

class ChessBoard : public QMainWindow
{
    Q_OBJECT

public:
    enum class Outcome { Ongoing, RedWon, BlackWon, Draw };
    Outcome outcome() const { return m_outcome; }
    QString resultReason() const { return m_resultReason; }
    virtual void resign();
    virtual void offerDraw();
    explicit ChessBoard(QWidget *parent = 0);
    ~ChessBoard();

    bool isRed(int id);
    bool isDead(int id);
    void killStone(int id);                                  // 吃子
    void reliveStone(int id);                                // 死者苏生
    void moveStone(int moveid, int row, int col);            // 移动棋子
    bool sameColor(int moveId, int killId);                  // 棋子是否同色
    int getStoneId(int row, int col);
    // 车、炮的功能辅助函数   判断两个点是否在一个直线上面,且返回直线之间的棋子个数
    int  getStoneCountAtLine(int row1, int col1, int row2, int col2);
    void whoWin();                                           // 谁胜谁负
    bool isChecked(QPointF pt, int& row, int& col);           // 是否选中该枚棋子。pt为输入参数; row， col为输出参数
    int relation(int row1, int col1, int row2, int col2);    // 计算选中的棋子的位置和要移动的位置之间的位置关系
    QPointF getRealPoint(QPointF pt);                          // 使mouseMoveEvent取得的坐标同Painter的坐标一致
    bool isGeneral();                                        // 校验将移动后位置是否将死

private:
    bool hongMenFeast();                                     // 鸿门宴：对将
    bool havePieces(int row, int col);                       // 判断某一格子，是否有棋子
    void reset();                                            // 胜负已分，重置
    void startGameTimer();
    void pauseGameTimer();
    void autoStartTimerIfNeeded();
    void buildBoardCache();
    void buildPieceCache();
    void animateMove(int id, int capturedId, const QPointF& from, const QPointF& to);

public:
    //视图相关
    QPointF center(int row, int col);                                    // 象棋的棋盘的坐标转换成界面坐标
    QPointF center(int id);
    virtual void paintEvent(QPaintEvent *);                             // 绘画棋盘
    void drawChessPieces(QPainter& painter, int id);                    // 绘画单个具体的棋子
    void drawLastStep(QPainter &painter, QVector<ChessStep*>& steps);   // 绘制上次移动棋子的起止位置
    void drawTextStep();                                                // 绘制文本棋谱
    void drawStarMarks(QPainter& painter);                              // 绘制星位标记
    void setPerspectiveFlipped(bool flipped);
//    virtual void mousePressEvent(QMouseEvent *);                      // 鼠标点击事件
//    virtual void clickPieces(int checkedID, int& row, int& col);
    bool boardTransform(QPointF& origin, qreal& side) const;            // 计算棋盘绘制和点击的起点与边长

    // 象棋移动的规则[将  士  象  马  车  炮  兵]
    bool canMove(int moveId, int killId, int row, int col);
    bool canMoveJIANG(int moveId, int killId, int row, int col);
    bool canMoveSHI(int moveId, int killId, int row, int col);
    bool canMoveXIANG(int moveId, int killId, int row, int col);
    bool canMoveMA(int moveId, int killId, int row, int col);
    bool canMoveCHE(int moveId, int killId, int row, int col);
    bool canMovePAO(int moveId, int killId, int row, int col);
    bool canMoveBING(int moveId, int killId, int row, int col);
    bool canSelect(int id);
    bool isMoveAnimating() const;
    void setGameDescription(const QString& mode, const QString& sides,
                            const QString& description = QString());
    void setPlayerRoles(const QString& redRole, const QString& blackRole);
    virtual void init();
    virtual ChessRecord gameRecord() const;
    virtual bool resumeRecord(const ChessRecord &record, QString *error = nullptr);
    int repetitionCount() const;
    void enableSessionSaving(const QString &id = {});

    //移动相关
    virtual void mouseReleaseEvent(QMouseEvent *ev);                                        // 鼠标释放事件
    void click(QPointF pt);                                                                  // 点击转换像素
    virtual void clickPieces(int id, int& row, int& col);                                   // 点击选棋
    void trySelectStone(int id);                                                            // 尝试选棋
    void tryMoveStone(int killid, int row, int col);                                        // 尝试移动
    void doMoveStone(int moveid, int killid, int row, int col);                             // 执行移动棋子
    void saveStep(int moveid, int killid, int row, int col, QVector<ChessStep*>& steps);    // 保存步数
    QString textStep(int moveid, int row, int col);                                         // 文本棋谱

    void backOne();                        // 悔棋一子
    void back(ChessStep* step);            // 悔棋到指定步数
    virtual void back();                   // 悔棋

    ChessPieces m_ChessPieces[32];         // 所有棋子
    QVector<ChessStep*> m_ChessSteps;      // 悔棋步数
    ChessVoice m_Chessvoice;               // 下棋音效
    qreal m_nR;                              // 棋子半径
    qreal m_nOffSet;                         // 距离界面的边距
    qreal m_nD;                              // 间距为50px
    int m_nSelectID;                       // 选中棋子[-1:选棋子 || 非-1:走棋子]
    int m_nCheckedID;                      // 将要被击杀的棋子ID
    bool m_bIsRed;                         // 是否是红方回合
    bool m_bIsOver;                        // 是否已经游戏结束
    bool m_bIsShowStep;                    // 是否显示步数
    bool m_bReverseView;                   // 是否翻转视角以便本地玩家
    QString textStepRecord;                // 文本棋谱字符串

signals:
    void toMenu();
    void moveAnimationSettled();

private slots:
    void updateTime();
    void on_pushButton_start_clicked();
    void on_pushButton_restart_clicked();
    void on_pushButton_back_clicked();
    void on_pushButton_showStep_clicked();
    void on_pushButton_toMenu_clicked();

protected:
    virtual void afterMove() {}
    virtual void showGameResult(bool redWon);
    void runAfterMoveAnimation(std::function<void()> action);
    void dismissGameResult();
    void resetGameTimer();
    virtual void refreshPanel();
    void showNotice(const QString& text);
    int elapsedSeconds() const;
    void setElapsedSeconds(int seconds, bool running);
    bool restoreMoves(const QVector<RecordedMove>& moves, int seconds);
    void finishGame(Outcome outcome, const QString& reason);
    virtual bool confirmLeaving();
    bool confirmAction(const QString& title, const QString& text);
    virtual bool humanIsRed() const { return m_bIsRed; }
    void stopMoveAnimation();
    void setAiThinking(bool thinking);
    bool hasMatchClock() const { return m_clockLimitMs > 0; }
    void applyRecordPosition(const ChessRecord &record, int steps, bool restoreResult);
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    Ui::ChessBoard *ui;
    bool         m_bTimerAutoStarted;

private:
    void applyPreferences();
    int remainingTime(bool red) const;
    bool settleMatchClock();
    void startMatchClock();
    void refreshMatchClock();
    void checkpointSession();
    QString m_sessionId;
    int m_clockLimitMs = 0;
    int m_redRemainingMs = 0;
    int m_blackRemainingMs = 0;
    QElapsedTimer m_turnClock;
    QTimer *m_matchTimer = nullptr;
    QLabel *m_redClock = nullptr;
    QLabel *m_blackClock = nullptr;
    GamePreferences m_preferences;
    std::unique_ptr<ChessRecord> m_loadedRecord;
    mutable QByteArray m_repetitionPosition;
    mutable int m_repetitionSteps = -1;
    mutable int m_repetitionCount = 1;
    bool m_allowExit = false;
    QPushButton* m_resignButton = nullptr;
    QPushButton* m_drawButton = nullptr;
    Outcome m_outcome = Outcome::Ongoing;
    QString m_resultReason;
    quint64 m_gameGeneration = 0;
    QPointer<QDialog> m_resultDialog;
    bool m_uiReady = false;
    bool m_aiThinking = false;
    int m_displayedSteps = -1;
    QVariantAnimation* m_moveAnimation = nullptr;
    QTimer* m_noticeTimer = nullptr;
    int m_animatedPiece = -1;
    int m_capturedPiece = -1;
    qreal m_moveProgress = 1.0;
    QPointF m_animationFrom;
    QPointF m_animationTo;
    QPixmap m_boardCache;
    QPixmap m_sceneCache;
    QPixmap m_pieceCache[2][7];
    QTimer*      m_timer;                  // 定时器 每秒更新时间
    QTime*       m_timeRecord;             // 记录时间
    bool         m_bIsStart;               // 记录是否已经开始计时
};

#endif // CHESSBOARD_H
