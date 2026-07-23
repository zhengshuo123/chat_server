#include "chatserver.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    constexpr quint16 port = 8888;

    ChatServer server(port);

    if (!server.start())
    {
        return 1;
    }

    return app.exec();
}
