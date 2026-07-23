#include "chatserver.h"

#include <QAbstractSocket>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QObject>
#include <QTcpSocket>

namespace
{
constexpr qint64 maxProtocolLineBytes =
    64 * 1024;

constexpr int maxNicknameLength = 20;
constexpr int maxMessageLength = 500;

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
{
    QObject::connect(
        &m_server,
        &QTcpServer::newConnection,
        &m_server,
        [this]()
        {
            handleNewConnections();
        });
}

bool ChatServer::start()
{
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
            ClientInfo{});

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
    while (clientSocket->canReadLine())
    {
        QByteArray lineData =
            clientSocket->readLine(
                maxProtocolLineBytes + 1);

        if (lineData.size() >
            maxProtocolLineBytes)
        {
            sendError(
                clientSocket,
                QStringLiteral("协议消息过长"));

            clientSocket->disconnectFromHost();

            return;
        }

        if (lineData.endsWith('\n'))
        {
            lineData.chop(1);
        }

        if (lineData.endsWith('\r'))
        {
            lineData.chop(1);
        }

        QJsonParseError parseError;

        const QJsonDocument document =
            QJsonDocument::fromJson(
                lineData,
                &parseError);

        if (parseError.error !=
                QJsonParseError::NoError
            || !document.isObject())
        {
            sendError(
                clientSocket,
                QStringLiteral(
                    "收到无效的 JSON 消息"));

            continue;
        }

        handleJsonMessage(
            clientSocket,
            document.object());
    }

    if (clientSocket->bytesAvailable() >
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
    const QString type =
        messageObject
            .value(QStringLiteral("type"))
            .toString();

    if (type == QStringLiteral("login"))
    {
        const QString nickname =
            messageObject
                .value(QStringLiteral("nickname"))
                .toString();

        handleLogin(
            clientSocket,
            nickname);

        return;
    }

    if (type == QStringLiteral("message"))
    {
        const QString message =
            messageObject
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
            messageObject
                .value(QStringLiteral("target"))
                .toString();

        const QString message =
            messageObject
                .value(QStringLiteral("content"))
                .toString();

        handlePrivateMessage(
            clientSocket,
            targetNickname,
            message);

        return;
    }

    sendError(
        clientSocket,
        QStringLiteral("无法识别的消息类型"));
}

void ChatServer::handleLogin(
    QTcpSocket *clientSocket,
    const QString &nickname)
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
        nickname.trimmed();

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

    clientInfo.nickname =
        trimmedNickname;

    clientInfo.loggedIn = true;

    qInfo() << "Client logged in:"
            << trimmedNickname
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
            .arg(trimmedNickname));

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

    const QJsonDocument document(
        messageObject);

    QByteArray data =
        document.toJson(
            QJsonDocument::Compact);

    data.append('\n');

    const qint64 bytesWritten =
        clientSocket->write(data);

    if (bytesWritten == -1)
    {
        qWarning() << "Failed to send data:"
                   << clientSocket->errorString();
    }
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
