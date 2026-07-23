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

QTEST_MAIN(SQLiteRepositoryTest)

#include "tst_sqliterepository.moc"
