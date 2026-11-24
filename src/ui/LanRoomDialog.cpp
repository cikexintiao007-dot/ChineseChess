// SPDX-License-Identifier: GPL-3.0-or-later
#include "LanRoomDialog.h"
#include "network/LanRoom.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QApplication>
#include <QClipboard>
#include <QSettings>

LanRoomDialog::LanRoomDialog(QWidget* parent) : QDialog(parent), m_room(new LanRoom(this))
{
    setWindowTitle(QStringLiteral("局域网联机"));
    setMinimumWidth(460);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);
    auto* intro = new QLabel(QStringLiteral("两台电脑连接同一个 Wi-Fi 或局域网\n房主创建房间，另一位玩家输入六位房间号加入"), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* tabs = new QTabWidget(this);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { background: #fff7e9; border: 1px solid #bea17a; }"
        "QTabWidget > QStackedWidget > QWidget { background: #fff7e9; }"
        "QTabBar::tab { background: #e9d8bb; color: #5a301b; padding: 8px 20px; border: 1px solid #bea17a; }"
        "QTabBar::tab:selected { background: #fff7e9; border-bottom-color: #fff7e9; }"));
    layout->addWidget(tabs);
    auto* create = new QWidget(tabs);
    auto* hostLayout = new QVBoxLayout(create);
    auto* codeLabel = new QLabel(QStringLiteral("等待创建"), create);
    codeLabel->setObjectName(QStringLiteral("roomCodeLabel"));
    codeLabel->setAlignment(Qt::AlignCenter);
    codeLabel->setStyleSheet(QStringLiteral("font-size: 34px; font-weight: 700; color: #8f2e20; padding: 16px;"));
    codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hostLayout->addWidget(codeLabel);
    auto* createButton = new QPushButton(QStringLiteral("创建房间"), create);
    createButton->setObjectName(QStringLiteral("createRoomButton"));
    hostLayout->addWidget(createButton);
    auto* copy = new QPushButton(QStringLiteral("复制房间号"), create);
    copy->setEnabled(false);
    hostLayout->addWidget(copy);
    auto* addresses = new QLabel(QStringLiteral("本机地址  %1").arg(LanRoom::localAddresses().join(QStringLiteral(" / "))), create);
    addresses->setWordWrap(true);
    addresses->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hostLayout->addWidget(addresses);
    tabs->addTab(create, QStringLiteral("创建房间"));
    auto* join = new QWidget(tabs);
    auto* joinLayout = new QVBoxLayout(join);
    auto* code = new QLineEdit(join);
    code->setObjectName(QStringLiteral("roomCodeInput"));
    code->setPlaceholderText(QStringLiteral("输入六位数字房间号"));
    code->setMaxLength(6);
    code->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{6}")), code));
    joinLayout->addWidget(new QLabel(QStringLiteral("房间号"), join));
    code->setText(QSettings().value("network/lastRoom").toString());
    joinLayout->addWidget(code);
    auto* hint = new QLineEdit(join);
    joinLayout->addWidget(new QLabel(QStringLiteral("房主 IPv4 地址（可选）"), join));
    hint->setText(QSettings().value("network/lastHost").toString());
    hint->setPlaceholderText(QStringLiteral("房主 IPv4 地址（可选，自动搜索失败时填写）"));
    joinLayout->addWidget(hint);
    auto* joinButton = new QPushButton(QStringLiteral("加入房间"), join);
    joinButton->setObjectName(QStringLiteral("joinRoomButton"));
    joinLayout->addWidget(joinButton);
    joinLayout->addStretch();
    tabs->addTab(join, QStringLiteral("加入房间"));
    auto* status = new QLabel(QStringLiteral("房主执红先行，加入者执黑"), this);
    status->setObjectName(QStringLiteral("roomStatus"));
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* cancel = new QPushButton(QStringLiteral("返回"), this);
    layout->addWidget(cancel);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::rejected, m_room, &LanRoom::closeRoom);
    connect(createButton, &QPushButton::clicked, this, [=] {
        if (m_room->createRoom()) {
            codeLabel->setText(m_room->roomCode());
            status->setText(QStringLiteral("等待另一位玩家加入… 请把房间号发给对方"));
            copy->setEnabled(true);
            createButton->setEnabled(false);
            tabs->setTabEnabled(1, false);
        }
    });
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(m_room->roomCode()); });
    connect(joinButton, &QPushButton::clicked, this, [=] {
        if (!code->hasAcceptableInput()) {
            status->setText(QStringLiteral("请输入完整的六位数字房间号"));
            return;
        }
        joinButton->setEnabled(false);
        tabs->setTabEnabled(0, false);
        m_room->joinRoom(code->text(), hint->text().trimmed());
    });
    connect(code, &QLineEdit::returnPressed, joinButton, &QPushButton::click);
    connect(m_room, &LanRoom::statusChanged, status, &QLabel::setText);
    connect(m_room, &LanRoom::failed, this, [=](const QString& error) {
        status->setText(error);
        joinButton->setEnabled(true);
        tabs->setTabEnabled(0, true);
    });
    connect(m_room, &LanRoom::connected, this, &QDialog::accept);
    connect(m_room, &LanRoom::connected, this, [this] {
        if (m_room->isHost()) return;
        QSettings settings;
        settings.setValue("network/lastRoom", m_room->roomCode());
        settings.setValue("network/lastHost", m_room->hostAddress());
    });
}

LanRoom* LanRoomDialog::takeRoom()
{
    auto* room = m_room;
    room->disconnect(this);
    room->setParent(nullptr);
    m_room = nullptr;
    return room;
}
