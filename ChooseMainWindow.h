// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#ifndef CHOOSEMAINWINDOW_H
#define CHOOSEMAINWINDOW_H

#include <QWidget>
#include "ChessBoard.h"
#include "MachineGame.h"
#include "PuzzleGame.h"

class ChooseMainWindow : public QDialog
{
    Q_OBJECT
public:
    explicit ChooseMainWindow(QWidget *parent = nullptr);
    virtual ~ChooseMainWindow() = default;

private:
    void paintEvent(QPaintEvent* event) override;
    void openGame(ChessBoard *game);
    void openMatch(const ChessRecord &record, const QString &title, const QString &sessionId = {});
    void refreshContinue();
    QPixmap m_sceneCache;
    QPushButton* m_buttons[4];
    QPushButton *m_continueButton = nullptr;
};

#endif // CHOOSEMAINWINDOW_H
