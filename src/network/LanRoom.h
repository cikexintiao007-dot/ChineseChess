// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LANROOM_H
#define LANROOM_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QQueue>

class LanRoom : public QObject
{
    Q_OBJECT
public:
    explicit LanRoom(QObject* parent = nullptr);
    ~LanRoom() override;
    bool createRoom();
    void joinRoom(const QString& code, const QString& hostHint = QString());
    void closeRoom();
    void startMessages();
    bool sendMessage(const QJsonObject& message);
    bool isHost() const { return m_host; }
    bool isConnected() const { return m_connected; }
    bool isSearching() const { return m_searching; }
    QString roomCode() const { return m_code; }
    QString hostAddress() const { return m_hostHint; }
    static QStringList localAddresses();

signals:
    void roomCreated(const QString& code);
    void connected();
    void disconnected(const QString& reason);
    void failed(const QString& reason);
    void statusChanged(const QString& text);
    void messageReceived(const QJsonObject& message);

private:
    void probe();
    void readDatagrams();
    void acceptConnection();
    void attachSocket(QTcpSocket* socket);
    void readSocket();
    void handleFrame(const QJsonObject& message);
    void loseConnection(const QString& reason);
    void failJoin(const QString& reason);
    void writeFrame(const QJsonObject& message);

    static constexpr quint16 DiscoveryPort = 45454;
    static constexpr int MaxFrameBytes = 512 * 1024;
    QTcpServer m_server;
    QUdpSocket m_discovery;
    QTcpSocket* m_socket = nullptr;
    QTimer m_probeTimer;
    QTimer m_deadline;
    QTimer m_heartbeat;
    QElapsedTimer m_lastReceived;
    QByteArray m_buffer;
    QQueue<QJsonObject> m_pendingMessages;
    QString m_code;
    QString m_session;
    QString m_hostHint;
    bool m_host = false;
    bool m_searching = false;
    bool m_connected = false;
    bool m_deliverMessages = false;
    bool m_roomBusy = false;
};

#endif
