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
        qint64 attachmentId = 0;
        QString attachmentFileName;
        qint64 attachmentSizeBytes = 0;
    };

    struct StoredConversation
    {
        QString id;
        QString type;
        QString title;
        int unreadCount = 0;
    };

    struct UserCredentials
    {
        qint64 id = 0;
        QString username;
        QString passwordHash;
        QString passwordSalt;
    };

    struct StoredAttachment
    {
        qint64 id = 0;
        qint64 messageId = 0;
        QString fileName;
        QString mimeType;
        qint64 sizeBytes = 0;
        QString storagePath;
        QString sha256;
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

    UserCredentials credentialsForUser(
        const QString &username) const;

    bool createConversation(
        const QString &conversationId,
        const QString &type,
        const QString &title);

    bool ensureConversationMember(
        const QString &conversationId,
        const QString &username);

    QList<StoredConversation> conversationsForUser(
        const QString &username) const;

    bool appendMessage(
        const QString &conversationId,
        const QString &senderUsername,
        const QString &kind,
        const QString &content,
        const QString &status,
        const QDateTime &createdAt);

    qint64 appendMessageReturningId(
        const QString &conversationId,
        const QString &senderUsername,
        const QString &kind,
        const QString &content,
        const QString &status,
        const QDateTime &createdAt);

    bool addAttachment(
        qint64 messageId,
        const QString &fileName,
        const QString &mimeType,
        qint64 sizeBytes,
        const QString &storagePath,
        const QString &sha256);

    StoredAttachment attachment(
        qint64 attachmentId) const;

    QList<StoredMessage> messagesForConversation(
        const QString &conversationId,
        int limit) const;

    bool markConversationRead(
        const QString &conversationId,
        const QString &username);

    int unreadCount(
        const QString &conversationId,
        const QString &username) const;

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
