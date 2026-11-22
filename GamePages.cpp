// SPDX-License-Identifier: GPL-3.0-or-later
#include "GamePages.h"
#include "GameSession.h"
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
constexpr auto recentKey = "records/recentFiles";

QVBoxLayout *pageLayout(QDialog &page, const QString &title)
{
    page.setWindowTitle(title);
    page.resize(600, 420);
    auto *layout = new QVBoxLayout(&page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    auto *heading = new QLabel(title, &page);
    heading->setObjectName(QStringLiteral("menuTitle"));
    layout->addWidget(heading);
    return layout;
}

QLabel *pageText(QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return label;
}
}

void rememberChessRecord(const QString &path)
{
    const QFileInfo file(path);
    if (!file.isFile()) return;
    QSettings settings;
    auto recent = settings.value(recentKey).toStringList();
    const QString absolutePath = file.canonicalFilePath();
    if (absolutePath.isEmpty()) return;
    recent.removeAll(absolutePath);
    recent.prepend(absolutePath);
    settings.setValue(recentKey, recent.mid(0, 30));
}

bool chooseChessRecord(QWidget *parent, ChessRecord &record, QString *sessionId)
{
    QDialog page(parent);
    page.setObjectName(QStringLiteral("recordLibraryPage"));
    auto *layout = pageLayout(page, QStringLiteral("棋谱库"));
    auto *filter = new QComboBox(&page);
    filter->setObjectName(QStringLiteral("recordLibraryFilter"));
    filter->setAccessibleName(QStringLiteral("棋谱分类"));
    filter->addItems({QStringLiteral("最近棋谱"), QStringLiteral("已结束对局"), QStringLiteral("未完成对局")});
    if (!sessionId) filter->removeItem(2);
    layout->addWidget(filter);
    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    auto *list = new QListWidget(&page);
    list->setAccessibleName(QStringLiteral("最近保存或打开的棋谱"));
    list->setObjectName(QStringLiteral("recordLibraryList"));
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    columns->addWidget(list, 1);
    auto *card = new QFrame(&page);
    card->setObjectName(QStringLiteral("controlCard"));
    auto *details = new QVBoxLayout(card);
    details->setContentsMargins(16, 14, 16, 14);
    auto *summary = pageText(card);
    summary->setObjectName(QStringLiteral("recordLibrarySummary"));
    details->addWidget(summary, 1);
    columns->addWidget(card, 1);
    layout->addLayout(columns, 1);
    auto *actions = new QHBoxLayout;
    auto *browse = new QPushButton(QStringLiteral("浏览文件…"), &page);
    auto *close = new QPushButton(QStringLiteral("返回"), &page);
    auto *open = new QPushButton(QStringLiteral("开始复盘"), &page);
    open->setObjectName(QStringLiteral("recordLibraryOpen"));
    actions->addWidget(browse);
    actions->addStretch();
    actions->addWidget(close);
    actions->addWidget(open);
    layout->addLayout(actions);
    open->setEnabled(false);
    const auto preview = [&](const QString &path) {
        QString error;
        const bool valid = ChessRecord::load(path, record, &error);
        open->setEnabled(valid);
        if (!valid) {
            summary->setText(QStringLiteral("无法读取棋谱\n\n%1").arg(error));
            return false;
        }
        const QStringList outcomes{QStringLiteral("未结束"), QStringLiteral("红方获胜"),
                                   QStringLiteral("黑方获胜"), QStringLiteral("和棋")};
        summary->setText(QStringLiteral("%1\n\n%2 · %3 步\n%4 分 %5 秒\n%6")
            .arg(record.mode.isEmpty() ? QStringLiteral("中国象棋") : record.mode, outcomes.at(record.outcome))
            .arg(record.moves.size()).arg(record.seconds / 60).arg(record.seconds % 60).arg(record.reason));
        return true;
    };
    QObject::connect(list, &QListWidget::currentItemChanged, &page, [&](QListWidgetItem *item) {
        if (item) preview(item->data(Qt::UserRole).toString());
    });
    const auto openSelected = [&] {
        if (auto *item = list->currentItem()) {
            const QString path = item->data(Qt::UserRole).toString();
            if (preview(path)) {
                if (sessionId && filter->currentIndex() == 2)
                    *sessionId = QFileInfo(path).completeBaseName();
                else
                    rememberChessRecord(path);
                page.accept();
            }
        }
    };
    QObject::connect(open, &QPushButton::clicked, &page, openSelected);
    QObject::connect(list, &QListWidget::itemActivated, &page, [&](QListWidgetItem *) { openSelected(); });
    QObject::connect(browse, &QPushButton::clicked, &page, [&] {
        const QString path = QFileDialog::getOpenFileName(&page, QStringLiteral("打开棋谱"), QString(),
            QStringLiteral("象棋棋谱 (*.cchess);;所有文件 (*)"));
        if (path.isEmpty()) return;
        if (preview(path)) {
            rememberChessRecord(path);
            page.accept();
        } else {
            open->setEnabled(false);
            list->setCurrentRow(-1);
        }
    });
    QObject::connect(close, &QPushButton::clicked, &page, &QDialog::reject);
    const auto populate = [&] {
        list->clear();
        open->setEnabled(false);
        const int category = filter->currentIndex();
        open->setText(category == 2 ? QStringLiteral("继续对局") : QStringLiteral("开始复盘"));
        const auto paths = category == 0 ? QSettings().value(recentKey).toStringList()
                                         : GameSession::files(category == 1);
        for (const auto &path : paths) {
            const QFileInfo file(path);
            if (!file.isFile()) continue;
            const QString title = category == 0 ? file.fileName()
                : file.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            auto *item = new QListWidgetItem(title, list);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
        }
        if (list->count()) list->setCurrentRow(0);
        else summary->setText(category == 0 ? QStringLiteral("暂无最近棋谱。可浏览已有文件。")
                                            : QStringLiteral("此分类暂无对局。"));
    };
    QObject::connect(filter, QOverload<int>::of(&QComboBox::currentIndexChanged), &page, populate);
    populate();
    if (list->count()) list->setFocus();
    else browse->setFocus();
    return page.exec() == QDialog::Accepted;
}

bool configureMatch(QWidget *parent, bool computer, ChessRecord &record)
{
    QDialog page(parent);
    page.setObjectName(QStringLiteral("matchSetupPage"));
    auto *layout = pageLayout(page, computer ? QStringLiteral("人机对弈") : QStringLiteral("本地双人对弈"));
    page.resize(460, computer ? 320 : 250);
    auto *card = new QFrame(&page);
    card->setObjectName(QStringLiteral("controlCard"));
    auto *form = new QFormLayout(card);
    form->setContentsMargins(16, 16, 16, 16);
    auto *time = new QComboBox(card);
    time->setObjectName(QStringLiteral("matchTimeControl"));
    time->addItem(QStringLiteral("不限时"), 0);
    for (int minutes : {5, 10, 15})
        time->addItem(QStringLiteral("每方 %1 分钟").arg(minutes), minutes * 60000);
    form->addRow(QStringLiteral("对局时限"), time);
    auto *side = new QComboBox(card);
    auto *level = new QComboBox(card);
    if (computer) {
        side->addItems({QStringLiteral("红方（先行）"), QStringLiteral("黑方（后行）")});
        level->addItems({QStringLiteral("简单"), QStringLiteral("中等"), QStringLiteral("困难")});
        level->setCurrentIndex(1);
        form->addRow(QStringLiteral("你执棋方"), side);
        form->addRow(QStringLiteral("AI 难度"), level);
    } else {
        side->hide();
        level->hide();
    }
    auto *hint = new QLabel(QStringLiteral("限时对局在落子后切换棋钟，超时判负，期间关闭悔棋。"), card);
    hint->setWordWrap(true);
    form->addRow(hint);
    layout->addWidget(card, 1);
    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("返回"), &page);
    auto *start = new QPushButton(QStringLiteral("开始对弈"), &page);
    start->setObjectName(QStringLiteral("matchSetupStart"));
    start->setDefault(true);
    actions->addStretch();
    actions->addWidget(back);
    actions->addWidget(start);
    layout->addLayout(actions);
    QObject::connect(back, &QPushButton::clicked, &page, &QDialog::reject);
    QObject::connect(start, &QPushButton::clicked, &page, &QDialog::accept);
    time->setFocus();
    if (page.exec() != QDialog::Accepted) return false;
    record.computer = computer;
    record.aiRed = computer && side->currentIndex() == 1;
    record.flipped = record.aiRed;
    record.difficulty = computer ? level->currentIndex() : 1;
    record.clockLimitMs = time->currentData().toInt();
    record.redRemainingMs = record.blackRemainingMs = record.clockLimitMs;
    record.mode = computer ? QStringLiteral("人机对弈 · %1").arg(level->currentText()) : QStringLiteral("本地双人对弈");
    record.redRole = record.aiRed ? QStringLiteral("电脑") : QStringLiteral("玩家");
    record.blackRole = computer && !record.aiRed ? QStringLiteral("电脑") : QStringLiteral("玩家");
    return true;
}

bool choosePuzzle(QWidget *parent, PuzzleConfig &puzzle, AIDifficulty &difficulty)
{
    const auto puzzles = getBuiltinPuzzles();
    if (puzzles.isEmpty()) return false;
    QDialog page(parent);
    page.setObjectName(QStringLiteral("puzzleSelectionPage"));
    auto *layout = pageLayout(page, QStringLiteral("残局挑战"));
    auto *columns = new QHBoxLayout;
    columns->setSpacing(14);
    auto *list = new QListWidget(&page);
    list->setAccessibleName(QStringLiteral("残局题目"));
    for (const auto &entry : puzzles) list->addItem(entry.name);
    columns->addWidget(list, 1);
    auto *card = new QFrame(&page);
    card->setObjectName(QStringLiteral("controlCard"));
    auto *details = new QVBoxLayout(card);
    details->setContentsMargins(16, 14, 16, 14);
    auto *summary = pageText(card);
    auto *label = new QLabel(QStringLiteral("AI 难度"), card);
    auto *level = new QComboBox(card);
    level->addItems({QStringLiteral("简单"), QStringLiteral("中等"), QStringLiteral("困难")});
    level->setCurrentIndex(1);
    level->setAccessibleName(label->text());
    label->setBuddy(level);
    details->addWidget(summary, 1);
    details->addWidget(label);
    details->addWidget(level);
    columns->addWidget(card, 1);
    layout->addLayout(columns, 1);
    auto *actions = new QHBoxLayout;
    auto *back = new QPushButton(QStringLiteral("返回"), &page);
    auto *start = new QPushButton(QStringLiteral("开始挑战"), &page);
    start->setObjectName(QStringLiteral("startPuzzleButton"));
    actions->addStretch();
    actions->addWidget(back);
    actions->addWidget(start);
    layout->addLayout(actions);
    start->setDefault(true);
    QObject::connect(list, &QListWidget::currentRowChanged, &page, [&](int row) {
        start->setEnabled(row >= 0);
        if (row < 0) return;
        const auto &entry = puzzles.at(row);
        summary->setText(QStringLiteral("%1\n\n%2\n\n你执%3 · %4先行")
            .arg(entry.name, entry.description, entry.playerIsRed ? QStringLiteral("红") : QStringLiteral("黑"),
                 entry.redFirst ? QStringLiteral("红方") : QStringLiteral("黑方")));
    });
    QObject::connect(start, &QPushButton::clicked, &page, &QDialog::accept);
    QObject::connect(back, &QPushButton::clicked, &page, &QDialog::reject);
    list->setCurrentRow(0);
    list->setFocus();
    if (page.exec() != QDialog::Accepted || list->currentRow() < 0) return false;
    puzzle = puzzles.at(list->currentRow());
    difficulty = static_cast<AIDifficulty>(level->currentIndex());
    return true;
}

void showChessGuide(QWidget *parent)
{
    QDialog page(parent);
    page.setObjectName(QStringLiteral("chessGuidePage"));
    auto *layout = pageLayout(page, QStringLiteral("象棋指南"));
    auto *tabs = new QTabWidget(&page);
    const auto addChapter = [tabs](const QString &title, const QString &html) {
        auto *text = new QTextBrowser(tabs);
        text->setStyleSheet(QStringLiteral("QTextBrowser { background: #fff7e9; color: #4c2f1e; padding: 12px; }"));
        text->setHtml(html);
        tabs->addTab(text, title);
    };
    addChapter(QStringLiteral("棋子走法"), QStringLiteral(
        "<p><b>车</b> 沿横线或竖线走，中间不能隔子。</p>"
        "<p><b>马</b> 走日字，第一步直向相邻格有棋子时会蹩马腿。</p>"
        "<p><b>炮</b> 不吃子时走直线且不能隔子，吃子时恰好隔一枚棋子。</p>"
        "<p><b>象／相</b> 斜走两格，不能过河，中点有棋子时不能通过。</p>"
        "<p><b>士／仕</b> 在九宫内斜走一格。<b>将／帅</b> 在九宫内直走一格。</p>"
        "<p><b>卒／兵</b> 每次前进一格，过河后也可左右走一格，始终不能后退。</p>"));
    addChapter(QStringLiteral("对局规则"), QStringLiteral(
        "<p>标准开局由红方先行，双方轮流走一枚棋子。</p>"
        "<p>被将军时必须应将。走完后己方将帅必须安全，将帅也不能在同一列无子阻隔时照面。</p>"
        "<p>轮到一方时无合法着法，若正被将军则为将死，否则为困毙，本程序均判该方负。</p>"
        "<p>同一局面连同行棋方出现至少三次时，游戏提示变招或求和。双方可以协商结束本局。</p>"));
    addChapter(QStringLiteral("游戏操作"), QStringLiteral(
        "<p><b>落子</b> 先选己方棋子，再选落点。设置中可启用合法落点和走子音效。</p>"
        "<p><b>联机</b> 连接同一局域网，一人创建房间，另一人输入六位房间号。</p>"
        "<p><b>复盘</b> 对局中可保存棋谱。主菜单进入棋谱库后可浏览最近文件，复盘时按左右方向键切换步骤。</p>"));
    layout->addWidget(tabs, 1);
    auto *close = new QPushButton(QStringLiteral("返回"), &page);
    QObject::connect(close, &QPushButton::clicked, &page, &QDialog::reject);
    layout->addWidget(close, 0, Qt::AlignRight);
    page.exec();
}
