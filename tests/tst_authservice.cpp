#include "../persistence/sqliterepository.h"
#include "../services/authservice.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtTest/QtTest>

class AuthServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void registersAndVerifiesPasswordHash();
    void rejectsDuplicateAndShortPassword();
};

namespace
{
QString tempDatabasePath(
    const QString &name)
{
    return QDir(
               QStandardPaths::writableLocation(
                   QStandardPaths::TempLocation))
        .filePath(
            QStringLiteral("%1_%2.sqlite")
                .arg(name)
                .arg(QDateTime::currentMSecsSinceEpoch()));
}

void removeDatabaseFiles(
    const QString &path)
{
    QFile::remove(path);
    QFile::remove(path + QStringLiteral("-wal"));
    QFile::remove(path + QStringLiteral("-shm"));
}
}

void AuthServiceTest::registersAndVerifiesPasswordHash()
{
    const QString databasePath =
        tempDatabasePath(QStringLiteral("auth_register"));

    SQLiteRepository repository;
    QVERIFY2(
        repository.open(databasePath, QStringLiteral("auth_register")),
        qPrintable(repository.lastError()));
    QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));

    AuthService authService(repository, 1000);
    const AuthService::Result registerResult =
        authService.registerUser(
            QStringLiteral("alice"),
            QStringLiteral("correct horse battery staple"));

    QVERIFY2(registerResult.success, qPrintable(registerResult.message));
    QVERIFY(registerResult.userId > 0);

    const SQLiteRepository::UserCredentials credentials =
        repository.credentialsForUser(QStringLiteral("alice"));

    QVERIFY(!credentials.passwordHash.contains(
        QStringLiteral("correct horse battery staple")));
    QVERIFY(credentials.passwordHash.startsWith(
        QStringLiteral("pbkdf2_sha256$")));
    QVERIFY(!credentials.passwordSalt.isEmpty());

    const AuthService::Result loginResult =
        authService.verifyLogin(
            QStringLiteral("alice"),
            QStringLiteral("correct horse battery staple"));
    QVERIFY2(loginResult.success, qPrintable(loginResult.message));
    QCOMPARE(loginResult.userId, registerResult.userId);

    const AuthService::Result badLoginResult =
        authService.verifyLogin(
            QStringLiteral("alice"),
            QStringLiteral("wrong password"));
    QVERIFY(!badLoginResult.success);

    repository.close();
    removeDatabaseFiles(databasePath);
}

void AuthServiceTest::rejectsDuplicateAndShortPassword()
{
    const QString databasePath =
        tempDatabasePath(QStringLiteral("auth_reject"));

    SQLiteRepository repository;
    QVERIFY2(
        repository.open(databasePath, QStringLiteral("auth_reject")),
        qPrintable(repository.lastError()));
    QVERIFY2(repository.initializeSchema(), qPrintable(repository.lastError()));

    AuthService authService(repository, 1000);

    const AuthService::Result shortResult =
        authService.registerUser(
            QStringLiteral("bob"),
            QStringLiteral("short"));
    QVERIFY(!shortResult.success);

    const AuthService::Result firstResult =
        authService.registerUser(
            QStringLiteral("bob"),
            QStringLiteral("long enough password"));
    QVERIFY(firstResult.success);

    const AuthService::Result duplicateResult =
        authService.registerUser(
            QStringLiteral("bob"),
            QStringLiteral("another long password"));
    QVERIFY(!duplicateResult.success);

    repository.close();
    removeDatabaseFiles(databasePath);
}

QTEST_MAIN(AuthServiceTest)

#include "tst_authservice.moc"
