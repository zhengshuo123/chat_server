#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#include <QByteArray>
#include <QDateTime>
#include <QSet>
#include <QString>

class QTcpSocket;

class ClientSession
{
public:
    explicit ClientSession(
        QTcpSocket *socket = nullptr);

    void markActive();

    bool timedOut(
        const QDateTime &nowUtc,
        int timeoutSeconds) const;

    bool rememberRequestId(
        const QString &requestId,
        int maxTrackedIds);

    QTcpSocket *socket = nullptr;
    QString nickname;
    bool loggedIn = false;
    QByteArray inputBuffer;
    QDateTime lastActivityUtc;

private:
    QSet<QString> m_processedRequestIds;
};

#endif // CLIENT_SESSION_H
