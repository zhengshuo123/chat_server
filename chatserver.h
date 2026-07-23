#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <QHash>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QTcpServer>

class QTcpSocket;

class ChatServer
{
public:
    explicit ChatServer(quint16 port);

    bool start();

private:
    struct ClientInfo
    {
        QString nickname;
        bool loggedIn = false;
        QByteArray inputBuffer;
    };

private:
    void handleNewConnections();

    void handleReadyRead(
        QTcpSocket *clientSocket);

    void handleJsonMessage(
        QTcpSocket *clientSocket,
        const QJsonObject &messageObject);

    QJsonObject normalizeMessage(
        const QJsonObject &messageObject) const;

    void handleLogin(
        QTcpSocket *clientSocket,
        const QString &nickname);

    void handleChatMessage(
        QTcpSocket *clientSocket,
        const QString &message);

    void handlePrivateMessage(
        QTcpSocket *clientSocket,
        const QString &targetNickname,
        const QString &message);

    void handleDisconnected(
        QTcpSocket *clientSocket);

    void sendJson(
        QTcpSocket *clientSocket,
        const QJsonObject &messageObject);

    void sendError(
        QTcpSocket *clientSocket,
        const QString &errorMessage);

    void broadcastJson(
        const QJsonObject &messageObject);

    void broadcastSystemMessage(
        const QString &message);

    void broadcastUserList();

    QStringList onlineNicknames() const;

    QTcpSocket *findClientByNickname(
        const QString &nickname) const;

    bool nicknameInUse(
        const QString &nickname,
        QTcpSocket *excludedSocket = nullptr) const;

private:
    QHash<QTcpSocket *, ClientInfo> m_clients;

    quint16 m_port;
    QTcpServer m_server;
};

#endif // CHATSERVER_H
