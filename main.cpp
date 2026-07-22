#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTcpServer server;

    constexpr quint16 port = 8888;

    if (!server.listen(QHostAddress::AnyIPv4, port))
    {
        qCritical() << "Server listen failed:"
                    << server.errorString();

        return 1;
    }

    qInfo() << "Server is listening on"
            << server.serverAddress().toString()
            << ":"
            << server.serverPort();

    QObject::connect(
        &server,
        &QTcpServer::newConnection,
        &server,
        [&server]()
        {
            while (server.hasPendingConnections())
            {
                QTcpSocket *clientSocket =
                    server.nextPendingConnection();

                if (clientSocket == nullptr)
                {
                    continue;
                }

                qInfo() << "New client connected:"
                        << clientSocket->peerAddress().toString()
                        << ":"
                        << clientSocket->peerPort();

                QObject::connect(
                    clientSocket,
                    &QTcpSocket::readyRead,
                    clientSocket,
                    [clientSocket]()
                    {
                        const QByteArray receivedData =
                            clientSocket->readAll();

                        qInfo() << "Received message:"
                                << receivedData;

                        const QByteArray reply =
                            "hello from server";

                        const qint64 bytesWritten =
                            clientSocket->write(reply);

                        if (bytesWritten == -1)
                        {
                            qCritical() << "Failed to send reply:"
                                        << clientSocket->errorString();

                            return;
                        }

                        qInfo() << "Reply sent:"
                                << reply;
                    });

                QObject::connect(
                    clientSocket,
                    &QTcpSocket::disconnected,
                    clientSocket,
                    [clientSocket]()
                    {
                        qInfo() << "Client disconnected:"
                                << clientSocket->peerAddress().toString()
                                << ":"
                                << clientSocket->peerPort();

                        clientSocket->deleteLater();
                    });
            }
        });

    return app.exec();
}
