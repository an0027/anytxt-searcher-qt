#ifndef CORE_EXCEPTIONS_H
#define CORE_EXCEPTIONS_H

#include <stdexcept>
#include <QString>
#include <xapian.h>

class IndexerException : public std::runtime_error {
public:
    explicit IndexerException(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class SearchException : public std::runtime_error {
public:
    explicit SearchException(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class ParserException : public std::runtime_error {
public:
    explicit ParserException(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class ConfigException : public std::runtime_error {
public:
    explicit ConfigException(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

// These inherit from std::runtime_error so they can be caught with std::exception
class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class IndexError : public std::runtime_error {
public:
    explicit IndexError(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class SearchError : public std::runtime_error {
public:
    explicit SearchError(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

class InvalidQueryError : public std::runtime_error {
public:
    explicit InvalidQueryError(const QString& msg)
        : std::runtime_error(msg.toStdString()) {}
};

#endif // CORE_EXCEPTIONS_H
