// SPDX-License-Identifier: GPL-3.0-or-later
#include "LanRoom.h"
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QUuid>
#include <QRegularExpression>

namespace {
const QString discoveryTag = QStringLiteral("ChineseChess.LAN.2");
}

LanRoom::LanRoom(QObject* parent) : QObject(parent)
{
    m_probeTimer.setInterval(700);
    m_deadline.setSingleShot(true);
    m_heartbeat.setInterval(3000);
    connect(&m_discovery, &QUdpSocket::readyRead, this, &LanRoom::readDatagrams);
    connect(&m_server, &QTcpServer::newConnection, this, &LanRoom::acceptConnection);
    connect(&m_probeTimer, &QTimer::timeout, this, &LanRoom::probe);
    connect(&m_deadline, &QTimer::timeout, this, [this] {
        if (m_host) loseConnection(QStringLiteral("对方连接超时"));
        else failJoin(m_roomBusy ? QStringLiteral("房间已有两位玩家，请等待空位后重试")
            : QStringLiteral("搜索超时，请检查房间号、同一局域网及防火墙设置"));
    });
    connect(&m_heartbeat, &QTimer::timeout, this, [this] {
        if (m_lastReceived.elapsed() > 12000) {
            loseConnection(QStringLiteral("连接中断，请重新加入房间"));
            return;
        }
        writeFrame({{"type", "ping"}});
    });
}

LanRoom::~LanRoom() { closeRoom(); }

void LanRoom::closeRoom()
{
    m_probeTimer.stop();
    m_deadline.stop();
    m_heartbeat.stop();
    m_server.close();
    m_discovery.close();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();
    m_pendingMessages.clear();
    m_connected = false;
    m_searching = false;
    m_deliverMessages = false;
}

bool LanRoom::createRoom()
{
    closeRoom();
    m_host = true;
    if (!m_discovery.bind(QHostAddress::AnyIPv4, DiscoveryPort,
                         QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)
        || !m_server.listen(QHostAddress::AnyIPv4, 0)) {
        const QString error = QStringLiteral("创建房间失败，请检查网络及端口占用");
        closeRoom();
        emit failed(error);
        return false;
    }
    m_code = QString::number(QRandomGenerator::global()->bounded(100000, 1000000));
    m_session = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit roomCreated(m_code);
    return true;
}

void LanRoom::joinRoom(const QString& code, const QString& hostHint)
{
    closeRoom();
    m_host = false;
    if (!QRegularExpression(QStringLiteral("^[0-9]{6}$")).match(code).hasMatch()) {
        emit failed(QStringLiteral("请输入六位数字房间号"));
        return;
    }
    if (!hostHint.isEmpty() && QHostAddress(hostHint).protocol() != QAbstractSocket::IPv4Protocol) {
        emit failed(QStringLiteral("房主地址请输入 IPv4 地址，例如 192.168.1.20"));
        return;
    }
    if (!m_discovery.bind(QHostAddress(QHostAddress::AnyIPv4), quint16(0))) {
        emit failed(QStringLiteral("无法开启房间搜索"));
        return;
    }
    m_code = code;
    m_hostHint = hostHint;
    m_searching = true;
    m_roomBusy = false;
    m_probeTimer.start();
    m_deadline.start(15000);
    emit statusChanged(QStringLiteral("正在查找房间 %1…").arg(code));
    probe();
}

QStringList LanRoom::localAddresses()
{
    QStringList addresses;
    for (const auto& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
            addresses.append(address.toString());
    }
    return addresses;
}

void LanRoom::probe()
{
    const QByteArray bytes = QJsonDocument(QJsonObject{{"tag", discoveryTag}, {"query", m_code}}).toJson(QJsonDocument::Compact);
    if (!m_hostHint.isEmpty()) {
        m_discovery.writeDatagram(bytes, QHostAddress(m_hostHint), DiscoveryPort);
        return;
    }
    m_discovery.writeDatagram(bytes, QHostAddress::Broadcast, DiscoveryPort);
    m_discovery.writeDatagram(bytes, QHostAddress::LocalHost, DiscoveryPort);
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        for (const auto& entry : iface.addressEntries()) {
            if (!entry.broadcast().isNull())
                m_discovery.writeDatagram(bytes, entry.broadcast(), DiscoveryPort);
        }
    }
}

void LanRoom::readDatagrams()
{
    while (m_discovery.hasPendingDatagrams()) {
        const auto datagram = m_discovery.receiveDatagram(4096);
        const auto message = QJsonDocument::fromJson(datagram.data()).object();
        if (message.value("tag").toString() != discoveryTag) continue;
        if (m_host) {
            if (message.value("query").toString() != m_code) continue;
            QJsonObject reply{{"tag", discoveryTag}, {"room", m_code}, {"session", m_session},
                              {"port", m_server.serverPort()}, {"busy", m_socket != nullptr}};
            m_discovery.writeDatagram(QJsonDocument(reply).toJson(QJsonDocument::Compact),
                                      datagram.senderAddress(), datagram.senderPort());
        } else if (m_searching && !m_socket && message.value("room").toString() == m_code) {
            if (message.value("busy").toBool()) {
                m_roomBusy = true;
                emit statusChanged(QStringLiteral("房间已有两位玩家，正在等待空位…"));
                continue;
            }
            const int port = message.value("port").toInt();
            if (port < 1 || port > 65535 || message.value("session").toString().isEmpty()) continue;
            m_session = message.value("session").toString();
            m_hostHint = datagram.senderAddress().toString();
            attachSocket(new QTcpSocket(this));
            connect(m_socket, &QTcpSocket::connected, this, [this] {
                writeFrame({{"type", "join"}, {"protocol", 2}, {"room", m_code}, {"session", m_session}});
            });
            m_socket->connectToHost(datagram.senderAddress(), quint16(port));
        }
    }
}

void LanRoom::acceptConnection()
{
    while (m_server.hasPendingConnections()) {
        auto* socket = m_server.nextPendingConnection();
        if (m_socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }
        attachSocket(socket);
        m_deadline.start(5000);
    }
}

void LanRoom::attachSocket(QTcpSocket* socket)
{
    m_socket = socket;
    socket->setParent(this);
    socket->setReadBufferSize(MaxFrameBytes + 1);
    socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    connect(socket, &QTcpSocket::readyRead, this, &LanRoom::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, [this] { loseConnection(QStringLiteral("对方已离开房间")); });
    const auto onError = [this](QAbstractSocket::SocketError) {
        loseConnection(QStringLiteral("网络连接失败或已中断"));
    };
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(socket, &QTcpSocket::errorOccurred, this, onError);
#else
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, onError);
#endif
}

void LanRoom::readSocket()
{
    if (!m_socket) return;
    m_buffer += m_socket->readAll();
    while (m_socket) {
        const auto end = m_buffer.indexOf('\n');
        if (end < 0) break;
        if (end > MaxFrameBytes) { loseConnection(QStringLiteral("网络消息过长")); return; }
        const auto line = m_buffer.left(end);
        m_buffer.remove(0, end + 1);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            loseConnection(QStringLiteral("收到无效的网络消息"));
            return;
        }
        handleFrame(document.object());
    }
    if (m_buffer.size() > MaxFrameBytes) loseConnection(QStringLiteral("网络消息过长"));
}

void LanRoom::handleFrame(const QJsonObject& message)
{
    const QString type = message.value("type").toString();
    if (!m_connected) {
        const bool valid = m_host
            ? type == "join" && message.value("room").toString() == m_code
                && message.value("session").toString() == m_session && message.value("protocol").toInt() == 2
            : type == "welcome" && message.value("protocol").toInt() == 2;
        if (!valid) { loseConnection(QStringLiteral("房间信息不匹配，请重新加入")); return; }
        if (m_host) writeFrame({{"type", "welcome"}, {"protocol", 2}});
        m_connected = true;
        m_searching = false;
        m_probeTimer.stop();
        m_deadline.stop();
        m_lastReceived.start();
        m_heartbeat.start();
        emit connected();
        return;
    }
    m_lastReceived.restart();
    if (type == "ping") { writeFrame({{"type", "pong"}}); return; }
    if (type == "pong") return;
    if (m_deliverMessages) emit messageReceived(message);
    else if (m_pendingMessages.size() < 64) m_pendingMessages.enqueue(message);
    else loseConnection(QStringLiteral("网络消息积压，请重新加入"));
}

void LanRoom::startMessages()
{
    m_deliverMessages = true;
    while (m_connected && !m_pendingMessages.isEmpty()) emit messageReceived(m_pendingMessages.dequeue());
}

void LanRoom::writeFrame(const QJsonObject& message)
{
    if (!m_socket) return;
    const QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    if (data.size() > MaxFrameBytes || m_socket->bytesToWrite() > MaxFrameBytes * 2) {
        loseConnection(QStringLiteral("网络发送队列已满"));
        return;
    }
    m_socket->write(data);
}

bool LanRoom::sendMessage(const QJsonObject& message)
{
    if (!m_connected) return false;
    writeFrame(message);
    return m_connected;
}

void LanRoom::loseConnection(const QString& reason)
{
    const bool wasConnected = m_connected;
    m_connected = false;
    m_heartbeat.stop();
    m_deadline.stop();
    if (m_socket) {
        auto* socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_buffer.clear();
    m_pendingMessages.clear();
    if (wasConnected) emit disconnected(reason);
    else if (!m_host) failJoin(reason);
}

void LanRoom::failJoin(const QString& reason)
{
    closeRoom();
    emit failed(reason);
}
