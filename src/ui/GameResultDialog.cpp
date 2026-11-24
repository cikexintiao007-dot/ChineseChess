// SPDX-License-Identifier: GPL-3.0-or-later
#include "GameResultDialog.h"
#include "GameTheme.h"
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

GameResultDialog::GameResultDialog(bool redWon, const QString& winnerRole,
                                 const QString& mode, const QString& elapsed,
                                 int steps, const QPixmap& emblem, QWidget* parent,
                                 const QString& reason, bool drawn)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("gameResultDialog"));
    setWindowTitle(QStringLiteral("对局结束"));
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(380);
    resize(420, 410);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 24, 32, 26);
    layout->setSpacing(9);

    auto* icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("resultEmblem"));
    icon->setFixedSize(88, 88);
    icon->setScaledContents(true);
    icon->setPixmap(emblem);
    layout->addWidget(icon, 0, Qt::AlignHCenter);

    auto* winner = new QLabel(drawn ? QStringLiteral("双方和棋") : redWon ? QStringLiteral("红方获胜") : QStringLiteral("黑方获胜"), this);
    winner->setObjectName(QStringLiteral("resultWinner"));
    winner->setProperty("side", redWon ? "red" : "black");
    winner->setAlignment(Qt::AlignCenter);
    layout->addWidget(winner);

    auto* role = new QLabel(drawn ? QStringLiteral("本局握手言和") : QStringLiteral("%1赢得本局").arg(winnerRole), this);
    role->setObjectName(QStringLiteral("resultRole"));
    role->setAlignment(Qt::AlignCenter);
    layout->addWidget(role);

    if (!reason.isEmpty()) {
        auto* reasonLabel = new QLabel(reason, this);
        reasonLabel->setObjectName(QStringLiteral("resultReason"));
        reasonLabel->setWordWrap(true);
        reasonLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(reasonLabel);
    }

    auto* modeLabel = new QLabel(mode, this);
    modeLabel->setObjectName(QStringLiteral("resultMode"));
    modeLabel->setAlignment(Qt::AlignCenter);
    modeLabel->setWordWrap(true);
    layout->addWidget(modeLabel);
    layout->addSpacing(5);

    auto* stats = new QFrame(this);
    stats->setObjectName(QStringLiteral("resultStats"));
    auto* statsLayout = new QHBoxLayout(stats);
    statsLayout->setContentsMargins(18, 12, 18, 12);
    statsLayout->setSpacing(20);

    auto* timeColumn = new QVBoxLayout;
    auto* timeCaption = new QLabel(QStringLiteral("本局用时"), stats);
    timeCaption->setObjectName(QStringLiteral("resultCaption"));
    timeCaption->setAlignment(Qt::AlignCenter);
    auto* timeValue = new QLabel(elapsed, stats);
    timeValue->setObjectName(QStringLiteral("resultTime"));
    timeValue->setAlignment(Qt::AlignCenter);
    timeColumn->addWidget(timeCaption);
    timeColumn->addWidget(timeValue);
    statsLayout->addLayout(timeColumn, 1);

    auto* stepColumn = new QVBoxLayout;
    auto* stepCaption = new QLabel(QStringLiteral("总步数"), stats);
    stepCaption->setObjectName(QStringLiteral("resultCaption"));
    stepCaption->setAlignment(Qt::AlignCenter);
    auto* stepValue = new QLabel(QStringLiteral("%1 步").arg(steps), stats);
    stepValue->setObjectName(QStringLiteral("resultSteps"));
    stepValue->setAlignment(Qt::AlignCenter);
    stepColumn->addWidget(stepCaption);
    stepColumn->addWidget(stepValue);
    statsLayout->addLayout(stepColumn, 1);
    layout->addWidget(stats);
    layout->addSpacing(5);

    auto* again = new QPushButton(QStringLiteral("再来一局"), this);
    again->setObjectName(QStringLiteral("resultAgain"));
    again->setMinimumHeight(25);
    connect(again, &QPushButton::clicked, this, [this]() { done(PlayAgain); });
    layout->addWidget(again);

    auto* secondary = new QHBoxLayout;
    auto* review = new QPushButton(QStringLiteral("查看棋盘"), this);
    review->setObjectName(QStringLiteral("resultReview"));
    connect(review, &QPushButton::clicked, this, [this]() { done(ViewBoard); });
    secondary->addWidget(review);
    auto* menu = new QPushButton(QStringLiteral("返回主菜单"), this);
    menu->setObjectName(QStringLiteral("resultMenu"));
    connect(menu, &QPushButton::clicked, this, [this]() { done(ReturnToMenu); });
    secondary->addWidget(menu);
    layout->addLayout(secondary);

    // Enter and Escape dismiss the result for inspection without clearing its history.
    again->setAutoDefault(false);
    menu->setAutoDefault(false);
    review->setDefault(true);
    review->setFocus();
    for (auto* button : {again, review, menu})
        button->setCursor(Qt::PointingHandCursor);
}

void GameResultDialog::paintEvent(QPaintEvent* event)
{
    QDialog::paintEvent(event);
    if (m_sceneCache.size() != size() * devicePixelRatioF())
        m_sceneCache = createSceneBackground(size(), devicePixelRatioF());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_sceneCache);
    painter.setPen(QPen(QColor("#b79a71"), 1));
    painter.setBrush(QColor(255, 248, 234, 235));
    painter.drawRoundedRect(QRectF(rect()).adjusted(12.5, 12.5, -12.5, -12.5), 14, 14);
}
