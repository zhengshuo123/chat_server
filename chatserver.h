#ifndef CHATSERVER_H
#define CHATSERVER_H

#include "client_session.h"
#include "persistence/sqliterepository.h"
#include "services/authservice.h"

#include <QHash>
#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QSet>
#include <memory>
#include <QString>
#include <QStringList>
#include <QTcpServer>

class QTcpSocket;
class QTimer;

class ChatServer
{
public:
    explicit ChatServer(quint16 port);

    bool start();

    struct PendingUpload
    {
        QTcpSocket *ownerSocket = nullptr;
        QString senderNickname;
        QString targetNickname;
        QString conversationId;
        QString uploadId;
        QString fileName;
        QString storagePath;
        qint64 size = 0;
        qint64 received = 0;
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
        const QString &username,
        const QString &password);

    void handleRegister(
        QTcpSocket *clientSocket,
        const QString &username,
        const QString &password);

    void completeLogin(
        QTcpSocket *clientSocket,
        const QString &username);

    void handleChatMessage(
        QTcpSocket *clientSocket,
        const QString &message);

    void handlePrivateMessage(
        QTcpSocket *clientSocket,
        const QString &targetNickname,
        const QString &message);

    void handleFileMessage(
        QTcpSocket *clientSocket,
        const QString &targetNickname,
        const QString &fileName,
        qint64 size,
        const QString &base64Data);

    void handleFileUploadStart(
        QTcpSocket *clientSocket,
        const QString &uploadId,
        const QString &targetNickname,
        const QString &fileName,
        qint64 size);

    void handleFileUploadChunk(
        QTcpSocket *clientSocket,
        const QString &uploadId,
        qint64 offset,
        const QString &base64Data);

    void completeFileUpload(
        const PendingUpload &upload);

    void handleFileDownload(
        QTcpSocket *clientSocket,
        qint64 attachmentId);

    void handleHistoryRequest(
        QTcpSocket *clientSocket,
        const QString &conversationId,
        int limit);

    void handleConversationListRequest(
        QTcpSocket *clientSocket);

    void handleMarkReadRequest(
        QTcpSocket *clientSocket,
        const QString &conversationId);

    void handlePing(
        QTcpSocket *clientSocket);

    void checkConnectionTimeouts();

    bool shouldIgnoreDuplicateRequest(
        QTcpSocket *clientSocket,
        const QString &type,
        const QString &requestId);

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

    QString directConversationId(
        const QString &firstUsername,
        const QString &secondUsername) const;

    QTcpSocket *findClientByNickname(
        const QString &nickname) const;

    bool nicknameInUse(
        const QString &nickname,
        QTcpSocket *excludedSocket = nullptr) const;

    QString attachmentStoragePath(
        const QString &fileName) const;

    QString safeAttachmentFileName(
        const QString &fileName) const;

private:
    QHash<QTcpSocket *, ClientSession> m_clients;
    QHash<QString, PendingUpload> m_pendingUploads;

    quint16 m_port;
    QTcpServer m_server;
    QTimer *m_timeoutTimer;
    SQLiteRepository m_repository;
    std::unique_ptr<AuthService> m_authService;
};

#endif // CHATSERVER_H
