// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NETWORKGAME_H
#define NETWORKGAME_H
#include "ChessBoard.h"
#include "LanRoom.h"
#include <QJsonArray>

class NetworkGame : public ChessBoard
{
    Q_OBJECT
public:
    explicit NetworkGame(LanRoom* room, QWidget* parent = nullptr);
    ~NetworkGame() override;
    void init() override;
    void back() override;
    void clickPieces(int id, int& row, int& col) override;
    void resign() override;
    void offerDraw() override;
protected:
    bool humanIsRed() const override { return m_room->isHost(); }
    void refreshPanel() override;
    void closeEvent(QCloseEvent* event) override;
private:
    void receive(const QJsonObject& message);
    void publishState(const QString& notice = QString());
    void applyState(const QJsonObject& message);
    void queueUpdate(const QJsonObject& message);
    void processUpdates();
    void applyMove(const QJsonObject& message);
    void applyOutcome(const QJsonObject& message);
    QString positionHash() const;
    void commitMove(int id, int row, int col, int revision, bool red);
    void requestAction(const QString& action);
    void propose(const QString& action, bool requesterRed, int revision);
    void showProposal(const QJsonObject& message);
    void finishProposal(bool accepted);
    void dismissProposal();
    bool canAct() const;
    int undoCount(bool red) const;
    LanRoom* m_room;
    int m_revision = 0;
    int m_epoch = 1;
    quint64 m_connectionGeneration = 0;
    bool m_haveState = false;
    bool m_pending = false;
    bool m_waitingState = false;
    bool m_closed = false;
    QQueue<QJsonObject> m_updates;
    QPushButton* m_reconnectButton = nullptr;
    QJsonObject m_proposal;
    QPointer<QDialog> m_voteDialog;
    QTimer m_voteTimeout;
    QTimer m_clockSync;
};
#endif
