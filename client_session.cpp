#include "client_session.h"

#include <QTcpSocket>

ClientSession::ClientSession(
    QTcpSocket *socket)
    : socket(socket)
    , lastActivityUtc(QDateTime::currentDateTimeUtc())
{
}

void ClientSession::markActive()
{
    lastActivityUtc =
        QDateTime::currentDateTimeUtc();
}

bool ClientSession::timedOut(
    const QDateTime &nowUtc,
    int timeoutSeconds) const
{
    return lastActivityUtc.secsTo(nowUtc) > timeoutSeconds;
}

bool ClientSession::rememberRequestId(
    const QString &requestId,
    int maxTrackedIds)
{
    if (requestId.isEmpty())
    {
        return false;
    }

    if (m_processedRequestIds.contains(requestId))
    {
        return true;
    }

    if (m_processedRequestIds.size() > maxTrackedIds)
    {
        m_processedRequestIds.clear();
    }

    m_processedRequestIds.insert(requestId);
    return false;
}
