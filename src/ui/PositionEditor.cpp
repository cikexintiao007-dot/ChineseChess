// SPDX-License-Identifier: GPL-3.0-or-later
#include "PositionEditor.h"
#include "ChessBoard.h"
#include "core/ChessRules.h"
#include "GamePages.h"
#include "GameTheme.h"
#include "ui_ChessBoard.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <memory>

namespace {
class SetupBoard : public ChessBoard
{
public:
    explicit SetupBoard(QWidget *parent) : ChessBoard(parent)
    {
        setWindowFlags(Qt::Widget);
        setMinimumSize(280, 316);
        ui->gameHeader->hide();
        ui->leftPanel->hide();
        ui->verticalWidget->hide();
        ui->bodyLayout->setContentsMargins(0, 0, 0, 0);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(QStringLiteral("摆棋棋盘"));
        setObjectName(QStringLiteral("setupBoard"));
    }

    int brush = 7 + ChessPieces::CHE;
    std::function<void(const QString &)> feedback;

    void clickPieces(int id, int &row, int &col) override
    {
        cursor = QPoint(col, row);
        if (brush < 0) {
            killStone(id);
        } else {
            const bool red = brush >= 7;
            const int type = brush % 7;
            if (id >= 0 && m_ChessPieces[id].m_bRed == red && m_ChessPieces[id].m_emType == type)
                return;
            int available = -1;
            for (const auto &piece : m_ChessPieces) {
                if (piece.m_bDead && piece.m_bRed == red && piece.m_emType == type) {
                    available = piece.m_nID;
                    break;
                }
            }
            if (available < 0) {
                feedback(QStringLiteral("该方此类棋子已用完，可先移除一枚。"));
                return;
            }
            killStone(id);
            auto &piece = m_ChessPieces[available];
            piece.m_bDead = false;
            piece.m_nRow = row;
            piece.m_nCol = col;
        }
        feedback(QString());
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ChessBoard::paintEvent(event);
        QPointF origin;
        qreal side;
        if (!hasFocus() || !boardTransform(origin, side)) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#456643"), 2));
        const qreal radius = 41 * side / 960;
        painter.drawEllipse(origin + center(cursor.y(), cursor.x()) * (side / 960), radius, radius);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        switch (event->key()) {
        case Qt::Key_Left: cursor.rx() = qMax(0, cursor.x() - 1); break;
        case Qt::Key_Right: cursor.rx() = qMin(8, cursor.x() + 1); break;
        case Qt::Key_Up: cursor.ry() = qMax(0, cursor.y() - 1); break;
        case Qt::Key_Down: cursor.ry() = qMin(9, cursor.y() + 1); break;
        case Qt::Key_Space:
        case Qt::Key_Delete: {
            int row = cursor.y(), col = cursor.x();
            const int previous = brush;
            if (event->key() == Qt::Key_Delete) brush = -1;
            clickPieces(getStoneId(row, col), row, col);
            brush = previous;
            break;
        }
        default: ChessBoard::keyPressEvent(event); return;
        }
        event->accept();
        update();
    }

    void focusInEvent(QFocusEvent *event) override { ChessBoard::focusInEvent(event); update(); }
    void focusOutEvent(QFocusEvent *event) override { ChessBoard::focusOutEvent(event); update(); }

private:
    QPoint cursor{4, 4};
};

QString setupError(const ChessRecord &record)
{
    const auto &pieces = record.initial;
    if (pieces[4].m_bDead || pieces[20].m_bDead)
        return QStringLiteral("请各放置一枚红帅和黑将。");
    for (const auto &piece : pieces) {
        if (piece.m_bDead) continue;
        const int row = piece.m_bRed ? 9 - piece.m_nRow : piece.m_nRow;
        const int col = piece.m_nCol;
        switch (piece.m_emType) {
        case ChessPieces::JIANG:
            if (row > 2 || col < 3 || col > 5)
                return QStringLiteral("将帅必须在各自的九宫内。");
            break;
        case ChessPieces::SHI:
            if (!((row == 1 && col == 4) || ((row == 0 || row == 2) && (col == 3 || col == 5))))
                return QStringLiteral("士和仕只能摆在各自九宫的四角或中心。");
            break;
        case ChessPieces::XIANG:
            if (!((row == 2 && (col == 0 || col == 4 || col == 8))
                  || ((row == 0 || row == 4) && (col == 2 || col == 6))))
                return QStringLiteral("象和相只能摆在己方可到达的象位。");
            break;
        case ChessPieces::BING:
            if (row < 3 || (row < 5 && col % 2 != 0))
                return QStringLiteral("兵卒不能摆在起始线后方，过河前须位于原来的行棋路线。");
            break;
        default: break;
        }
    }
    const ChessRules rules(pieces.data());
    if (pieces[4].m_nCol == pieces[20].m_nCol
        && rules.getStoneCountAtLine(pieces[4].m_nRow, pieces[4].m_nCol, pieces[20].m_nRow, pieces[20].m_nCol) == 0)
        return QStringLiteral("将帅不能在同一列无子阻隔时照面。");
    if (rules.isInCheck(!record.redFirst))
        return QStringLiteral("后行方已被将军，请调整棋子或更改先手。");
    if (!rules.hasLegalMove(record.redFirst))
        return QStringLiteral("先行方没有合法走法，请调整局面后再开局。");
    QString error;
    record.validate(&error);
    return error;
}
}

ChessBoard *editStartingPosition(QWidget *parent)
{
    QDialog page(parent);
    page.setObjectName(QStringLiteral("positionEditor"));
    page.setWindowTitle(QStringLiteral("自由摆棋"));
    page.resize(810, 560);
    auto *layout = new QVBoxLayout(&page);
    layout->setContentsMargins(20, 16, 20, 16);
    auto *heading = new QLabel(QStringLiteral("自由摆棋"), &page);
    heading->setObjectName(QStringLiteral("menuTitle"));
    layout->addWidget(heading);
    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    auto *board = new SetupBoard(&page);
    columns->addWidget(board, 1);
    auto *card = new QFrame(&page);
    card->setObjectName(QStringLiteral("controlCard"));
    auto *tools = new QVBoxLayout(card);
    tools->setContentsMargins(14, 14, 14, 14);
    auto *instructions = new QLabel(QStringLiteral("选择棋子，再点击交叉点。\n方向键移动光标，空格落子，Delete 移除。"), card);
    instructions->setWordWrap(true);
    tools->addWidget(instructions);
    auto *palette = new QGridLayout;
    auto *brushes = new QButtonGroup(&page);
    const QString names[] = {QStringLiteral("将士象馬車炮卒"), QStringLiteral("帥仕相馬車炮兵")};
    for (int side = 0; side < 2; ++side) {
        for (int type = 0; type < 7; ++type) {
            const int brush = side * 7 + type;
            const QString name(names[side].at(type));
            auto *button = new QPushButton(name, card);
            button->setProperty("pieceSide", side == 0 ? "black" : "red");
            button->setAccessibleName((side == 0 ? QStringLiteral("黑方") : QStringLiteral("红方")) + name);
            button->setObjectName(QStringLiteral("setupPiece%1").arg(brush));
            button->setCheckable(true);
            button->setChecked(brush == board->brush);
            brushes->addButton(button, brush);
            palette->addWidget(button, side, type);
            QObject::connect(button, &QPushButton::clicked, board, [board, brush] { board->brush = brush; });
        }
    }
    tools->addLayout(palette);
    auto *editActions = new QHBoxLayout;
    auto *erase = new QPushButton(QStringLiteral("移除"), card);
    erase->setObjectName(QStringLiteral("setupErase"));
    erase->setCheckable(true);
    brushes->addButton(erase);
    auto *clear = new QPushButton(QStringLiteral("清空"), card);
    auto *reset = new QPushButton(QStringLiteral("标准开局"), card);
    for (auto *button : {erase, clear, reset}) editActions->addWidget(button);
    tools->addLayout(editActions);
    auto *turn = new QComboBox(card);
    turn->setObjectName(QStringLiteral("setupTurn"));
    turn->addItems({QStringLiteral("红方先行"), QStringLiteral("黑方先行")});
    turn->setAccessibleName(QStringLiteral("先手"));
    tools->addWidget(turn);
    auto *mode = new QComboBox(card);
    mode->setObjectName(QStringLiteral("setupMode"));
    mode->addItems({QStringLiteral("本地双人"), QStringLiteral("人机 · 你执红"), QStringLiteral("人机 · 你执黑")});
    mode->setAccessibleName(QStringLiteral("对弈方式"));
    tools->addWidget(mode);
    auto *status = new QLabel(card);
    status->setObjectName(QStringLiteral("setupStatus"));
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setMinimumHeight(status->fontMetrics().height() * 3);
    tools->addWidget(status);
    tools->addStretch();
    columns->addWidget(card);
    layout->addLayout(columns, 1);
    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("返回"), &page);
    auto *save = new QPushButton(QStringLiteral("保存局面…"), &page);
    auto *start = new QPushButton(QStringLiteral("开始对弈"), &page);
    start->setObjectName(QStringLiteral("setupStart"));
    actions->addWidget(back);
    actions->addStretch();
    actions->addWidget(save);
    actions->addWidget(start);
    layout->addLayout(actions);
    for (auto *button : page.findChildren<QPushButton *>()) button->setAutoDefault(false);

    const auto makeRecord = [&] {
        ChessRecord record = board->gameRecord();
        record.redFirst = turn->currentIndex() == 0;
        record.computer = mode->currentIndex() != 0;
        record.aiRed = mode->currentIndex() == 2;
        record.flipped = record.computer && record.aiRed;
        record.mode = QStringLiteral("自定义局面 · %1").arg(mode->currentText());
        record.redRole = record.computer && record.aiRed ? QStringLiteral("电脑") : QStringLiteral("玩家");
        record.blackRole = record.computer && !record.aiRed ? QStringLiteral("电脑") : QStringLiteral("玩家");
        return record;
    };
    const auto refresh = [&](const QString &message = {}) {
        const QString error = setupError(makeRecord());
        const bool valid = error.isEmpty();
        status->setText(!message.isEmpty() ? message : valid ? QStringLiteral("局面有效，可开局或保存。") : error);
        start->setEnabled(valid);
        save->setEnabled(valid);
        board->update();
    };
    board->feedback = refresh;
    QObject::connect(erase, &QPushButton::clicked, board, [board] { board->brush = -1; });
    QObject::connect(clear, &QPushButton::clicked, &page, [&] {
        for (auto &piece : board->m_ChessPieces) piece.m_bDead = true;
        refresh();
    });
    QObject::connect(reset, &QPushButton::clicked, &page, [&] {
        for (int id = 0; id < 32; ++id) board->m_ChessPieces[id].init(id);
        turn->setCurrentIndex(0);
        refresh();
    });
    QObject::connect(turn, QOverload<int>::of(&QComboBox::currentIndexChanged), &page, [&] { refresh(); });
    QObject::connect(back, &QPushButton::clicked, &page, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &page, [&] {
        QFileDialog dialog(&page, QStringLiteral("保存自定义局面"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            QStringLiteral("象棋棋谱 (*.cchess)"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setDefaultSuffix(QStringLiteral("cchess"));
        dialog.selectFile(QStringLiteral("自定义局面.cchess"));
        if (dialog.exec() != QDialog::Accepted) return;
        const QString path = dialog.selectedFiles().value(0);
        QString error;
        if (!makeRecord().save(path, &error)) {
            refresh(error);
            return;
        }
        rememberChessRecord(path);
        refresh(QStringLiteral("局面已保存，可在棋谱库打开。"));
    });
    std::unique_ptr<ChessBoard> game;
    QObject::connect(start, &QPushButton::clicked, &page, [&] {
        const auto record = makeRecord();
        if (const QString error = setupError(record); !error.isEmpty()) {
            refresh(error);
            return;
        }
        if (record.computer) game = std::make_unique<MachineGame>();
        else game = std::make_unique<ChessBoard>();
        QString error;
        if (!game->resumeRecord(record, &error)) {
            game.reset();
            refresh(error);
            return;
        }
        game->setGameDescription(record.mode,
            mode->currentIndex() == 0 ? QStringLiteral("红黑双方轮流操作")
            : record.aiRed ? QStringLiteral("你执黑棋 · 电脑执红") : QStringLiteral("你执红棋 · 电脑执黑"));
        page.accept();
    });
    refresh();
    page.exec();
    return game.release();
}
