// SPDX-License-Identifier: GPL-3.0-or-later
#include "NetworkGame.h"
#include "core/ChessRules.h"
#include "ui_ChessBoard.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include <QUuid>
#include <QMessageBox>
#include <QCryptographicHash>

namespace {
bool readInteger(const QJsonObject& object, const QString& key, int low, int high, int& value)
{
    const auto item = object.value(key);
    if (!item.isDouble()) return false;
    const double number = item.toDouble();
    if (number < low || number > high || number != int(number)) return false;
    value = int(number);
    return true;
}
}

NetworkGame::NetworkGame(LanRoom* room, QWidget* parent) : ChessBoard(parent), m_room(room)
{
    room->setParent(this);
    m_reconnectButton = new QPushButton(QStringLiteral("重新连接"), this);
    m_reconnectButton->setObjectName(QStringLiteral("reconnectButton"));
    ui->headerLayout->insertWidget(ui->headerLayout->count() - 1, m_reconnectButton);
    connect(m_reconnectButton, &QPushButton::clicked, this, [this] {
        m_haveState = false;
        m_reconnectButton->setEnabled(false);
        m_room->joinRoom(m_room->roomCode(), m_room->hostAddress());
        refreshPanel();
    });
    connect(room, &LanRoom::statusChanged, ui->labelHint, &QLabel::setText);
    connect(room, &LanRoom::failed, this, [this](const QString& reason) {
        refreshPanel();
        ui->labelHint->setText(reason);
    });
    m_haveState = room->isHost();
    setWindowTitle(QStringLiteral("局域网对弈 · 房间 %1").arg(room->roomCode()));
    setPerspectiveFlipped(!room->isHost());
    setGameDescription(QStringLiteral("联机 · %1").arg(room->roomCode()),
                       room->isHost() ? QStringLiteral("你执红方 · 对手执黑方") : QStringLiteral("对手执红方 · 你执黑方"));
    setPlayerRoles(room->isHost() ? QStringLiteral("你 · 房主") : QStringLiteral("对手 · 房主"),
                   room->isHost() ? QStringLiteral("对手") : QStringLiteral("你"));
    m_voteTimeout.setSingleShot(true);
    m_clockSync.setInterval(5000);
    connect(&m_voteTimeout, &QTimer::timeout, this, [this] { finishProposal(false); });
    connect(&m_clockSync, &QTimer::timeout, this, [this] {
        if (m_room->isHost() && m_room->isConnected())
            m_room->sendMessage({{"type", "clock"}, {"revision", m_revision}, {"seconds", elapsedSeconds()}, {"hash", positionHash()}});
    });
    m_clockSync.start();
    connect(room, &LanRoom::messageReceived, this, &NetworkGame::receive);
    connect(room, &LanRoom::connected, this, [this] {
        m_pending = false;
        m_room->startMessages();
        if (m_room->isHost()) {
            setElapsedSeconds(elapsedSeconds(), !m_ChessSteps.isEmpty());
            publishState();
        } else m_room->sendMessage({{"type", "sync"}});
        refreshPanel();
    });
    connect(room, &LanRoom::disconnected, this, [this](const QString& reason) {
        m_Chessvoice.stop();
        ++m_connectionGeneration;
        dismissGameResult();
        dismissProposal();
        m_proposal = {};
        m_voteTimeout.stop();
        m_pending = false;
        m_updates.clear();
        setElapsedSeconds(elapsedSeconds(), false);
        refreshPanel();
        ui->labelHint->setText(reason + (m_room->isHost()
            ? QStringLiteral("，等待对方用原房间号重新加入")
            : QStringLiteral("，可点击重新连接")));
    });
    refreshPanel();
    QTimer::singleShot(0, this, [this] {
        m_room->startMessages();
        if (m_room->isHost()) publishState();
        else m_room->sendMessage({{"type", "sync"}});
    });
}

NetworkGame::~NetworkGame()
{
    m_room->disconnect(this);
    m_room->closeRoom();
}

bool NetworkGame::canAct() const
{
    return !m_closed && m_room->isConnected() && m_haveState && !m_pending
        && m_proposal.isEmpty() && !isMoveAnimating();
}

void NetworkGame::refreshPanel()
{
    ChessBoard::refreshPanel();
    if (!m_room) return;
    m_reconnectButton->setVisible(!m_room->isHost() && !m_room->isConnected());
    m_reconnectButton->setEnabled(!m_room->isSearching());
    ui->pushButton_start->setEnabled(false);
    ui->pushButton_back->setEnabled(canAct() && !m_bIsOver && undoCount(m_room->isHost()) > 0);
    ui->pushButton_restart->setEnabled(canAct());
    findChild<QPushButton*>(QStringLiteral("resignButton"))->setEnabled(canAct() && !m_bIsOver);
    findChild<QPushButton*>(QStringLiteral("drawButton"))->setEnabled(canAct() && !m_bIsOver);
    if (!m_room->isConnected()) {
        ui->headerState->setText(m_room->isSearching() ? QStringLiteral("正在连接") : QStringLiteral("等待连接"));
        QString hint = m_room->isHost() ? QStringLiteral("对方离线，等待重新加入")
                                      : QStringLiteral("连接中断，可点击重新连接");
        if (m_room->isSearching()) hint = QStringLiteral("正在重新查找房间 %1…").arg(m_room->roomCode());
        ui->labelHint->setText(hint);
    } else if (!m_haveState || m_pending) {
        ui->headerState->setText(QStringLiteral("同步中"));
        ui->labelHint->setText(QStringLiteral("正在等待房主确认…"));
    } else if (!m_proposal.isEmpty()) {
        ui->headerState->setText(QStringLiteral("等待确认"));
        ui->labelHint->setText(QStringLiteral("等待双方确认，15 秒后自动取消"));
    } else if (!m_bIsOver && !isMoveAnimating()) {
        ui->labelHint->setText(repetitionCount() >= 3
            ? QStringLiteral("局面重复 %1 次，可求和或变招").arg(repetitionCount())
            : m_bIsRed == m_room->isHost() ? QStringLiteral("轮到你走棋") : QStringLiteral("等待对方走棋…"));
    }
}

void NetworkGame::clickPieces(int id, int& row, int& col)
{
    if (!canAct() || m_bIsOver || m_bIsRed != m_room->isHost()) return;
    if (id >= 0 && id < 32 && isRed(id) == m_room->isHost()) {
        trySelectStone(id);
        return;
    }
    if (m_nSelectID < 0 || !ChessRules(m_ChessPieces).canMove(m_nSelectID, id, row, col)) return;
    if (m_room->isHost()) commitMove(m_nSelectID, row, col, m_revision, true);
    else {
        m_pending = true;
        m_room->sendMessage({{"type", "move"}, {"id", m_nSelectID}, {"row", row}, {"col", col}, {"revision", m_revision}});
        m_nSelectID = -1;
        refreshPanel();
        update();
    }
}

void NetworkGame::commitMove(int id, int row, int col, int revision, bool red)
{
    if (revision != m_revision || !canAct() || m_bIsOver || m_bIsRed != red
        || id < 0 || id >= 32 || row < 0 || row > 9 || col < 0 || col > 8
        || m_ChessPieces[id].m_bDead || isRed(id) != red) {
        publishState(QStringLiteral("该操作已失效，棋盘已同步"));
        return;
    }
    const int captured = getStoneId(row, col);
    if (!ChessRules(m_ChessPieces).canMove(id, captured, row, col)) {
        publishState(QStringLiteral("此处无法落子"));
        return;
    }
    const int previous = m_revision++;
    doMoveStone(id, captured, row, col);
    m_room->sendMessage({{"type", "moveApplied"}, {"previous", previous}, {"revision", m_revision},
        {"epoch", m_epoch}, {"move", QJsonObject{{"id", id}, {"captured", captured}, {"row", row}, {"col", col}}},
        {"seconds", elapsedSeconds()}, {"outcome", int(outcome())}, {"reason", resultReason()}, {"hash", positionHash()}});
}

void NetworkGame::publishState(const QString& notice)
{
    if (!m_room->isHost()) return;
    QJsonArray moves;
    for (const auto* step : m_ChessSteps)
        moves.append(QJsonObject{{"id", step->m_nMoveID}, {"captured", step->m_nKillID},
                                 {"row", step->m_nRowTo}, {"col", step->m_nnColTo}});
    m_room->sendMessage({{"type", "state"}, {"revision", m_revision}, {"epoch", m_epoch},
                         {"moves", moves}, {"seconds", elapsedSeconds()}, {"notice", notice},
                         {"outcome", int(outcome())}, {"reason", resultReason()}, {"hash", positionHash()}});
}

QString NetworkGame::positionHash() const
{
    QByteArray bytes;
    for (const auto& piece : m_ChessPieces) {
        bytes.append(char(piece.m_bDead));
        bytes.append(char(piece.m_nRow));
        bytes.append(char(piece.m_nCol));
    }
    bytes.append(char(m_bIsRed));
    bytes.append(char(outcome()));
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

void NetworkGame::queueUpdate(const QJsonObject& message)
{
    if (m_updates.size() >= 64) {
        m_updates.clear();
        m_room->sendMessage({{"type", "sync"}});
        return;
    }
    m_updates.enqueue(message);
    processUpdates();
}

void NetworkGame::processUpdates()
{
    if (m_waitingState || m_updates.isEmpty() || !m_room->isConnected()) return;
    m_waitingState = true;
    runAfterMoveAnimation([this] {
        m_waitingState = false;
        if (m_updates.isEmpty() || !m_room->isConnected()) return;
        const auto next = m_updates.dequeue();
        if (next.value("type").toString() == "state") applyState(next);
        else applyMove(next);
        processUpdates();
    });
}

void NetworkGame::applyOutcome(const QJsonObject& message)
{
    int result;
    if (!readInteger(message, "outcome", 0, 3, result)) return;
    if (result != int(Outcome::Ongoing) && !m_bIsOver)
        finishGame(Outcome(result), message.value("reason").toString().left(128));
}

void NetworkGame::applyMove(const QJsonObject& message)
{
    int previous, revision, epoch, seconds;
    if (!readInteger(message, "previous", 0, 2000000000, previous)
        || !readInteger(message, "revision", 1, 2000000000, revision)
        || !readInteger(message, "epoch", 1, 2000000000, epoch)
        || !readInteger(message, "seconds", 0, 86399, seconds)) return;
    if (revision <= m_revision && m_haveState) return;
    const auto move = message.value("move").toObject();
    int id, captured, row, col;
    const bool valid = m_haveState && previous == m_revision && revision == previous + 1 && epoch == m_epoch
        && readInteger(move, "id", 0, 31, id) && readInteger(move, "captured", -1, 31, captured)
        && readInteger(move, "row", 0, 9, row) && readInteger(move, "col", 0, 8, col)
        && !m_bIsOver && isRed(id) == m_bIsRed && ChessRules(m_ChessPieces).canMove(id, captured, row, col);
    if (!valid) {
        m_room->sendMessage({{"type", "sync"}});
        return;
    }
    doMoveStone(id, captured, row, col);
    applyOutcome(message);
    m_revision = revision;
    m_pending = false;
    setElapsedSeconds(seconds, !m_bIsOver);
    if (message.value("hash").toString() != positionHash()) m_room->sendMessage({{"type", "sync"}});
    refreshPanel();
}

void NetworkGame::receive(const QJsonObject& message)
{
    if (m_closed) return;
    const QString type = message.value("type").toString();
    if (m_room->isHost()) {
        if (type == "sync") { publishState(); return; }
        if (type == "vote") {
            if (!m_proposal.isEmpty() && m_proposal.value("requesterRed").toBool()
                && message.value("proposal").toString() == m_proposal.value("proposal").toString())
                finishProposal(message.value("accept").toBool());
            return;
        }
        if (type == "move" || type == "request") {
            const auto generation = m_connectionGeneration;
            runAfterMoveAnimation([this, message, type, generation] {
                if (!m_room->isConnected() || m_closed || generation != m_connectionGeneration) return;
                int revision;
                if (!readInteger(message, "revision", 0, 2000000000, revision)) { publishState(); return; }
                if (type == "request") {
                    propose(message.value("action").toString(), false, revision);
                    return;
                }
                int id, row, col;
                if (!readInteger(message, "id", 0, 31, id) || !readInteger(message, "row", 0, 9, row)
                    || !readInteger(message, "col", 0, 8, col)) { publishState(); return; }
                commitMove(id, row, col, revision, false);
            });
        }
    } else if (type == "state" || type == "moveApplied") {
        queueUpdate(message);
    } else if (type == "proposal") {
        showProposal(message);
    } else if (type == "proposalDone") {
        dismissProposal();
        m_proposal = {};
        m_pending = false;
        refreshPanel();
        if (!message.value("accept").toBool()) showNotice(QStringLiteral("请求已取消或被拒绝"));
    } else if (type == "clock") {
        int seconds, revision;
        if (!readInteger(message, "seconds", 0, 86399, seconds)
            || !readInteger(message, "revision", 0, 2000000000, revision) || revision != m_revision) return;
        setElapsedSeconds(seconds, !m_ChessSteps.isEmpty() && !m_bIsOver);
        if (message.value("hash").toString() != positionHash())
            m_room->sendMessage({{"type", "sync"}});
    }
}

void NetworkGame::applyState(const QJsonObject& message)
{
    int revision, epoch, seconds;
    if (!readInteger(message, "revision", 0, 2000000000, revision)
        || !readInteger(message, "epoch", 1, 2000000000, epoch)
        || !readInteger(message, "seconds", 0, 86399, seconds)
        || !message.value("moves").isArray()) return;
    if (m_haveState && revision < m_revision) return;
    const auto array = message.value("moves").toArray();
    QVector<RecordedMove> moves;
    for (const auto& value : array) {
        if (!value.isObject()) return;
        const auto object = value.toObject();
        RecordedMove move;
        if (!readInteger(object, "id", 0, 31, move.id) || !readInteger(object, "captured", -1, 31, move.captured)
            || !readInteger(object, "row", 0, 9, move.row) || !readInteger(object, "col", 0, 8, move.col)) return;
        moves.append(move);
    }
    bool prefix = m_haveState && epoch == m_epoch && moves.size() >= m_ChessSteps.size();
    for (int i = 0; prefix && i < m_ChessSteps.size(); ++i) {
        const auto* step = m_ChessSteps[i];
        const auto& move = moves[i];
        prefix = step->m_nMoveID == move.id && step->m_nKillID == move.captured
            && step->m_nRowTo == move.row && step->m_nnColTo == move.col;
    }
    bool valid = true;
    if (prefix && moves.size() == m_ChessSteps.size() + 1) {
        const auto move = moves.last();
        valid = !m_bIsOver && !m_ChessPieces[move.id].m_bDead && isRed(move.id) == m_bIsRed
            && getStoneId(move.row, move.col) == move.captured
            && ChessRules(m_ChessPieces).canMove(move.id, move.captured, move.row, move.col);
        if (valid) doMoveStone(move.id, move.captured, move.row, move.col);
    } else if (!(prefix && moves.size() == m_ChessSteps.size())
               || message.value("hash").toString() != positionHash()) {
        valid = restoreMoves(moves, seconds);
    }
    if (!valid) {
        m_room->closeRoom();
        setElapsedSeconds(elapsedSeconds(), false);
        refreshPanel();
        showNotice(QStringLiteral("棋盘同步校验失败，请重新加入房间"));
        return;
    }
    applyOutcome(message);
    m_haveState = true;
    m_revision = revision;
    m_epoch = epoch;
    m_pending = false;
    setElapsedSeconds(seconds, !moves.isEmpty() && !m_bIsOver);
    refreshPanel();
    if (message.value("hash").toString() != positionHash()) {
        m_room->closeRoom();
        setElapsedSeconds(elapsedSeconds(), false);
        refreshPanel();
        showNotice(QStringLiteral("局面校验失败，请重新连接"));
        return;
    }
    const auto notice = message.value("notice").toString();
    if (!notice.isEmpty()) showNotice(notice);
}

int NetworkGame::undoCount(bool red) const
{
    if (m_bIsOver || m_ChessSteps.isEmpty()) return 0;
    if (m_ChessPieces[m_ChessSteps.last()->m_nMoveID].m_bRed == red) return 1;
    if (m_ChessSteps.size() >= 2 && m_ChessPieces[m_ChessSteps[m_ChessSteps.size()-2]->m_nMoveID].m_bRed == red) return 2;
    return 0;
}

void NetworkGame::init() { requestAction(QStringLiteral("restart")); }
void NetworkGame::back() { requestAction(QStringLiteral("undo")); }

void NetworkGame::resign()
{
    if (!canAct() || m_bIsOver) return;
    if (confirmAction(QStringLiteral("认输"), QStringLiteral("确认认输并结束本局？")))
        requestAction(QStringLiteral("resign"));
}

void NetworkGame::offerDraw()
{
    if (canAct() && !m_bIsOver) requestAction(QStringLiteral("draw"));
}

void NetworkGame::requestAction(const QString& action)
{
    if (!canAct()) return;
    if (m_room->isHost()) propose(action, true, m_revision);
    else {
        m_pending = true;
        m_room->sendMessage({{"type", "request"}, {"action", action}, {"revision", m_revision}});
        refreshPanel();
    }
}

void NetworkGame::propose(const QString& action, bool requesterRed, int revision)
{
    if (!canAct() || revision != m_revision
        || (action != "restart" && action != "undo" && action != "draw" && action != "resign")
        || ((action == "draw" || action == "resign") && m_bIsOver)
        || (action == "undo" && undoCount(requesterRed) == 0)) {
        publishState(QStringLiteral("当前无法执行该请求"));
        return;
    }
    if (action == "resign") {
        finishGame(requesterRed ? Outcome::BlackWon : Outcome::RedWon,
                   requesterRed ? QStringLiteral("红方认输") : QStringLiteral("黑方认输"));
        ++m_revision;
        publishState();
        return;
    }
    m_proposal = {{"type", "proposal"}, {"proposal", QUuid::createUuid().toString()},
                  {"action", action}, {"requesterRed", requesterRed}, {"revision", m_revision}};
    m_voteTimeout.start(15000);
    m_room->sendMessage(m_proposal);
    showProposal(m_proposal);
}

void NetworkGame::showProposal(const QJsonObject& message)
{
    dismissProposal();
    m_proposal = message;
    m_pending = false;
    refreshPanel();
    if (message.value("requesterRed").toBool() == m_room->isHost()) return;
    const auto token = message.value("proposal").toString();
    auto* dialog = new QDialog(this);
    m_voteDialog = dialog;
    dialog->setObjectName(QStringLiteral("networkVoteDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle(QStringLiteral("对局请求"));
    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(18);
    const QString action = message.value("action").toString();
    QString request = QStringLiteral("对方请求重新开始，是否同意？");
    if (action == "undo") request = QStringLiteral("对方请求悔棋，是否同意？");
    if (action == "draw") request = QStringLiteral("对方请求和棋，是否同意？");
    auto* text = new QLabel(request, dialog);
    layout->addWidget(text);
    auto* yes = new QPushButton(QStringLiteral("同意"), dialog);
    yes->setObjectName(QStringLiteral("acceptNetworkRequest"));
    auto* no = new QPushButton(QStringLiteral("拒绝"), dialog);
    layout->addWidget(yes);
    layout->addWidget(no);
    connect(yes, &QPushButton::clicked, dialog, &QDialog::accept);
    connect(no, &QPushButton::clicked, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, this, [this, token](int result) {
        m_voteDialog = nullptr;
        if (m_proposal.value("proposal").toString() != token) return;
        if (m_room->isHost()) finishProposal(result == QDialog::Accepted);
        else m_room->sendMessage({{"type", "vote"}, {"proposal", token}, {"accept", result == QDialog::Accepted}});
    });
    dialog->open();
}

void NetworkGame::dismissProposal()
{
    if (m_voteDialog) {
        auto* dialog = m_voteDialog.data();
        m_voteDialog = nullptr;
        dialog->disconnect(this);
        dialog->close();
    }
}

void NetworkGame::finishProposal(bool accepted)
{
    if (!m_room->isHost() || m_proposal.isEmpty()) return;
    const auto proposal = m_proposal;
    m_proposal = {};
    m_voteTimeout.stop();
    dismissProposal();
    if (accepted && m_room->isConnected()) {
        if (proposal.value("action").toString() == "restart") {
            ++m_epoch;
            ChessBoard::init();
        } else if (proposal.value("action").toString() == "draw") {
            finishGame(Outcome::Draw, QStringLiteral("双方同意和棋"));
        } else {
            const int count = undoCount(proposal.value("requesterRed").toBool());
            for (int i = 0; i < count; ++i) backOne();
        }
        ++m_revision;
    } else accepted = false;
    m_room->sendMessage({{"type", "proposalDone"}, {"accept", accepted}});
    publishState();
    refreshPanel();
    update();
}

void NetworkGame::closeEvent(QCloseEvent* event)
{
    ChessBoard::closeEvent(event);
    if (!event->isAccepted()) return;
    m_closed = true;
    m_voteTimeout.stop();
    m_clockSync.stop();
    dismissProposal();
    m_room->closeRoom();
}
