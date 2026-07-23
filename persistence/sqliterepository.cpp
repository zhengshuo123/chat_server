#include "sqliterepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

namespace
{
QString uniqueConnectionName()
{
    return QStringLiteral("chat_repository_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
}

SQLiteRepository::SQLiteRepository()
    : m_connectionName(uniqueConnectionName())
{
}

SQLiteRepository::~SQLiteRepository()
{
    close();
}

bool SQLiteRepository::open(
    const QString &databasePath,
    const QString &connectionName)
{
    close();

    m_connectionName =
        connectionName.isEmpty()
            ? uniqueConnectionName()
            : connectionName;

    m_database =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            m_connectionName);
    m_database.setDatabaseName(databasePath);

    if (!m_database.open())
    {
        m_lastError = m_database.lastError().text();
        return false;
    }

    if (!execPragma(QStringLiteral("PRAGMA foreign_keys = ON")))
    {
        return false;
    }

    if (!execPragma(QStringLiteral("PRAGMA journal_mode = WAL")))
    {
        return false;
    }

    if (!execPragma(QStringLiteral("PRAGMA busy_timeout = 5000")))
    {
        return false;
    }

    return true;
}

void SQLiteRepository::close()
{
    if (m_database.isValid())
    {
        const QString connectionName =
            m_database.connectionName();

        m_database.close();
        m_database = QSqlDatabase{};
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool SQLiteRepository::initializeSchema()
{
    if (!m_database.transaction())
    {
        m_lastError = m_database.lastError().text();
        return false;
    }

    const QStringList statements{
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "username TEXT NOT NULL UNIQUE,"
            "password_hash TEXT NOT NULL,"
            "password_salt TEXT NOT NULL,"
            "display_name TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "last_seen_at TEXT"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversations ("
            "id TEXT PRIMARY KEY,"
            "type TEXT NOT NULL CHECK(type IN ('group','direct')),"
            "title TEXT NOT NULL,"
            "created_at TEXT NOT NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation_members ("
            "conversation_id TEXT NOT NULL,"
            "user_id INTEGER NOT NULL,"
            "last_read_message_id INTEGER,"
            "PRIMARY KEY(conversation_id, user_id),"
            "FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE,"
            "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS messages ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "conversation_id TEXT NOT NULL,"
            "sender_user_id INTEGER,"
            "kind TEXT NOT NULL CHECK(kind IN ('text','system','image','file')),"
            "content TEXT NOT NULL,"
            "status TEXT NOT NULL,"
            "created_at TEXT NOT NULL,"
            "request_id TEXT,"
            "FOREIGN KEY(conversation_id) REFERENCES conversations(id) ON DELETE CASCADE,"
            "FOREIGN KEY(sender_user_id) REFERENCES users(id) ON DELETE SET NULL"
            ")"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS attachments ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "message_id INTEGER NOT NULL,"
            "file_name TEXT NOT NULL,"
            "mime_type TEXT NOT NULL,"
            "size_bytes INTEGER NOT NULL,"
            "storage_path TEXT NOT NULL,"
            "sha256 TEXT,"
            "FOREIGN KEY(message_id) REFERENCES messages(id) ON DELETE CASCADE"
            ")"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_messages_conversation_created "
            "ON messages(conversation_id, created_at, id)")};

    for (const QString &statement : statements)
    {
        if (!execStatement(statement))
        {
            m_database.rollback();
            return false;
        }
    }

    if (!m_database.commit())
    {
        m_lastError = m_database.lastError().text();
        return false;
    }

    return true;
}

bool SQLiteRepository::createUser(
    const QString &username,
    const QString &passwordHash,
    const QString &passwordSalt)
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral(
            "INSERT INTO users(username, password_hash, password_salt, display_name, created_at) "
            "VALUES(:username, :password_hash, :password_salt, :display_name, :created_at)"));
    query.bindValue(QStringLiteral(":username"), username);
    query.bindValue(QStringLiteral(":password_hash"), passwordHash);
    query.bindValue(QStringLiteral(":password_salt"), passwordSalt);
    query.bindValue(QStringLiteral(":display_name"), username);
    query.bindValue(
        QStringLiteral(":created_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

qint64 SQLiteRepository::userId(
    const QString &username) const
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral("SELECT id FROM users WHERE username = :username"));
    query.bindValue(QStringLiteral(":username"), username);

    if (!query.exec() || !query.next())
    {
        return 0;
    }

    return query.value(0).toLongLong();
}

bool SQLiteRepository::createConversation(
    const QString &conversationId,
    const QString &type,
    const QString &title)
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral(
            "INSERT OR IGNORE INTO conversations(id, type, title, created_at) "
            "VALUES(:id, :type, :title, :created_at)"));
    query.bindValue(QStringLiteral(":id"), conversationId);
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(
        QStringLiteral(":created_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool SQLiteRepository::appendMessage(
    const QString &conversationId,
    const QString &senderUsername,
    const QString &kind,
    const QString &content,
    const QString &status,
    const QDateTime &createdAt)
{
    const qint64 senderId =
        senderUsername.isEmpty()
            ? 0
            : userId(senderUsername);

    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral(
            "INSERT INTO messages(conversation_id, sender_user_id, kind, content, status, created_at) "
            "VALUES(:conversation_id, :sender_user_id, :kind, :content, :status, :created_at)"));
    query.bindValue(QStringLiteral(":conversation_id"), conversationId);
    query.bindValue(
        QStringLiteral(":sender_user_id"),
        senderId == 0 ? QVariant{} : QVariant{senderId});
    query.bindValue(QStringLiteral(":kind"), kind);
    query.bindValue(QStringLiteral(":content"), content);
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(
        QStringLiteral(":created_at"),
        createdAt.toUTC().toString(Qt::ISODateWithMs));

    if (!query.exec())
    {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

QList<SQLiteRepository::StoredMessage> SQLiteRepository::messagesForConversation(
    const QString &conversationId,
    int limit) const
{
    QSqlQuery query(m_database);
    query.prepare(
        QStringLiteral(
            "SELECT m.id, m.conversation_id, COALESCE(u.username, ''), m.kind, "
            "m.content, m.created_at, m.status "
            "FROM messages m "
            "LEFT JOIN users u ON u.id = m.sender_user_id "
            "WHERE m.conversation_id = :conversation_id "
            "ORDER BY m.id DESC "
            "LIMIT :limit"));
    query.bindValue(QStringLiteral(":conversation_id"), conversationId);
    query.bindValue(QStringLiteral(":limit"), qMax(1, limit));

    QList<StoredMessage> messages;

    if (!query.exec())
    {
        return messages;
    }

    while (query.next())
    {
        StoredMessage message;
        message.id = query.value(0).toLongLong();
        message.conversationId = query.value(1).toString();
        message.senderUsername = query.value(2).toString();
        message.kind = query.value(3).toString();
        message.content = query.value(4).toString();
        message.createdAt =
            QDateTime::fromString(
                query.value(5).toString(),
                Qt::ISODateWithMs);
        message.status = query.value(6).toString();
        messages.prepend(message);
    }

    return messages;
}

QString SQLiteRepository::lastError() const
{
    return m_lastError;
}

bool SQLiteRepository::execPragma(
    const QString &statement)
{
    return execStatement(statement);
}

bool SQLiteRepository::execStatement(
    const QString &statement)
{
    QSqlQuery query(m_database);

    if (!query.exec(statement))
    {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}
