// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlayerPage.h"
#include "game/PlayerProfiles.h"
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
bool editProfile(QWidget *parent, PlayerProfile &profile)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("playerEditPage"));
    dialog.setWindowTitle(profile.id.isEmpty() ? QStringLiteral("新建玩家") : QStringLiteral("编辑玩家"));
    dialog.setMinimumWidth(350);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *name = new QLineEdit(profile.name, &dialog);
    name->setObjectName(QStringLiteral("playerNickname"));
    name->setMaxLength(24);
    auto *avatar = new QComboBox(&dialog);
    avatar->setObjectName(QStringLiteral("playerAvatar"));
    avatar->addItems(PlayerProfiles::avatars());
    if (!profile.avatar.isEmpty()) avatar->setCurrentText(profile.avatar);
    form->addRow(QStringLiteral("昵称"), name);
    form->addRow(QStringLiteral("棋子头像"), avatar);
    layout->addLayout(form);
    auto *notice = new QLabel(&dialog);
    notice->setWordWrap(true);
    notice->hide();
    layout->addWidget(notice);
    auto *actions = new QDialogButtonBox(&dialog);
    auto *save = actions->addButton(QStringLiteral("保存"), QDialogButtonBox::AcceptRole);
    save->setObjectName(QStringLiteral("playerSaveButton"));
    actions->addButton(QStringLiteral("取消"), QDialogButtonBox::RejectRole);
    layout->addWidget(actions);
    QObject::connect(actions, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(actions, &QDialogButtonBox::accepted, &dialog, [&] {
        PlayerProfile candidate{profile.id, name->text(), avatar->currentText()};
        QString error;
        if (PlayerProfiles::save(candidate, &error)) {
            profile = candidate;
            dialog.accept();
        } else {
            notice->setText(error);
            notice->show();
            name->setFocus();
        }
    });
    name->setFocus();
    return dialog.exec() == QDialog::Accepted;
}
}

bool choosePlayerReplay(QWidget *parent, ChessRecord &record)
{
    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("playerProfilesPage"));
    dialog.setWindowTitle(QStringLiteral("玩家与排行"));
    dialog.resize(650, 510);
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget(&dialog);
    tabs->setObjectName(QStringLiteral("playerTabs"));
    layout->addWidget(tabs);
    auto *ranking = new QWidget(tabs);
    auto *rankLayout = new QVBoxLayout(ranking);
    auto *notice = new QLabel(ranking);
    notice->setObjectName(QStringLiteral("playerRankNotice"));
    notice->setWordWrap(true);
    rankLayout->addWidget(notice);
    auto *table = new QTableWidget(0, 7, ranking);
    table->setObjectName(QStringLiteral("playerStandings"));
    table->setHorizontalHeaderLabels({QStringLiteral("排名"), QStringLiteral("玩家"), QStringLiteral("积分"),
        QStringLiteral("胜"), QStringLiteral("和"), QStringLiteral("负"), QStringLiteral("场次")});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    rankLayout->addWidget(table);
    auto *rankActions = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("新建玩家"), ranking);
    add->setObjectName(QStringLiteral("playerAddButton"));
    auto *edit = new QPushButton(QStringLiteral("编辑档案"), ranking);
    edit->setObjectName(QStringLiteral("playerEditButton"));
    auto *headToHead = new QPushButton(QStringLiteral("查看交手"), ranking);
    headToHead->setObjectName(QStringLiteral("playerHeadToHeadButton"));
    rankActions->addWidget(add);
    rankActions->addWidget(edit);
    rankActions->addWidget(headToHead);
    rankActions->addStretch();
    rankLayout->addLayout(rankActions);
    tabs->addTab(ranking, QStringLiteral("本机积分榜"));

    auto *history = new QWidget(tabs);
    auto *historyLayout = new QVBoxLayout(history);
    auto *filters = new QFormLayout;
    auto *player = new QComboBox(history);
    player->setObjectName(QStringLiteral("headToHeadPlayer"));
    auto *opponent = new QComboBox(history);
    opponent->setObjectName(QStringLiteral("headToHeadOpponent"));
    filters->addRow(QStringLiteral("玩家"), player);
    filters->addRow(QStringLiteral("对手"), opponent);
    historyLayout->addLayout(filters);
    auto *summary = new QLabel(history);
    summary->setObjectName(QStringLiteral("headToHeadSummary"));
    summary->setWordWrap(true);
    historyLayout->addWidget(summary);
    auto *games = new QListWidget(history);
    games->setObjectName(QStringLiteral("headToHeadMatches"));
    games->setAccessibleName(QStringLiteral("交手棋谱"));
    historyLayout->addWidget(games);
    auto *replay = new QPushButton(QStringLiteral("复盘选中对局"), history);
    replay->setObjectName(QStringLiteral("headToHeadReplay"));
    historyLayout->addWidget(replay, 0, Qt::AlignRight);
    tabs->addTab(history, QStringLiteral("交手记录"));
    auto *close = new QPushButton(QStringLiteral("返回大厅"), &dialog);
    QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
    layout->addWidget(close, 0, Qt::AlignRight);

    QVector<PlayerProfile> profiles;
    QVector<PlayerMatch> matches;
    QVector<PlayerStanding> standings;
    const auto refreshHistory = [&] {
        games->clear();
        replay->setEnabled(false);
        const QString playerId = player->currentData().toString();
        const QString opponentId = opponent->currentData().toString();
        if (playerId.isEmpty() || opponentId.isEmpty()) {
            summary->setText(QStringLiteral("至少需要两个玩家档案才能查看交手记录。"));
            return;
        }
        int wins = 0, draws = 0, losses = 0;
        for (const auto &match : matches) {
            const bool red = match.redId == playerId && match.blackId == opponentId;
            if (!red && !(match.blackId == playerId && match.redId == opponentId)) continue;
            QString result;
            if (match.outcome == 3) {
                ++draws;
                result = QStringLiteral("和");
            } else if ((match.outcome == 1) == red) {
                ++wins;
                result = QStringLiteral("胜");
            } else {
                ++losses;
                result = QStringLiteral("负");
            }
            auto *item = new QListWidgetItem(QStringLiteral("%1　执%2　%3")
                .arg(match.finishedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm")),
                     red ? QStringLiteral("红") : QStringLiteral("黑"), result), games);
            item->setData(Qt::UserRole, match.path);
        }
        summary->setText(games->count() ? QStringLiteral("对选中对手：%1 胜 · %2 和 · %3 负").arg(wins).arg(draws).arg(losses)
                                        : QStringLiteral("两位玩家暂无计分交手。"));
        if (games->count()) games->setCurrentRow(0);
    };
    const auto refreshOpponents = [&] {
        const QString previous = opponent->currentData().toString();
        {
            const QSignalBlocker blocked(opponent);
            opponent->clear();
            for (const auto &entry : profiles)
                if (entry.id != player->currentData().toString())
                    opponent->addItem(entry.displayName(), entry.id);
            const int index = opponent->findData(previous);
            if (index >= 0) opponent->setCurrentIndex(index);
        }
        refreshHistory();
    };
    const auto refreshRanking = [&](const QString &selection) {
        QString error;
        const bool loaded = PlayerProfiles::load(profiles, &error);
        if (!loaded) profiles.clear();
        matches = PlayerProfiles::ratedMatches(profiles);
        standings = PlayerProfiles::standings(profiles, matches);
        table->setRowCount(standings.size());
        int selected = 0;
        int rank = 0;
        int lastPoints = -1;
        for (int row = 0; row < standings.size(); ++row) {
            const auto &entry = standings[row];
            if (entry.points() != lastPoints) {
                rank = row + 1;
                lastPoints = entry.points();
            }
            const QStringList columns{entry.games() ? QString::number(rank) : QStringLiteral("—"),
                entry.profile.displayName(), QString::number(entry.points()), QString::number(entry.wins),
                QString::number(entry.draws), QString::number(entry.losses), QString::number(entry.games())};
            for (int col = 0; col < columns.size(); ++col) {
                auto *item = new QTableWidgetItem(columns[col]);
                if (col != 1) item->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, col, item);
            }
            if (entry.profile.id == selection) selected = row;
        }
        add->setEnabled(loaded);
        edit->setEnabled(!standings.isEmpty());
        headToHead->setEnabled(standings.size() > 1);
        if (!standings.isEmpty()) table->selectRow(selected);
        notice->setText(!loaded ? error : profiles.isEmpty() ? QStringLiteral("还没有玩家档案。先新建两位玩家，再到本地双人对弈选择身份。")
            : QStringLiteral("本地双人对局双方均使用档案，且至少走一步才计分。胜 3 分，和 1 分，负 0 分，同分并列。"));
        {
            const QSignalBlocker blocked(player);
            player->clear();
            for (const auto &entry : profiles) player->addItem(entry.displayName(), entry.id);
        }
        refreshOpponents();
    };
    QObject::connect(add, &QPushButton::clicked, &dialog, [&] {
        PlayerProfile profile;
        if (editProfile(&dialog, profile)) refreshRanking(profile.id);
    });
    QObject::connect(edit, &QPushButton::clicked, &dialog, [&] {
        const int row = table->currentRow();
        if (row < 0) return;
        auto profile = standings.at(row).profile;
        if (editProfile(&dialog, profile)) refreshRanking(profile.id);
    });
    QObject::connect(headToHead, &QPushButton::clicked, &dialog, [&] {
        const int row = table->currentRow();
        if (row < 0) return;
        player->setCurrentIndex(player->findData(standings.at(row).profile.id));
        tabs->setCurrentWidget(history);
    });
    QObject::connect(player, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, refreshOpponents);
    QObject::connect(opponent, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, refreshHistory);
    QObject::connect(games, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem *item) {
        replay->setEnabled(item != nullptr);
    });
    const auto openMatch = [&] {
        if (!games->currentItem()) return;
        QString error;
        if (ChessRecord::load(games->currentItem()->data(Qt::UserRole).toString(), record, &error))
            dialog.accept();
        else summary->setText(error);
    };
    QObject::connect(replay, &QPushButton::clicked, &dialog, openMatch);
    QObject::connect(games, &QListWidget::itemDoubleClicked, &dialog, openMatch);
    refreshRanking({});
    return dialog.exec() == QDialog::Accepted;
}
