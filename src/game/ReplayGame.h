// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef REPLAYGAME_H
#define REPLAYGAME_H

#include "ui/ChessBoard.h"
class QSlider;

class ReplayGame : public ChessBoard
{
public:
    explicit ReplayGame(const ChessRecord &record, QWidget *embeddedParent = nullptr);
    ChessRecord gameRecord() const override { return m_record; }
    void mouseReleaseEvent(QMouseEvent *) override {}
    void init() override;

protected:
    void refreshPanel() override;
    bool confirmLeaving() override { return true; }

private:
    void seek(int step);
    void continueGame();
    ChessRecord m_record;
    int m_step = 0;
    bool m_ready = false;
    QLabel *m_stepLabel = nullptr;
    QSlider *m_slider = nullptr;
    QPushButton *m_first = nullptr;
    QPushButton *m_previous = nullptr;
    QPushButton *m_next = nullptr;
    QPushButton *m_last = nullptr;
    QPushButton *m_continue = nullptr;
};

#endif
