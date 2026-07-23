#ifndef SQLITEREPOSITORY_H
#define SQLITEREPOSITORY_H

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

class SQLiteRepository
{
public:
    struct StoredMessage
    {
        qint64 id = 0;
        QString conversationId;
        QString senderUsername;
        QString kind;
        QString content;
        QDateTime createdAt;
        QString status;
    };

    SQLiteRepository();
    ~SQLiteRepository();

    bool open(
        const QString &databasePath,
        const QString &connectionName);

    void close();

    bool initializeSchema();

    bool createUser(
        const QString &username,
        const QString &passwordHash,
        const QString &passwordSalt);

    qint64 userId(
        const QString &username) const;

    bool createConversation(
        const QString &conversationId,
        const QString &type,
        const QString &title);

    bool appendMessage(
        const QString &conversationId,
        const QString &senderUsername,
        const QString &kind,
        const QString &content,
        const QString &status,
        const QDateTime &createdAt);

    QList<StoredMessage> messagesForConversation(
        const QString &conversationId,
        int limit) const;

    QString lastError() const;

private:
    bool execPragma(
        const QString &statement);

    bool execStatement(
        const QString &statement);

private:
    QString m_connectionName;
    QString m_lastError;
    QSqlDatabase m_database;
};

#endif // SQLITEREPOSITORY_H
