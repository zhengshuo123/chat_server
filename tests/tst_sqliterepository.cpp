#include "../persistence/sqliterepository.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtTest/QtTest>

class SQLiteRepositoryTest : public QObject
{
    Q_OBJECT

private slots:
    void persistsUsersConversationsAndMessages();
    void tracksReadState();
    void listsConversationsForMember();
    void persistsAttachmentMetadata();
};

void SQLiteRepositoryTest::persistsUsersConversationsAndMessages()
{
    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString databasePath =
        QDir(tempRoot).filePath(
            QStringLiteral("chat_repository_test_%1.sqlite")
                .arg(QDateTime::currentMSecsSinceEpoch()));

    {
        SQLiteRepository repository;
        QVERIFY2(
            repository.open(databasePath, QStringLiteral("repo_test_one")),
            qPrintable(repository.lastError()));
        QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));
        QVERIFY2(
            repository.createUser(
                QStringLiteral("alice"),
                QStringLiteral("hash"),
                QStringLiteral("salt")),
            qPrintable(repository.lastError()));
        QVERIFY(repository.userId(QStringLiteral("alice")) > 0);
        QVERIFY2(
            repository.createConversation(
                QStringLiteral("hall"),
                QStringLiteral("group"),
                QStringLiteral("公共聊天室")),
            qPrintable(repository.lastError()));
        QVERIFY2(
            repository.appendMessage(
                QStringLiteral("hall"),
                QStringLiteral("alice"),
                QStringLiteral("text"),
                QStringLiteral("hello"),
                QStringLiteral("sent"),
                QDateTime::currentDateTimeUtc()),
            qPrintable(repository.lastError()));

        const QList<SQLiteRepository::StoredMessage> messages =
            repository.messagesForConversation(QStringLiteral("hall"), 20);

        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.at(0).senderUsername, QStringLiteral("alice"));
        QCOMPARE(messages.at(0).content, QStringLiteral("hello"));
    }

    {
        SQLiteRepository repository;
        QVERIFY2(
            repository.open(databasePath, QStringLiteral("repo_test_two")),
            qPrintable(repository.lastError()));
        QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));
        QVERIFY(repository.userId(QStringLiteral("alice")) > 0);

        const QList<SQLiteRepository::StoredMessage> messages =
            repository.messagesForConversation(QStringLiteral("hall"), 20);

        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.at(0).content, QStringLiteral("hello"));
    }

    QFile::remove(databasePath);
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

void SQLiteRepositoryTest::tracksReadState()
{
    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString databasePath =
        QDir(tempRoot).filePath(
            QStringLiteral("chat_repository_read_state_%1.sqlite")
                .arg(QDateTime::currentMSecsSinceEpoch()));

    SQLiteRepository repository;
    QVERIFY2(
        repository.open(databasePath, QStringLiteral("repo_read_state")),
        qPrintable(repository.lastError()));
    QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createUser(
            QStringLiteral("alice"),
            QStringLiteral("hash"),
            QStringLiteral("salt")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createUser(
            QStringLiteral("bob"),
            QStringLiteral("hash"),
            QStringLiteral("salt")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createConversation(
            QStringLiteral("hall"),
            QStringLiteral("group"),
            QStringLiteral("公共聊天室")),
        qPrintable(repository.lastError()));

    QVERIFY2(
        repository.appendMessage(
            QStringLiteral("hall"),
            QStringLiteral("bob"),
            QStringLiteral("text"),
            QStringLiteral("one"),
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()),
        qPrintable(repository.lastError()));
    QCOMPARE(
        repository.unreadCount(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        1);

    QVERIFY2(
        repository.markConversationRead(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        qPrintable(repository.lastError()));
    QCOMPARE(
        repository.unreadCount(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        0);

    QVERIFY2(
        repository.appendMessage(
            QStringLiteral("hall"),
            QStringLiteral("alice"),
            QStringLiteral("text"),
            QStringLiteral("own message"),
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()),
        qPrintable(repository.lastError()));
    QCOMPARE(
        repository.unreadCount(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        0);

    QVERIFY2(
        repository.appendMessage(
            QStringLiteral("hall"),
            QStringLiteral("bob"),
            QStringLiteral("text"),
            QStringLiteral("two"),
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()),
        qPrintable(repository.lastError()));
    QCOMPARE(
        repository.unreadCount(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        1);

    QFile::remove(databasePath);
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

void SQLiteRepositoryTest::listsConversationsForMember()
{
    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString databasePath =
        QDir(tempRoot).filePath(
            QStringLiteral("chat_repository_conversation_list_%1.sqlite")
                .arg(QDateTime::currentMSecsSinceEpoch()));

    SQLiteRepository repository;
    QVERIFY2(
        repository.open(databasePath, QStringLiteral("repo_conversation_list")),
        qPrintable(repository.lastError()));
    QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createUser(
            QStringLiteral("alice"),
            QStringLiteral("hash"),
            QStringLiteral("salt")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createUser(
            QStringLiteral("bob"),
            QStringLiteral("hash"),
            QStringLiteral("salt")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createConversation(
            QStringLiteral("hall"),
            QStringLiteral("group"),
            QStringLiteral("公共聊天室")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createConversation(
            QStringLiteral("dm:alice:bob"),
            QStringLiteral("direct"),
            QStringLiteral("alice / bob")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.ensureConversationMember(
            QStringLiteral("hall"),
            QStringLiteral("alice")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.ensureConversationMember(
            QStringLiteral("dm:alice:bob"),
            QStringLiteral("alice")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.appendMessage(
            QStringLiteral("dm:alice:bob"),
            QStringLiteral("bob"),
            QStringLiteral("text"),
            QStringLiteral("hello"),
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc()),
        qPrintable(repository.lastError()));

    const QList<SQLiteRepository::StoredConversation> conversations =
        repository.conversationsForUser(QStringLiteral("alice"));
    QCOMPARE(conversations.size(), 2);
    QCOMPARE(conversations.at(0).id, QStringLiteral("hall"));
    QCOMPARE(conversations.at(1).id, QStringLiteral("dm:alice:bob"));
    QCOMPARE(conversations.at(1).unreadCount, 1);

    QFile::remove(databasePath);
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

void SQLiteRepositoryTest::persistsAttachmentMetadata()
{
    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString databasePath =
        QDir(tempRoot).filePath(
            QStringLiteral("chat_repository_attachment_%1.sqlite")
                .arg(QDateTime::currentMSecsSinceEpoch()));

    SQLiteRepository repository;
    QVERIFY2(
        repository.open(databasePath, QStringLiteral("repo_attachment")),
        qPrintable(repository.lastError()));
    QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createUser(
            QStringLiteral("alice"),
            QStringLiteral("hash"),
            QStringLiteral("salt")),
        qPrintable(repository.lastError()));
    QVERIFY2(
        repository.createConversation(
            QStringLiteral("hall"),
            QStringLiteral("group"),
            QStringLiteral("公共聊天室")),
        qPrintable(repository.lastError()));

    const qint64 messageId =
        repository.appendMessageReturningId(
            QStringLiteral("hall"),
            QStringLiteral("alice"),
            QStringLiteral("file"),
            QStringLiteral("notes.txt"),
            QStringLiteral("sent"),
            QDateTime::currentDateTimeUtc());
    QVERIFY(messageId > 0);
    QVERIFY2(
        repository.addAttachment(
            messageId,
            QStringLiteral("notes.txt"),
            QStringLiteral("text/plain"),
            12,
            QStringLiteral("C:/tmp/notes.txt"),
            QStringLiteral("abc")),
        qPrintable(repository.lastError()));

    const QList<SQLiteRepository::StoredMessage> messages =
        repository.messagesForConversation(QStringLiteral("hall"), 20);
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.at(0).attachmentFileName, QStringLiteral("notes.txt"));
    QCOMPARE(messages.at(0).attachmentSizeBytes, 12);
    QVERIFY(messages.at(0).attachmentId > 0);

    const SQLiteRepository::StoredAttachment attachment =
        repository.attachment(messages.at(0).attachmentId);
    QCOMPARE(attachment.messageId, messageId);
    QCOMPARE(attachment.fileName, QStringLiteral("notes.txt"));
    QCOMPARE(attachment.storagePath, QStringLiteral("C:/tmp/notes.txt"));

    QFile::remove(databasePath);
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

QTEST_MAIN(SQLiteRepositoryTest)

#include "tst_sqliterepository.moc"
