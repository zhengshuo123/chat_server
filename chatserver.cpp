#include "chatserver.h"
#include "protocol/protocolcodec.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>
#include <QUuid>
#include <QTcpSocket>
#include <QTimer>

namespace
{
constexpr qint64 maxProtocolLineBytes =
    ProtocolCodec::maxFrameBytes;

constexpr int maxNicknameLength = 20;
constexpr int maxMessageLength = 500;
constexpr qint64 maxInlineFileBytes = 512 * 1024;
constexpr int connectionTimeoutSeconds = 45;

QString currentTimestamp()
{
    /*
     * 直接获得当前 UTC 时间戳。
     *
     * 不再先调用 currentDateTimeUtc()
     * 创建一个 QDateTime 对象。
     */
    return QString::number(
        QDateTime::currentMSecsSinceEpoch());
}
}

ChatServer::ChatServer(quint16 port)
    : m_port(port)
    , m_timeoutTimer(new QTimer(&m_server))
{
    QObject::connect(
        &m_server,
        &QTcpServer::newConnection,
        &m_server,
        [this]()
        {
            handleNewConnections();
        });

    m_timeoutTimer->setInterval(10000);
    QObject::connect(
        m_timeoutTimer,
        &QTimer::timeout,
        &m_server,
        [this]()
        {
            checkConnectionTimeouts();
        });
}

bool ChatServer::start()
{
    const QString databasePath =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("chat_server.sqlite"));

    if (!m_repository.open(
            databasePath,
            QStringLiteral("chat_server_main")))
    {
        qCritical() << "Database open failed:"
                    << m_repository.lastError();
        return false;
    }

    if (!m_repository.initializeSchema())
    {
        qCritical() << "Database schema initialization failed:"
                    << m_repository.lastError();
        return false;
    }

    if (!m_repository.createConversation(
            QStringLiteral("hall"),
            QStringLiteral("group"),
            QStringLiteral("公共聊天室")))
    {
        qCritical() << "Hall conversation initialization failed:"
                    << m_repository.lastError();
        return false;
    }

    m_authService =
        std::make_unique<AuthService>(m_repository);

    if (!m_server.listen(
            QHostAddress::AnyIPv4,
            m_port))
    {
        qCritical() << "Server listen failed:"
                    << m_server.errorString();

        return false;
    }

    qInfo() << "Server is listening on"
            << m_server.serverAddress().toString()
            << ":"
            << m_server.serverPort();

    m_timeoutTimer->start();

    return true;
}

void ChatServer::handleNewConnections()
{
    while (m_server.hasPendingConnections())
    {
        QTcpSocket *clientSocket =
            m_server.nextPendingConnection();

        if (clientSocket == nullptr)
        {
            continue;
        }

        m_clients.insert(
            clientSocket,
            ClientInfo{
                QString{},
                false,
                QByteArray{},
                QDateTime::currentDateTimeUtc(),
                QSet<QString>{}});

        qInfo() << "New TCP connection:"
                << clientSocket->peerAddress().toString()
                << ":"
                << clientSocket->peerPort();

        qInfo() << "TCP connections:"
                << m_clients.size();

        QObject::connect(
            clientSocket,
            &QTcpSocket::readyRead,
            &m_server,
            [this, clientSocket]()
            {
                handleReadyRead(clientSocket);
            });

        QObject::connect(
            clientSocket,
            &QTcpSocket::disconnected,
            &m_server,
            [this, clientSocket]()
            {
                handleDisconnected(clientSocket);
            });
    }
}

void ChatServer::handleReadyRead(
    QTcpSocket *clientSocket)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end())
    {
        return;
    }

    ClientInfo &clientInfo =
        clientIt.value();

    clientInfo.inputBuffer.append(
        clientSocket->readAll());
    clientInfo.lastActivityUtc =
        QDateTime::currentDateTimeUtc();

    while (true)
    {
        ProtocolCodec::DecodeResult result =
            ProtocolCodec::tryDecode(
                clientInfo.inputBuffer);

        if (result.status ==
            ProtocolCodec::DecodeStatus::NeedMoreData)
        {
            break;
        }

        if (result.status ==
            ProtocolCodec::DecodeStatus::InvalidFrame)
        {
            sendError(clientSocket, result.error);
            clientSocket->disconnectFromHost();
            break;
        }

        handleJsonMessage(clientSocket, result.message);
    }

    if (clientInfo.inputBuffer.size() >
        maxProtocolLineBytes)
    {
        sendError(
            clientSocket,
            QStringLiteral("协议消息过长"));

        clientSocket->disconnectFromHost();
    }
}

void ChatServer::handleJsonMessage(
    QTcpSocket *clientSocket,
    const QJsonObject &messageObject)
{
    const QJsonObject normalizedObject =
        normalizeMessage(messageObject);

    const QString type =
        normalizedObject
            .value(QStringLiteral("type"))
            .toString();
    const QString requestId =
        normalizedObject
            .value(QStringLiteral("request_id"))
            .toString();

    if (shouldIgnoreDuplicateRequest(
            clientSocket,
            type,
            requestId))
    {
        return;
    }

    if (type == QStringLiteral("login"))
    {
        const QString username =
            normalizedObject
                .value(QStringLiteral("nickname"))
                .toString();
        const QString password =
            normalizedObject
                .value(QStringLiteral("password"))
                .toString();

        handleLogin(
            clientSocket,
            username,
            password);

        return;
    }

    if (type == QStringLiteral("register"))
    {
        const QString username =
            normalizedObject
                .value(QStringLiteral("nickname"))
                .toString();
        const QString password =
            normalizedObject
                .value(QStringLiteral("password"))
                .toString();

        handleRegister(
            clientSocket,
            username,
            password);

        return;
    }

    if (type == QStringLiteral("message"))
    {
        const QString message =
            normalizedObject
                .value(QStringLiteral("content"))
                .toString();

        handleChatMessage(
            clientSocket,
            message);

        return;
    }

    if (type ==
        QStringLiteral("private_message"))
    {
        const QString targetNickname =
            normalizedObject
                .value(QStringLiteral("target"))
                .toString();

        const QString message =
            normalizedObject
                .value(QStringLiteral("content"))
                .toString();

        handlePrivateMessage(
            clientSocket,
            targetNickname,
            message);

        return;
    }

    if (type == QStringLiteral("file_message"))
    {
        handleFileMessage(
            clientSocket,
            normalizedObject
                .value(QStringLiteral("target"))
                .toString(),
            normalizedObject
                .value(QStringLiteral("file_name"))
                .toString(),
            static_cast<qint64>(
                normalizedObject
                    .value(QStringLiteral("size"))
                    .toDouble()),
            normalizedObject
                .value(QStringLiteral("data_base64"))
                .toString());

        return;
    }

    if (type == QStringLiteral("history"))
    {
        const QString conversationId =
            normalizedObject
                .value(QStringLiteral("conversation_id"))
                .toString(QStringLiteral("hall"));
        const int limit =
            normalizedObject
                .value(QStringLiteral("limit"))
                .toInt(50);

        handleHistoryRequest(
            clientSocket,
            conversationId,
            limit);

        return;
    }

    if (type == QStringLiteral("ping"))
    {
        handlePing(clientSocket);
        return;
    }

    sendError(
        clientSocket,
        QStringLiteral("无法识别的消息类型"));
}

void ChatServer::handleLogin(
    QTcpSocket *clientSocket,
    const QString &username,
    const QString &password)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end())
    {
        return;
    }

    ClientInfo &clientInfo =
        clientIt.value();

    const auto sendLoginError =
        [this, clientSocket](
            const QString &reason)
    {
        QJsonObject resultObject;

        resultObject.insert(
            QStringLiteral("type"),
            QStringLiteral("login_result"));

        resultObject.insert(
            QStringLiteral("success"),
            false);

        resultObject.insert(
            QStringLiteral("reason"),
            reason);

        sendJson(
            clientSocket,
            resultObject);
    };

    if (clientInfo.loggedIn)
    {
        sendLoginError(
            QStringLiteral(
                "当前连接已经登录"));

        return;
    }

    const QString trimmedNickname =
        username.trimmed();

    if (trimmedNickname.isEmpty())
    {
        sendLoginError(
            QStringLiteral("昵称不能为空"));

        return;
    }

    if (trimmedNickname.length() >
        maxNicknameLength)
    {
        sendLoginError(
            QStringLiteral(
                "昵称不能超过20个字符"));

        return;
    }

    if (trimmedNickname.contains(
            QLatin1Char('\n'))
        || trimmedNickname.contains(
            QLatin1Char('\r')))
    {
        sendLoginError(
            QStringLiteral("昵称包含非法字符"));

        return;
    }

    if (nicknameInUse(
            trimmedNickname,
            clientSocket))
    {
        sendLoginError(
            QStringLiteral(
                "该昵称已经被使用"));

        return;
    }

    if (!password.isEmpty())
    {
        if (!m_authService)
        {
            sendLoginError(QStringLiteral("认证服务不可用"));
            return;
        }

        const AuthService::Result result =
            m_authService->verifyLogin(
                trimmedNickname,
                password);

        if (!result.success)
        {
            sendLoginError(result.message);
            return;
        }
    }

    completeLogin(clientSocket, trimmedNickname);
}

void ChatServer::handleRegister(
    QTcpSocket *clientSocket,
    const QString &username,
    const QString &password)
{
    if (!m_authService)
    {
        sendError(
            clientSocket,
            QStringLiteral("认证服务不可用"));
        return;
    }

    const QString trimmedUsername =
        username.trimmed();

    if (trimmedUsername.length() >
        maxNicknameLength)
    {
        sendError(
            clientSocket,
            QStringLiteral("用户名不能超过20个字符"));
        return;
    }

    if (nicknameInUse(
            trimmedUsername,
            clientSocket))
    {
        sendError(
            clientSocket,
            QStringLiteral("该用户已经在线"));
        return;
    }

    const AuthService::Result result =
        m_authService->registerUser(
            trimmedUsername,
            password);

    if (!result.success)
    {
        QJsonObject resultObject;
        resultObject.insert(
            QStringLiteral("type"),
            QStringLiteral("login_result"));
        resultObject.insert(
            QStringLiteral("success"),
            false);
        resultObject.insert(
            QStringLiteral("reason"),
            result.message);
        sendJson(clientSocket, resultObject);
        return;
    }

    completeLogin(clientSocket, trimmedUsername);
}

void ChatServer::completeLogin(
    QTcpSocket *clientSocket,
    const QString &username)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end())
    {
        return;
    }

    ClientInfo &clientInfo =
        clientIt.value();

    clientInfo.nickname =
        username;

    clientInfo.loggedIn = true;

    qInfo() << "Client logged in:"
            << username
            << clientSocket->peerAddress().toString()
            << ":"
            << clientSocket->peerPort();

    QJsonObject resultObject;

    resultObject.insert(
        QStringLiteral("type"),
        QStringLiteral("login_result"));

    resultObject.insert(
        QStringLiteral("success"),
        true);

    sendJson(
        clientSocket,
        resultObject);

    broadcastSystemMessage(
        QStringLiteral("%1 上线了")
            .arg(username));

    broadcastUserList();
}

void ChatServer::handleChatMessage(
    QTcpSocket *clientSocket,
    const QString &message)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end())
    {
        return;
    }

    const ClientInfo &clientInfo =
        clientIt.value();

    if (!clientInfo.loggedIn)
    {
        sendError(
            clientSocket,
            QStringLiteral("请先登录"));

        return;
    }

    const QString trimmedMessage =
        message.trimmed();

    if (trimmedMessage.isEmpty())
    {
        return;
    }

    if (trimmedMessage.length() >
        maxMessageLength)
    {
        sendError(
            clientSocket,
            QStringLiteral(
                "消息不能超过500个字符"));

        return;
    }

    qInfo() << "Group message from"
            << clientInfo.nickname
            << ":"
            << trimmedMessage;

    QJsonObject chatObject;

    chatObject.insert(
        QStringLiteral("type"),
        QStringLiteral("chat"));

    chatObject.insert(
        QStringLiteral("nickname"),
        clientInfo.nickname);

    chatObject.insert(
        QStringLiteral("content"),
        trimmedMessage);

    chatObject.insert(
        QStringLiteral("timestamp"),
        currentTimestamp());

    broadcastJson(chatObject);

    if (!m_repository.appendMessage(
            QStringLiteral("hall"),
            clientInfo.nickname,
            QStringLiteral("text"),
            trimmedMessage,
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()))
    {
        qWarning() << "Failed to persist group message:"
                   << m_repository.lastError();
    }
}

void ChatServer::handlePrivateMessage(
    QTcpSocket *clientSocket,
    const QString &targetNickname,
    const QString &message)
{
    auto senderIt =
        m_clients.find(clientSocket);

    if (senderIt == m_clients.end())
    {
        return;
    }

    const ClientInfo &senderInfo =
        senderIt.value();

    if (!senderInfo.loggedIn)
    {
        sendError(
            clientSocket,
            QStringLiteral("请先登录"));

        return;
    }

    const QString trimmedTarget =
        targetNickname.trimmed();

    const QString trimmedMessage =
        message.trimmed();

    if (trimmedTarget.isEmpty())
    {
        sendError(
            clientSocket,
            QStringLiteral("私聊对象不能为空"));

        return;
    }

    if (trimmedMessage.isEmpty())
    {
        return;
    }

    if (trimmedMessage.length() >
        maxMessageLength)
    {
        sendError(
            clientSocket,
            QStringLiteral(
                "消息不能超过500个字符"));

        return;
    }

    if (senderInfo.nickname.compare(
            trimmedTarget,
            Qt::CaseInsensitive) == 0)
    {
        sendError(
            clientSocket,
            QStringLiteral(
                "不能给自己发送私聊消息"));

        return;
    }

    QTcpSocket *targetSocket =
        findClientByNickname(
            trimmedTarget);

    if (targetSocket == nullptr)
    {
        sendError(
            clientSocket,
            QStringLiteral(
                "私聊对象当前不在线"));

        return;
    }

    QJsonObject privateChatObject;

    privateChatObject.insert(
        QStringLiteral("type"),
        QStringLiteral("private_chat"));

    privateChatObject.insert(
        QStringLiteral("from"),
        senderInfo.nickname);

    privateChatObject.insert(
        QStringLiteral("to"),
        trimmedTarget);

    privateChatObject.insert(
        QStringLiteral("content"),
        trimmedMessage);

    privateChatObject.insert(
        QStringLiteral("timestamp"),
        currentTimestamp());

    const QString conversationId =
        directConversationId(
            senderInfo.nickname,
            trimmedTarget);
    privateChatObject.insert(
        QStringLiteral("conversation_id"),
        conversationId);

    sendJson(
        targetSocket,
        privateChatObject);

    sendJson(
        clientSocket,
        privateChatObject);

    qInfo() << "Private message from"
            << senderInfo.nickname
            << "to"
            << trimmedTarget
            << ":"
            << trimmedMessage;

    if (!m_repository.createConversation(
            conversationId,
            QStringLiteral("direct"),
            QStringLiteral("%1 / %2")
                .arg(senderInfo.nickname, trimmedTarget)))
    {
        qWarning() << "Failed to ensure direct conversation:"
                   << m_repository.lastError();
    }

    if (!m_repository.appendMessage(
            conversationId,
            senderInfo.nickname,
            QStringLiteral("text"),
            trimmedMessage,
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()))
    {
        qWarning() << "Failed to persist private message:"
                   << m_repository.lastError();
    }
}

void ChatServer::handleFileMessage(
    QTcpSocket *clientSocket,
    const QString &targetNickname,
    const QString &fileName,
    qint64 size,
    const QString &base64Data)
{
    auto senderIt =
        m_clients.find(clientSocket);

    if (senderIt == m_clients.end()
        || !senderIt.value().loggedIn)
    {
        sendError(clientSocket, QStringLiteral("请先登录"));
        return;
    }

    const ClientInfo &senderInfo =
        senderIt.value();
    const QString trimmedFileName =
        fileName.trimmed();
    const QString trimmedTarget =
        targetNickname.trimmed();

    if (trimmedFileName.isEmpty()
        || size <= 0
        || size > maxInlineFileBytes)
    {
        sendError(clientSocket, QStringLiteral("文件无效或超过 512 KB"));
        return;
    }

    const QByteArray fileData =
        QByteArray::fromBase64(base64Data.toLatin1());

    if (fileData.size() != size)
    {
        sendError(clientSocket, QStringLiteral("文件数据无效"));
        return;
    }

    QJsonObject fileChatObject;
    fileChatObject.insert(QStringLiteral("type"), QStringLiteral("file_chat"));
    fileChatObject.insert(QStringLiteral("from"), senderInfo.nickname);
    fileChatObject.insert(QStringLiteral("to"), trimmedTarget);
    fileChatObject.insert(QStringLiteral("file_name"), trimmedFileName);
    fileChatObject.insert(QStringLiteral("size"), size);
    fileChatObject.insert(QStringLiteral("data_base64"), base64Data);
    fileChatObject.insert(QStringLiteral("timestamp"), currentTimestamp());

    QString conversationId = QStringLiteral("hall");

    if (trimmedTarget.isEmpty())
    {
        fileChatObject.insert(QStringLiteral("conversation_id"), conversationId);
        broadcastJson(fileChatObject);
    }
    else
    {
        QTcpSocket *targetSocket =
            findClientByNickname(trimmedTarget);

        if (targetSocket == nullptr)
        {
            sendError(clientSocket, QStringLiteral("私聊对象当前不在线"));
            return;
        }

        conversationId =
            directConversationId(senderInfo.nickname, trimmedTarget);
        fileChatObject.insert(QStringLiteral("conversation_id"), conversationId);

        sendJson(targetSocket, fileChatObject);
        sendJson(clientSocket, fileChatObject);

        if (!m_repository.createConversation(
                conversationId,
                QStringLiteral("direct"),
                QStringLiteral("%1 / %2")
                    .arg(senderInfo.nickname, trimmedTarget)))
        {
            qWarning() << "Failed to ensure direct file conversation:"
                       << m_repository.lastError();
        }
    }

    if (!m_repository.appendMessage(
            conversationId,
            senderInfo.nickname,
            QStringLiteral("file"),
            trimmedFileName,
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()))
    {
        qWarning() << "Failed to persist file message:"
                   << m_repository.lastError();
    }
}

void ChatServer::handleHistoryRequest(
    QTcpSocket *clientSocket,
    const QString &conversationId,
    int limit)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end()
        || !clientIt.value().loggedIn)
    {
        sendError(
            clientSocket,
            QStringLiteral("请先登录"));
        return;
    }

    const QString normalizedConversationId =
        conversationId.trimmed().isEmpty()
            ? QStringLiteral("hall")
            : conversationId.trimmed();
    const QList<SQLiteRepository::StoredMessage> messages =
        m_repository.messagesForConversation(
            normalizedConversationId,
            qBound(1, limit, 100));

    QJsonArray messageArray;

    for (const SQLiteRepository::StoredMessage &message : messages)
    {
        QJsonObject messageObject;
        messageObject.insert(QStringLiteral("id"), QString::number(message.id));
        messageObject.insert(QStringLiteral("conversation_id"), message.conversationId);
        messageObject.insert(QStringLiteral("sender"), message.senderUsername);
        messageObject.insert(QStringLiteral("kind"), message.kind);
        messageObject.insert(QStringLiteral("content"), message.content);
        messageObject.insert(
            QStringLiteral("created_at"),
            message.createdAt.toUTC().toString(Qt::ISODateWithMs));
        messageObject.insert(QStringLiteral("status"), message.status);
        messageArray.append(messageObject);
    }

    QJsonObject resultObject;
    resultObject.insert(QStringLiteral("type"), QStringLiteral("history_result"));
    resultObject.insert(QStringLiteral("conversation_id"), normalizedConversationId);
    resultObject.insert(QStringLiteral("messages"), messageArray);

    sendJson(clientSocket, resultObject);
}

void ChatServer::handlePing(
    QTcpSocket *clientSocket)
{
    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt != m_clients.end())
    {
        clientIt.value().lastActivityUtc =
            QDateTime::currentDateTimeUtc();
    }

    QJsonObject pongObject;
    pongObject.insert(QStringLiteral("type"), QStringLiteral("pong"));
    sendJson(clientSocket, pongObject);
}

void ChatServer::checkConnectionTimeouts()
{
    const QDateTime now =
        QDateTime::currentDateTimeUtc();
    QList<QTcpSocket *> timedOutSockets;

    for (auto clientIt = m_clients.cbegin();
         clientIt != m_clients.cend();
         ++clientIt)
    {
        if (clientIt.value().lastActivityUtc.secsTo(now)
            > connectionTimeoutSeconds)
        {
            timedOutSockets.append(clientIt.key());
        }
    }

    for (QTcpSocket *socket : timedOutSockets)
    {
        sendError(
            socket,
            QStringLiteral("连接超时"));
        socket->disconnectFromHost();
    }
}

bool ChatServer::shouldIgnoreDuplicateRequest(
    QTcpSocket *clientSocket,
    const QString &type,
    const QString &requestId)
{
    if (requestId.isEmpty())
    {
        return false;
    }

    const bool mutatingRequest =
        type == QStringLiteral("register")
        || type == QStringLiteral("message")
        || type == QStringLiteral("private_message")
        || type == QStringLiteral("file_message");

    if (!mutatingRequest)
    {
        return false;
    }

    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt == m_clients.end())
    {
        return false;
    }

    ClientInfo &clientInfo =
        clientIt.value();

    if (clientInfo.processedRequestIds.contains(requestId))
    {
        qInfo() << "Ignoring duplicate request_id:"
                << requestId
                << "type:"
                << type;
        return true;
    }

    if (clientInfo.processedRequestIds.size() > 1024)
    {
        clientInfo.processedRequestIds.clear();
    }

    clientInfo.processedRequestIds.insert(requestId);
    return false;
}

void ChatServer::handleDisconnected(
    QTcpSocket *clientSocket)
{
    QString nickname;
    bool wasLoggedIn = false;

    auto clientIt =
        m_clients.find(clientSocket);

    if (clientIt != m_clients.end())
    {
        nickname =
            clientIt.value().nickname;

        wasLoggedIn =
            clientIt.value().loggedIn;
    }

    const QString address =
        clientSocket->peerAddress().toString();

    const quint16 port =
        clientSocket->peerPort();

    m_clients.remove(clientSocket);

    qInfo() << "Client disconnected:"
            << address
            << ":"
            << port;

    qInfo() << "TCP connections:"
            << m_clients.size();

    if (wasLoggedIn)
    {
        broadcastSystemMessage(
            QStringLiteral("%1 下线了")
                .arg(nickname));

        broadcastUserList();
    }

    clientSocket->deleteLater();
}

void ChatServer::sendJson(
    QTcpSocket *clientSocket,
    const QJsonObject &messageObject)
{
    if (clientSocket == nullptr)
    {
        return;
    }

    if (clientSocket->state() !=
        QAbstractSocket::ConnectedState)
    {
        return;
    }

    const QString type =
        messageObject
            .value(QStringLiteral("type"))
            .toString();

    if (type.isEmpty())
    {
        return;
    }

    QJsonObject payload = messageObject;
    payload.remove(QStringLiteral("type"));

    const QJsonObject envelope =
        ProtocolCodec::createEnvelope(
            type,
            QUuid::createUuid().toString(
                QUuid::WithoutBraces),
            payload);

    const QByteArray data =
        ProtocolCodec::encode(envelope);

    if (data.isEmpty())
    {
        qWarning() << "Failed to encode protocol frame";
        return;
    }

    const qint64 bytesWritten =
        clientSocket->write(data);

    if (bytesWritten == -1)
    {
        qWarning() << "Failed to send data:"
                   << clientSocket->errorString();
    }
}

QJsonObject ChatServer::normalizeMessage(
    const QJsonObject &messageObject) const
{
    const QString envelopeType =
        messageObject
            .value(QStringLiteral("type"))
            .toString();

    if (!messageObject.contains(QStringLiteral("version"))
        || !messageObject.value(QStringLiteral("payload")).isObject())
    {
        return messageObject;
    }

    QJsonObject normalized =
        messageObject
            .value(QStringLiteral("payload"))
            .toObject();

    normalized.insert(
        QStringLiteral("type"),
        envelopeType);

    if (messageObject.contains(QStringLiteral("timestamp")))
    {
        normalized.insert(
            QStringLiteral("timestamp"),
            messageObject.value(QStringLiteral("timestamp")));
    }

    if (messageObject.contains(QStringLiteral("request_id")))
    {
        normalized.insert(
            QStringLiteral("request_id"),
            messageObject.value(QStringLiteral("request_id")));
    }

    return normalized;
}

void ChatServer::sendError(
    QTcpSocket *clientSocket,
    const QString &errorMessage)
{
    QJsonObject errorObject;

    errorObject.insert(
        QStringLiteral("type"),
        QStringLiteral("error"));

    errorObject.insert(
        QStringLiteral("message"),
        errorMessage);

    sendJson(
        clientSocket,
        errorObject);
}

void ChatServer::broadcastJson(
    const QJsonObject &messageObject)
{
    int successfulCount = 0;

    for (auto clientIt = m_clients.cbegin();
         clientIt != m_clients.cend();
         ++clientIt)
    {
        QTcpSocket *clientSocket =
            clientIt.key();

        const ClientInfo &clientInfo =
            clientIt.value();

        if (!clientInfo.loggedIn)
        {
            continue;
        }

        if (clientSocket->state() !=
            QAbstractSocket::ConnectedState)
        {
            continue;
        }

        sendJson(
            clientSocket,
            messageObject);

        ++successfulCount;
    }

    qInfo() << "Broadcast to"
            << successfulCount
            << "logged-in clients";
}

void ChatServer::broadcastSystemMessage(
    const QString &message)
{
    QJsonObject systemObject;

    systemObject.insert(
        QStringLiteral("type"),
        QStringLiteral("system"));

    systemObject.insert(
        QStringLiteral("content"),
        message);

    systemObject.insert(
        QStringLiteral("timestamp"),
        currentTimestamp());

    broadcastJson(systemObject);
}

void ChatServer::broadcastUserList()
{
    const QStringList nicknames =
        onlineNicknames();

    QJsonObject userListObject;

    userListObject.insert(
        QStringLiteral("type"),
        QStringLiteral("user_list"));

    userListObject.insert(
        QStringLiteral("users"),
        QJsonArray::fromStringList(
            nicknames));

    broadcastJson(userListObject);
}

QStringList ChatServer::onlineNicknames() const
{
    QStringList nicknames;

    for (auto clientIt = m_clients.cbegin();
         clientIt != m_clients.cend();
         ++clientIt)
    {
        const ClientInfo &clientInfo =
            clientIt.value();

        if (!clientInfo.loggedIn)
        {
            continue;
        }

        nicknames.append(
            clientInfo.nickname);
    }

    nicknames.sort(
        Qt::CaseInsensitive);

    return nicknames;
}

QString ChatServer::directConversationId(
    const QString &firstUsername,
    const QString &secondUsername) const
{
    QStringList names{
        firstUsername.trimmed().toCaseFolded(),
        secondUsername.trimmed().toCaseFolded()};
    names.sort();

    return QStringLiteral("dm:%1:%2")
        .arg(names.value(0), names.value(1));
}

QTcpSocket *ChatServer::findClientByNickname(
    const QString &nickname) const
{
    for (auto clientIt = m_clients.cbegin();
         clientIt != m_clients.cend();
         ++clientIt)
    {
        const ClientInfo &clientInfo =
            clientIt.value();

        if (!clientInfo.loggedIn)
        {
            continue;
        }

        if (clientInfo.nickname.compare(
                nickname,
                Qt::CaseInsensitive) == 0)
        {
            return clientIt.key();
        }
    }

    return nullptr;
}

bool ChatServer::nicknameInUse(
    const QString &nickname,
    QTcpSocket *excludedSocket) const
{
    for (auto clientIt = m_clients.cbegin();
         clientIt != m_clients.cend();
         ++clientIt)
    {
        if (clientIt.key() == excludedSocket)
        {
            continue;
        }

        const ClientInfo &clientInfo =
            clientIt.value();

        if (!clientInfo.loggedIn)
        {
            continue;
        }

        if (clientInfo.nickname.compare(
                nickname,
                Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }

    return false;
}
