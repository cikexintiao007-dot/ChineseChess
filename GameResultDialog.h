// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GAMERESULTDIALOG_H
#define GAMERESULTDIALOG_H

#include <QDialog>
#include <QPixmap>

class GameResultDialog : public QDialog
{
public:
    enum Action { ViewBoard = QDialog::Rejected, PlayAgain = 2, ReturnToMenu = 3 };

    GameResultDialog(bool redWon, const QString& winnerRole, const QString& mode,
                     const QString& elapsed, int steps, const QPixmap& emblem,
                     QWidget* parent = nullptr, const QString& reason = QString(), bool drawn = false);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_sceneCache;
};

#endif
