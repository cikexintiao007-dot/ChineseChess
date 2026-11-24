// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2019-2026 XMuli & Contributors
// SPDX-GitHub: https://github.com/XMuli/ChineseChess
// SPDX-Author: XMuli <xmulitech@gmail.com>

#include "ChessVoice.h"

#include <QTimer>

ChessVoice::ChessVoice(QObject *parent) : QObject(parent)
{
    m_win.setSource(QUrl(QStringLiteral("qrc:/sound/WinSound.wav")));
    m_select.setSource(QUrl(QStringLiteral("qrc:/sound/selectChess.wav")));
    m_move.setSource(QUrl(QStringLiteral("qrc:/sound/moveChess.wav")));
    m_eat.setSource(QUrl(QStringLiteral("qrc:/sound/eatChess.wav")));
    m_back.setSource(QUrl(QStringLiteral("qrc:/sound/backChess.wav")));
    m_general.setSource(QUrl(QStringLiteral("qrc:/sound/generalSound.wav")));
    m_draw.setSource(QUrl(QStringLiteral("qrc:/sound/drawSound.wav")));
    for (auto *effect : {&m_win, &m_select, &m_move, &m_eat, &m_back, &m_general, &m_draw})
    {
        effect->setParent(this);
        effect->setObjectName(effect->source().fileName());
        effect->setLoopCount(1);
        connect(effect, &QSoundEffect::playingChanged, this, [this, effect] {
            if (m_current != effect || effect->isPlaying())
                return;
            m_current = nullptr;
            QTimer::singleShot(25, this, &ChessVoice::playNext);
        });
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect] {
            if (effect->status() == QSoundEffect::Error && m_current == effect)
                m_current = nullptr;
            playNext();
        });
    }
}

ChessVoice::~ChessVoice()
{
    for (auto *effect : {&m_win, &m_select, &m_move, &m_eat, &m_back, &m_general, &m_draw})
        effect->disconnect(this);
    stop();
}

void ChessVoice::setVolume(qreal volume)
{
    if (volume <= 0)
        stop();
    for (auto *effect : {&m_win, &m_select, &m_move, &m_eat, &m_back, &m_general, &m_draw})
        effect->setVolume(qBound(qreal(0), volume, qreal(1)));
}

void ChessVoice::stop()
{
    m_pending.clear();
    m_current = nullptr;
    for (auto *effect : {&m_win, &m_select, &m_move, &m_eat, &m_back, &m_general, &m_draw})
        effect->stop();
}

void ChessVoice::enqueue(QSoundEffect *effect)
{
    if (effect->volume() <= 0 || m_pending.contains(effect))
        return;
    if (m_pending.size() >= 4)
        m_pending.dequeue();
    m_pending.enqueue(effect);
    playNext();
}

void ChessVoice::playNext()
{
    if (m_current)
        return;
    while (!m_pending.isEmpty())
    {
        auto *next = m_pending.head();
        if (next->status() == QSoundEffect::Error)
        {
            m_pending.dequeue();
            continue;
        }
        if (next->status() != QSoundEffect::Ready)
            return;
        m_current = m_pending.dequeue();
        m_current->play();
        return;
    }
}

void ChessVoice::voiceWin()
{
    stop();
    enqueue(&m_win);
}

void ChessVoice::voiceDraw()
{
    stop();
    enqueue(&m_draw);
}

void ChessVoice::voiceSelect()
{
    if (!m_current && m_pending.isEmpty())
        enqueue(&m_select);
}

void ChessVoice::voiceMove()
{
    if (!m_current && m_pending.isEmpty())
        enqueue(&m_move);
}

void ChessVoice::voiceEat()
{
    enqueue(&m_eat);
}

void ChessVoice::voiceBack()
{
    stop();
    enqueue(&m_back);
}

void ChessVoice::voiceGeneral()
{
    enqueue(&m_general);
}
