#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include "../persistence/sqliterepository.h"

#include <QString>

class AuthService
{
public:
    struct Result
    {
        bool success = false;
        QString message;
        qint64 userId = 0;
    };

    explicit AuthService(
        SQLiteRepository &repository,
        int iterations = 210000);

    Result registerUser(
        const QString &username,
        const QString &password);

    Result verifyLogin(
        const QString &username,
        const QString &password) const;

    static QString hashPassword(
        const QString &password,
        const QByteArray &salt,
        int iterations);

private:
    static QByteArray pbkdf2HmacSha256(
        const QByteArray &password,
        const QByteArray &salt,
        int iterations,
        qsizetype outputBytes);

    static QByteArray secureRandomBytes(
        qsizetype size);

    static bool constantTimeEquals(
        const QByteArray &left,
        const QByteArray &right);

    static bool parseHash(
        const QString &encodedHash,
        int *iterations,
        QByteArray *hashOutput);

private:
    SQLiteRepository &m_repository;
    int m_iterations;
};

#endif // AUTHSERVICE_H
