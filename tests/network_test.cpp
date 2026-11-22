// SPDX-License-Identifier: GPL-3.0-or-later
#include "NetworkGame.h"
#include "LanRoomDialog.h"
#include "GameTheme.h"
#include <QApplication>
#include <QFontDatabase>
#include <QTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QLineEdit>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QMessageBox>
#include <QDir>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char *reason)
{
    if (!ok)
        throw std::runtime_error(reason);
}
template <class F> void waitFor(F done, const char *reason, int limit = 2500)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < limit)
        QTest::qWait(5);
    require(done(), reason);
}
void sameBoard(const ChessBoard &a, const ChessBoard &b)
{
    require(a.m_ChessSteps.size() == b.m_ChessSteps.size(), "history mismatch");
    require(a.m_bIsRed == b.m_bIsRed && a.m_bIsOver == b.m_bIsOver, "turn mismatch");
    for (int i = 0; i < 32; ++i)
    {
        const auto &x = a.m_ChessPieces[i];
        const auto &y = b.m_ChessPieces[i];
        require(x.m_bDead == y.m_bDead && x.m_nRow == y.m_nRow && x.m_nCol == y.m_nCol, "piece mismatch");
    }
}
void move(NetworkGame &game, int fromRow, int fromCol, int toRow, int toCol)
{
    int row = fromRow, col = fromCol;
    game.clickPieces(game.getStoneId(row, col), row, col);
    row = toRow;
    col = toCol;
    game.clickPieces(game.getStoneId(row, col), row, col);
}
void settle(NetworkGame &a, NetworkGame &b, int count)
{
    waitFor(
        [&] {
            return a.m_ChessSteps.size() == count && b.m_ChessSteps.size() == count && !a.isMoveAnimating() &&
                   !b.isMoveAnimating();
        },
        "move sync failed");
    sameBoard(a, b);
}
void vote(NetworkGame &target, bool accepted)
{
    waitFor(
        [&] {
            auto *d = target.findChild<QDialog *>("networkVoteDialog");
            return d && d->isVisible();
        },
        "vote did not appear");
    auto *d = target.findChild<QDialog *>("networkVoteDialog");
    if (accepted)
        d->accept();
    else
        d->reject();
    QTest::qWait(80);
}

int runPeerProbe(bool hostMode, const QString &code, const QString &address, const QString &codeFile)
{
    auto *room = new LanRoom;
    if (hostMode)
    {
        require(room->createRoom(), "cross-system host creation failed");
        QFile output(codeFile);
        require(output.open(QIODevice::WriteOnly), "room code file unavailable");
        output.write(room->roomCode().toUtf8());
    }
    else
    {
        room->joinRoom(code, address);
    }
    waitFor([&] { return room->isConnected(); }, "cross-system connection failed", 60000);
    NetworkGame game(room);
    game.show();
    QTest::qWait(100);
    bool finished = false;
    bool sent = false;
    QObject::connect(room, &LanRoom::messageReceived, &game, [&](const QJsonObject &message) {
        if (hostMode && message.value("type").toString() == "probeComplete")
        {
            require(game.m_ChessSteps.size() == 3 && game.m_ChessPieces[11].m_bDead,
                    "host capture state differs");
            room->sendMessage({{"type", "probeAck"}});
            finished = true;
        }
        if (!hostMode && message.value("type").toString() == "probeAck")
            finished = true;
    });
    QElapsedTimer deadline;
    deadline.start();
    while (!finished && deadline.elapsed() < 20000)
    {
        const int steps = game.m_ChessSteps.size();
        if (!game.isMoveAnimating())
        {
            if (hostMode && steps == 0)
                move(game, 6, 0, 5, 0);
            if (!hostMode && steps == 1)
                move(game, 3, 0, 4, 0);
            if (hostMode && steps == 2)
                move(game, 5, 0, 4, 0);
            if (!hostMode && steps == 3 && !sent)
            {
                require(game.m_ChessPieces[11].m_bDead && game.getStoneId(4, 0) == 31,
                        "guest capture state differs");
                room->sendMessage({{"type", "probeComplete"}});
                sent = true;
            }
        }
        QTest::qWait(10);
    }
    require(finished, "cross-system move synchronization timed out");
    QTest::qWait(100);
    game.close();
    std::cout << "PASS cross-system discovery, TCP handshake, red/black moves and capture synchronization"
              << std::endl;
    return 0;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // The offscreen platform does not enumerate the Windows font collection.
    const QString fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
    for (const QString &file :
         {QStringLiteral("msyh.ttc"), QStringLiteral("msyhbd.ttc"), QStringLiteral("simkai.ttf")})
        QFontDatabase::addApplicationFont(fonts + file);
#endif
    app.setQuitOnLastWindowClosed(false);
    applyGameTheme(app);
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    app.setOrganizationName("ChineseChessTests");
    app.setApplicationName("network");
    auto preferences = GamePreferences::load();
    preferences.confirmExit = false;
    preferences.sound = false;
    preferences.save();
    QDir().mkpath(QDir::tempPath() + "/ChineseChess-test-artifacts");
    try
    {
        const auto arguments = app.arguments();
        if (arguments.size() == 3 && arguments[1] == "--peer-host")
            return runPeerProbe(true, QString(), QString(), arguments[2]);
        if (arguments.size() == 4 && arguments[1] == "--peer-join")
            return runPeerProbe(false, arguments[2], arguments[3], QString());
        auto *hostRoom = new LanRoom;
        auto *guestRoom = new LanRoom;
        require(hostRoom->createRoom(), "host failed");
        require(hostRoom->roomCode().size() == 6, "room not six digits");
        guestRoom->joinRoom(hostRoom->roomCode());
        waitFor([&] { return hostRoom->isConnected() && guestRoom->isConnected(); },
                "UDP discovery / TCP handshake failed", 5000);
        const QString code = hostRoom->roomCode();
        NetworkGame host(hostRoom), guest(guestRoom);
        host.show();
        guest.show();
        QTest::qWait(100);
        require(!host.m_bReverseView && guest.m_bReverseView, "guest perspective failed");
        require(!guest.findChild<QPushButton *>("pushButton_start")->isEnabled(), "network timer can pause");
        move(guest, 3, 0, 4, 0);
        QTest::qWait(40);
        settle(host, guest, 0);
        move(host, 6, 0, 5, 0);
        settle(host, guest, 1);
        guest.m_ChessPieces[16].m_nCol = 7;
        guestRoom->sendMessage({{"type", "sync"}});
        waitFor([&] { return guest.m_ChessPieces[16].m_nCol == host.m_ChessPieces[16].m_nCol; },
                "full sync did not repair divergent board");
        sameBoard(host, guest);
        guestRoom->sendMessage({{"type", "move"}, {"id", 11}, {"row", 4}, {"col", 0}, {"revision", 0}});
        QTest::qWait(80);
        settle(host, guest, 1);
        guestRoom->sendMessage({{"type", "move"}, {"id", 99}, {"row", 4}, {"col", 0}, {"revision", 1}});
        QTest::qWait(80);
        settle(host, guest, 1);
        move(guest, 3, 0, 4, 0);
        settle(host, guest, 2);
        move(host, 5, 0, 4, 0);
        settle(host, guest, 3);
        require(host.m_ChessPieces[11].m_bDead, "capture failed");
        guest.back();
        vote(host, false);
        settle(host, guest, 3);
        guest.back();
        vote(host, true);
        settle(host, guest, 1);
        require(!host.m_ChessPieces[11].m_bDead, "undo did not revive captured piece");
        host.back();
        vote(guest, true);
        settle(host, guest, 0);
        move(host, 6, 0, 5, 0);
        settle(host, guest, 1);
        guest.init();
        vote(host, true);
        settle(host, guest, 0);
        move(host, 6, 0, 5, 0);
        settle(host, guest, 1);
        host.init();
        vote(guest, true);
        settle(host, guest, 0);
        move(host, 9, 0, 8, 0);
        settle(host, guest, 1);
        move(guest, 0, 5, 1, 4);
        settle(host, guest, 2);
        move(host, 8, 0, 8, 4);
        settle(host, guest, 3);
        move(guest, 1, 4, 2, 3);
        settle(host, guest, 4);
        move(host, 6, 4, 5, 4);
        settle(host, guest, 5);
        move(guest, 2, 7, 6, 7);
        settle(host, guest, 6);
        move(host, 5, 4, 4, 4);
        settle(host, guest, 7);
        move(guest, 3, 0, 4, 0);
        settle(host, guest, 8);
        move(host, 4, 4, 4, 3);
        settle(host, guest, 9);
        move(guest, 3, 2, 4, 2);
        settle(host, guest, 10);
        move(host, 8, 4, 3, 4);
        settle(host, guest, 11);
        move(guest, 0, 4, 0, 5);
        settle(host, guest, 12);
        move(host, 3, 4, 3, 5);
        settle(host, guest, 13);
        require(host.m_bIsOver && guest.m_bIsOver, "victory did not synchronize");
        waitFor(
            [&] {
                return host.findChild<QDialog *>("gameResultDialog") &&
                       guest.findChild<QDialog *>("gameResultDialog");
            },
            "network result dialogs absent");
        host.findChild<QPushButton *>("resultAgain")->click();
        vote(guest, true);
        settle(host, guest, 0);
        std::cout << "PASS synchronized victory, result dialog replay with remote approval" << std::endl;
        for (int cycle = 0; cycle < 2; ++cycle) {
            move(host, 7, 1, 7, 2);
            settle(host, guest, cycle * 4 + 1);
            move(guest, 2, 1, 2, 2);
            settle(host, guest, cycle * 4 + 2);
            move(host, 7, 2, 7, 1);
            settle(host, guest, cycle * 4 + 3);
            move(guest, 2, 2, 2, 1);
            settle(host, guest, cycle * 4 + 4);
        }
        require(host.repetitionCount() == 3 && guest.repetitionCount() == 3 && !host.m_bIsOver,
                "network repetition history diverged");
        require(guest.findChild<QLabel *>("labelHint")->text().contains(QStringLiteral("重复 3 次")),
                "network status hid repetition warning");
        guest.m_ChessPieces[16].m_nCol = 7;
        guestRoom->sendMessage({{"type", "sync"}});
        waitFor([&] { return guest.m_ChessPieces[16].m_nCol == host.m_ChessPieces[16].m_nCol; },
                "repetition state repair failed");
        require(guest.repetitionCount() == 3, "full sync lost repetition history");
        host.init();
        vote(guest, true);
        settle(host, guest, 0);
        require(host.repetitionCount() == 1 && guest.repetitionCount() == 1,
                "network restart retained repetition history");
        std::cout << "PASS network repetition warning, full-state repair and reset" << std::endl;
        std::cout << "PASS discovery, handshake, colors, turns, capture, stale and malformed move rejection, "
                     "undo both players, consensual restart"
                  << std::endl;
        host.init();
        waitFor(
            [&] {
                auto *d = guest.findChild<QDialog *>("networkVoteDialog");
                return d && d->isVisible();
            },
            "timeout vote absent");
        waitFor([&] { return !guest.findChild<QDialog *>("networkVoteDialog"); },
                "unanswered request did not time out", 17000);
        require(host.findChild<QPushButton *>("pushButton_restart")->isEnabled(),
                "timeout left controls locked");
        std::cout << "PASS unanswered request expires and unlocks both peers" << std::endl;
        guest.offerDraw();
        vote(host, true);
        waitFor([&] { return host.m_bIsOver && guest.m_bIsOver; }, "agreed draw not synchronized");
        require(host.outcome() == ChessBoard::Outcome::Draw && guest.outcome() == ChessBoard::Outcome::Draw,
                "draw outcome differs");
        host.init();
        vote(guest, true);
        settle(host, guest, 0);
        QTimer::singleShot(0, [] {
            auto *confirmation = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            require(confirmation, "resignation confirmation missing");
            confirmation->done(QMessageBox::Yes);
        });
        guest.resign();
        waitFor([&] { return host.m_bIsOver && guest.m_bIsOver; }, "resignation not synchronized");
        require(host.outcome() == ChessBoard::Outcome::RedWon &&
                    guest.outcome() == ChessBoard::Outcome::RedWon,
                "resignation winner incorrect");
        require(host.resultReason() == QStringLiteral("黑方认输") && host.resultReason() == guest.resultReason(),
                "resignation reason differs between players");
        host.init();
        vote(guest, true);
        settle(host, guest, 0);
        guestRoom->findChild<QTcpSocket *>()->disconnectFromHost();
        waitFor([&] { return !hostRoom->isConnected() && !guestRoom->isConnected(); },
                "temporary disconnect not observed");
        guest.findChild<QPushButton *>("reconnectButton")->click();
        waitFor([&] { return hostRoom->isConnected() && guestRoom->isConnected(); },
                "reconnect button failed");
        QTest::qWait(100);
        std::cout << "PASS full-state repair, agreed draw, resignation, in-place reconnect" << std::endl;
        move(host, 6, 0, 5, 0);
        settle(host, guest, 1);
        guest.close();
        waitFor([&] { return !hostRoom->isConnected(); }, "disconnect not observed");
        move(host, 6, 2, 5, 2);
        require(host.m_ChessSteps.size() == 1, "offline host can move");
        auto *reconnectRoom = new LanRoom;
        reconnectRoom->joinRoom(code, "127.0.0.1");
        waitFor([&] { return reconnectRoom->isConnected() && hostRoom->isConnected(); }, "rejoin failed");
        NetworkGame rejoined(reconnectRoom);
        rejoined.show();
        settle(host, rejoined, 1);
        move(rejoined, 3, 0, 4, 0);
        settle(host, rejoined, 2);
        std::cout << "PASS disconnection freezes play, same room rejoin restores history and resumes play"
                  << std::endl;
        host.close();
        rejoined.close();
        QTest::qWait(30);
        // Exercise the normal create-room dialog and automatic acceptance.
        LanRoomDialog lobby;
        LanRoom peer;
        auto *create = lobby.findChild<QPushButton *>("createRoomButton");
        require(create, "create room UI absent");
        QTimer::singleShot(0, &lobby, [&] {
            create->click();
            lobby.grab().save(QDir::tempPath() + "/ChineseChess-test-artifacts/lan-room-host.png");
            const QString room = lobby.findChild<QLabel *>("roomCodeLabel")->text();
            peer.joinRoom(room, "127.0.0.1");
        });
        QTimer::singleShot(5000, &lobby, &QDialog::reject);
        require(lobby.exec() == QDialog::Accepted, "room lobby did not accept");
        auto *taken = lobby.takeRoom();
        require(taken->isConnected(), "lobby transport lost");
        delete taken;
        LanRoom joiningHost;
        require(joiningHost.createRoom(), "joining UI host failed");
        LanRoomDialog joiningLobby;
        QTimer::singleShot(0, &joiningLobby, [&] {
            joiningLobby.findChild<QTabWidget *>()->setCurrentIndex(1);
            joiningLobby.findChild<QLineEdit *>("roomCodeInput")->setText(joiningHost.roomCode());
            joiningLobby.grab().save(QDir::tempPath() + "/ChineseChess-test-artifacts/lan-room-join.png");
            joiningLobby.findChild<QPushButton *>("joinRoomButton")->click();
        });
        QTimer::singleShot(5000, &joiningLobby, &QDialog::reject);
        require(joiningLobby.exec() == QDialog::Accepted, "joining room UI failed");
        delete joiningLobby.takeRoom();
        joiningHost.closeRoom();
        // Validate TCP fragmentation/coalescing against a raw peer.
        LanRoom rawHost;
        require(rawHost.createRoom(), "raw host failed");
        QUdpSocket discovery;
        require(discovery.bind(QHostAddress(QHostAddress::LocalHost), quint16(0)), "probe bind failed");
        discovery.writeDatagram(
            QJsonDocument(QJsonObject{{"tag", "ChineseChess.LAN.2"}, {"query", rawHost.roomCode()}})
                .toJson(QJsonDocument::Compact),
            QHostAddress::LocalHost, 45454);
        waitFor([&] { return discovery.hasPendingDatagrams(); }, "raw discovery failed");
        const auto ad = QJsonDocument::fromJson(discovery.receiveDatagram().data()).object();
        QTcpSocket raw;
        raw.connectToHost(QHostAddress::LocalHost, quint16(ad.value("port").toInt()));
        waitFor([&] { return raw.state() == QTcpSocket::ConnectedState; }, "raw TCP failed");
        const auto handshake = QJsonDocument(QJsonObject{{"type", "join"},
                                                         {"protocol", 2},
                                                         {"room", rawHost.roomCode()},
                                                         {"session", ad.value("session")}})
                                   .toJson(QJsonDocument::Compact) +
                               "\n";
        raw.write(handshake.left(9));
        QTest::qWait(20);
        require(!rawHost.isConnected(), "partial handshake accepted");
        raw.write(handshake.mid(9) + "{\"type\":\"sync\"}\n{\"type\":\"sync\"}\n");
        waitFor([&] { return rawHost.isConnected(); }, "fragmented handshake failed");
        QSignalSpy messages(&rawHost, &LanRoom::messageReceived);
        rawHost.startMessages();
        require(messages.size() == 2, "coalesced or pre-attach messages lost");
        raw.write("bad json\n");
        waitFor([&] { return !rawHost.isConnected(); }, "malformed framing did not disconnect");
        std::cout << "PASS room dialog, explicit IPv4 discovery, fragmented TCP frames, combined frames, "
                     "queued initial sync, malformed JSON handling"
                  << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
