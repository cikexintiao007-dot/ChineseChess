// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#ifndef CHESSVOICE_H
#define CHESSVOICE_H

#include <QObject>
#include <QSoundEffect>
#include <QUrl>
#include <QQueue>

class ChessVoice : public QObject
{
    Q_OBJECT
  public:
    explicit ChessVoice(QObject *parent = nullptr);
    ~ChessVoice();

    void voiceWin();     // 胜利音效
    void voiceSelect();  // 选棋音效
    void voiceMove();    // 移动音效
    void voiceEat();     // 吃子音效
    void voiceBack();    // 悔棋音效
    void voiceGeneral(); // 将军音效
    void voiceDraw();
    void stop();
    void setVolume(qreal volume);

  private:
    void enqueue(QSoundEffect *effect);
    void playNext();
    QSoundEffect m_win;
    QSoundEffect m_select;
    QSoundEffect m_move;
    QSoundEffect m_eat;
    QSoundEffect m_back;
    QSoundEffect m_general;
    QSoundEffect m_draw;
    QQueue<QSoundEffect *> m_pending;
    QSoundEffect *m_current = nullptr;
};

#endif // CHESSVOICE_H
